// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/reporter_runner_win.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/typed_macros.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_dialog_controller_impl_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_fetcher_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/reporter_histogram_recorder.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_client_info_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/pref_names.h"
#include "components/component_updater/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"

// Needed for QueryUnbiasedInterruptTime and other Windows functions.
#include <windows.h>

using content::BrowserThread;

namespace safe_browsing {

namespace {

internal::SwReporterTestingDelegate* g_testing_delegate_ = nullptr;

ChromeCleanerController* GetCleanerController() {
  return g_testing_delegate_ ? g_testing_delegate_->GetCleanerController()
                             : ChromeCleanerController::GetInstance();
}

SwReporterInvocationResult ExitCodeToInvocationResult(int exit_code) {
  switch (exit_code) {
    case chrome_cleaner::kSwReporterCleanupNeeded:
      // This should only be used if a cleanup will not be offered. If a
      // cleanup is offered, the controller will get notified with a
      // kCleanupToBeOffered signal, and this one will be ignored.
      // Do not accept reboot required or post-reboot exit codes, since they
      // should not be sent out by the reporter, and should be treated as
      // errors.
      return SwReporterInvocationResult::kCleanupNotOffered;

    case chrome_cleaner::kSwReporterNothingFound:
    case chrome_cleaner::kSwReporterNonRemovableOnly:
    case chrome_cleaner::kSwReporterSuspiciousOnly:
      return SwReporterInvocationResult::kNothingFound;

    case chrome_cleaner::kSwReporterTimeoutWithUwS:
    case chrome_cleaner::kSwReporterTimeoutWithoutUwS:
      return SwReporterInvocationResult::kTimedOut;
  }

  return SwReporterInvocationResult::kGeneralFailure;
}

void CreateChromeCleanerDialogController() {
  if (g_testing_delegate_) {
    g_testing_delegate_->CreateChromeCleanerDialogController();
    return;
  }

  // The dialog controller manages its own lifetime. If the controller enters
  // the kInfected state, the dialog controller will show the chrome cleaner
  // dialog to the user.
  new ChromeCleanerDialogControllerImpl(GetCleanerController());
}

bool ShouldShowPromptForPeriodicRun() {
  Browser* browser = chrome::FindLastActive();
  if (!browser) {
    return false;
  }

  Profile* profile = browser->profile();
  DCHECK(profile);
  PrefService* prefs = profile->GetPrefs();
  DCHECK(prefs);

  // Don't show the prompt again if it's been shown before for this profile and
  // for the current variations seed. The seed preference will be updated once
  // the prompt is shown.
  const std::string incoming_seed =
      GetCleanerController()->GetIncomingPromptSeed();
  const std::string old_seed = prefs->GetString(prefs::kSwReporterPromptSeed);
  if (!incoming_seed.empty() && incoming_seed == old_seed) {
    RecordPromptNotShownWithReasonHistogram(NO_PROMPT_REASON_ALREADY_PROMPTED);
    return false;
  }

  return true;
}

base::Time Now() {
  return g_testing_delegate_ ? g_testing_delegate_->Now() : base::Time::Now();
}

}  // namespace

namespace internal {

// This class tries to run a queue of reporters and react to their exit codes.
// It schedules subsequent runs of the queue as needed, or retries as soon as a
// browser is available when none is on first try.
//
// This can't be in the anonymous namespace because it's a friend of
// ChromeMetricsServiceAccessor.
class ReporterRunner {
 public:
  // Tries to run |invocations| immediately. This must be called on the UI
  // thread.
  static void MaybeStartInvocations(
      SwReporterInvocationType invocation_type,
      SwReporterInvocationSequence&& invocations) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK_NE(SwReporterInvocationType::kUnspecified, invocation_type);

    // Ensures that any component waiting for the reporter sequence result will
    // be notified if |invocations| doesn't get scheduled.
    base::ScopedClosureRunner scoped_runner(
        base::BindOnce(&ChromeCleanerController::OnReporterSequenceDone,
                       base::Unretained(GetCleanerController()),
                       SwReporterInvocationResult::kNotScheduled));

    PrefService* local_state = g_browser_process->local_state();
    if (!invocations.version().IsValid() || !local_state)
      return;

    // By this point the reporter should be allowed to run.
    DCHECK(SwReporterIsAllowedByPolicy());

    ReporterRunTimeInfo time_info(local_state);

    // The UI should block a new user-initiated run if the reporter is
    // currently running. New periodic runs can be simply ignored.
    if (instance_)
      return;

    // A periodic run will only start if no run is currently happening and
    // if no sequence have been triggered in the last
    // |kDaysBetweenSuccessfulSwReporterRuns| days.
    if (!IsUserInitiated(invocation_type) && !time_info.ShouldRun())
      return;

    // The reporter runner object manages its own lifetime and will delete
    // itself once all invocations finish.
    instance_ = new ReporterRunner(invocation_type, std::move(invocations),
                                   std::move(time_info));
    GetCleanerController()->OnReporterSequenceStarted();
    instance_->PostNextInvocation();

    // The reporter sequence has been scheduled to run, so don't notify that
    // it has not been scheduled.
    std::ignore = scoped_runner.Release();
  }

 private:
  // Keeps track of last and upcoming reporter runs and logs uploading.
  //
  // Periodic runs are allowed to start if the last time the reporter ran was
  // more than |kDaysBetweenSuccessfulSwReporterRuns| days ago. As a safety
  // measure for failure recovery, also allow to run if
  // |prefs::kSwReporterLastTimeTriggered| is set in the future. This is to
  // prevent from no longer running the reporter if a time in the future is
  // accidentally written.
  //
  // Logs uploading is allowed if logs have never been uploaded for this user,
  // or if logs have been sent at least |kSwReporterLastTimeSentReport| days
  // ago. As a safety measure for failure recovery, also allow sending logs if
  // the last upload time in local state is incorrectly set to the future.
  class ReporterRunTimeInfo {
   public:
    explicit ReporterRunTimeInfo(PrefService* local_state) {
      DCHECK(local_state);

      base::Time now = Now();
      if (local_state->HasPrefPath(prefs::kSwReporterLastTimeTriggered)) {
        base::Time last_time_triggered =
            base::Time() + base::Microseconds(local_state->GetInt64(
                               prefs::kSwReporterLastTimeTriggered));
        base::Time next_trigger =
            last_time_triggered +
            base::Days(kDaysBetweenSuccessfulSwReporterRuns);
        should_run_ = next_trigger <= now || last_time_triggered > now;
      } else {
        should_run_ = true;
      }

      if (local_state->HasPrefPath(prefs::kSwReporterLastTimeSentReport)) {
        base::Time last_time_sent_logs =
            base::Time() + base::Microseconds(local_state->GetInt64(
                               prefs::kSwReporterLastTimeSentReport));
        base::Time next_time_send_logs =
            last_time_sent_logs + base::Days(kDaysBetweenReporterLogsSent);
        in_logs_upload_period_ =
            next_time_send_logs <= now || last_time_sent_logs > now;
      } else {
        in_logs_upload_period_ = true;
      }
    }

    bool ShouldRun() const { return should_run_; }

    bool InLogsUploadPeriod() const { return in_logs_upload_period_; }

    bool should_run_ = false;
    bool in_logs_upload_period_ = false;
  };

  ReporterRunner(SwReporterInvocationType invocation_type,
                 SwReporterInvocationSequence&& invocations,
                 ReporterRunTimeInfo&& time_info)
      : invocation_type_(invocation_type),
        invocations_(std::move(invocations)),
        on_sequence_done_(
            base::BindOnce(&ChromeCleanerController::OnReporterSequenceDone,
                           base::Unretained(GetCleanerController()))),
        time_info_(std::move(time_info)) {}

  ReporterRunner(const ReporterRunner&) = delete;
  ReporterRunner& operator=(const ReporterRunner&) = delete;

  ~ReporterRunner() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_EQ(instance_, this);

    instance_ = nullptr;
  }

  // Launches the command line at the head of the queue.
  void PostNextInvocation() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_EQ(instance_, this);

    DCHECK(!invocations_.container().empty());
    SwReporterInvocation next_invocation = invocations_.container().front();
    invocations_.mutable_container().pop();

    AppendInvocationSpecificSwitches(&next_invocation);

    base::TaskRunner* task_runner =
        g_testing_delegate_ ? g_testing_delegate_->BlockingTaskRunner()
                            : blocking_task_runner_.get();

    auto launch_and_wait =
        base::BindOnce(&LaunchAndWaitForExit, next_invocation);
    // Unretained is safe because ReporterRunner deletes itself after all
    // invocations are finished.
    auto reporter_done = base::BindOnce(
        &ReporterRunner::ReporterDone, base::Unretained(this), next_invocation);
    task_runner->PostTaskAndReplyWithResult(
        FROM_HERE, std::move(launch_and_wait), std::move(reporter_done));
  }

  // This method is called on the UI thread when an invocation of the reporter
  // has completed. This is run as a task posted from an interruptible worker
  // thread so should be resilient to unexpected shutdown.
  void ReporterDone(SwReporterInvocation finished_invocation,
                    ReporterRunResult result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_EQ(instance_, this);

    // Ensures finalization if there are no further invocations to run. This
    // scoped runner may be released later on if there are other invocations to
    // start.
    base::ScopedClosureRunner scoped_runner(base::BindOnce(
        &ReporterRunner::SendResultAndDeleteSelf, base::Unretained(this),
        ExitCodeToInvocationResult(result.exit_code)));

    ReporterHistogramRecorder uma(finished_invocation.suffix());

    // Don't continue the current queue of reporters if one failed to launch.
    // If the reporter failed to launch, do not process the results.
    if (result.exit_code == kReporterNotLaunchedExitCode) {
      uma.RecordExitCode(result.exit_code);
      NotifySequenceDone(SwReporterInvocationResult::kProcessFailedToLaunch);
      return;
    }

    // Tries to run the next invocation in the queue.
    if (!invocations_.container().empty()) {
      // If there are other invocations to start, then we shouldn't finalize
      // this object. ScopedClosureRunner::Release requires its return value to
      // be used, so simply std::ignore it, since it will not be needed.
      std::ignore = scoped_runner.Release();
      PostNextInvocation();
    }

    uma.RecordVersion(invocations_.version());
    uma.RecordExitCode(result.exit_code);
    uma.RecordEngineErrorCode();
    uma.RecordFoundUwS();

    PrefService* local_state = g_browser_process->local_state();
    if (local_state) {
      if (finished_invocation.BehaviourIsSupported(
              SwReporterInvocation::BEHAVIOUR_LOG_EXIT_CODE_TO_PREFS)) {
        local_state->SetInteger(prefs::kSwReporterLastExitCode,
                                result.exit_code);
      }
      const base::Time now = Now();
      local_state->SetInt64(prefs::kSwReporterLastTimeTriggered,
                            now.ToInternalValue());
    }
    uma.RecordRuntime(result.running_time, result.running_time_without_sleep);
    uma.RecordMemoryUsage();
    if (finished_invocation.reporter_logs_upload_enabled())
      uma.RecordLogsUploadResult();

    if (!finished_invocation.BehaviourIsSupported(
            SwReporterInvocation::BEHAVIOUR_TRIGGER_PROMPT)) {
      RecordPromptNotShownWithReasonHistogram(
          NO_PROMPT_REASON_BEHAVIOUR_NOT_SUPPORTED);
      return;
    }

    // Do not accept reboot required or post-reboot exit codes, since they
    // should not be sent out by the reporter.
    if (result.exit_code != chrome_cleaner::kSwReporterCleanupNeeded) {
      RecordPromptNotShownWithReasonHistogram(NO_PROMPT_REASON_NOTHING_FOUND);
      return;
    }

    ChromeCleanerController* cleaner_controller = GetCleanerController();

    // The most common state for the controller at this moment is
    // kReporterRunning, set before this invocation sequence started. However,
    // it's possible that the reporter starts running again during the prompt
    // routine (for example, a new reporter version became available while the
    // cleaner prompt is presented to the user). In that case, only prompt if
    // the controller has returned to the idle state.
    if (cleaner_controller->state() != ChromeCleanerController::State::kIdle &&
        cleaner_controller->state() !=
            ChromeCleanerController::State::kReporterRunning) {
      RecordPromptNotShownWithReasonHistogram(
          NO_PROMPT_REASON_NOT_ON_IDLE_STATE);
      return;
    }

    if (!IsUserInitiated(invocation_type_) &&
        !ShouldShowPromptForPeriodicRun()) {
      return;
    }

    finished_invocation.set_cleaner_logs_upload_enabled(
        invocation_type_ ==
        SwReporterInvocationType::kUserInitiatedWithLogsAllowed);

    finished_invocation.set_chrome_prompt(
        IsUserInitiated(invocation_type_)
            ? chrome_cleaner::ChromePromptValue::kUserInitiated
            : chrome_cleaner::ChromePromptValue::kPrompted);

    NotifySequenceDone(SwReporterInvocationResult::kCleanupToBeOffered);
    cleaner_controller->Scan(finished_invocation);

    // If this is a periodic reporter run, then create the dialog controller, so
    // that the user may eventually be prompted. Otherwise, all interaction
    // should be driven from the Settings page.
    if (!IsUserInitiated(invocation_type_))
      CreateChromeCleanerDialogController();
  }

  // Returns true if the experiment to send reporter logs is enabled, the user
  // opted into Safe Browsing extended reporting, and this queue of invocations
  // started during the logs upload interval.
  bool ShouldSendReporterLogs(const std::string& suffix) {
    ReporterHistogramRecorder uma(suffix);

    Browser* browser = chrome::FindLastActive();
    if (!browser) {
      return false;
    }

    Profile* profile = browser->profile();
    DCHECK(profile);
    PrefService* prefs = profile->GetPrefs();
    DCHECK(prefs);

    // The enterprise policy overrides all other choices.
    if (!SwReporterReportingIsAllowedByPolicy(profile)) {
      uma.RecordLogsUploadEnabled(
          SwReporterLogsUploadsEnabled::DISABLED_BY_POLICY);
      return false;
    }

    switch (invocation_type_) {
      case SwReporterInvocationType::kUnspecified:
      case SwReporterInvocationType::kMax:
        NOTREACHED();
        return false;

      case SwReporterInvocationType::kUserInitiatedWithLogsDisallowed:
        uma.RecordLogsUploadEnabled(
            SwReporterLogsUploadsEnabled::DISABLED_BY_USER);
        return false;

      case SwReporterInvocationType::kUserInitiatedWithLogsAllowed:
        uma.RecordLogsUploadEnabled(
            SwReporterLogsUploadsEnabled::ENABLED_BY_USER);
        return true;

      case SwReporterInvocationType::kPeriodicRun:
        if (!SafeBrowsingExtendedReportingEnabled()) {
          uma.RecordLogsUploadEnabled(
              SwReporterLogsUploadsEnabled::SBER_DISABLED);
          return false;
        }

        if (!time_info_.InLogsUploadPeriod()) {
          uma.RecordLogsUploadEnabled(
              SwReporterLogsUploadsEnabled::RECENTLY_SENT_LOGS);
          return false;
        }

        uma.RecordLogsUploadEnabled(SwReporterLogsUploadsEnabled::SBER_ENABLED);
        return true;
    }

    NOTREACHED();
    return false;
  }

  // Appends switches to the next invocation that depend on the user current
  // state with respect to opting into extended Safe Browsing reporting and
  // metrics and crash reporting.
  void AppendInvocationSpecificSwitches(SwReporterInvocation* invocation) {
    // Add switches for users who opted into extended Safe Browsing reporting.
    PrefService* local_state = g_browser_process->local_state();
    if (local_state && ShouldSendReporterLogs(invocation->suffix())) {
      invocation->set_reporter_logs_upload_enabled(true);
      AddSwitchesForExtendedReportingUser(invocation);
      // Set the local state value before the first attempt to run the
      // reporter, because we only want to upload logs once in the window
      // defined by |kDaysBetweenReporterLogsSent|. If we set with other local
      // state values after the reporter runs, we could send logs again too
      // quickly (for example, if Chrome stops before the reporter finishes).
      local_state->SetInt64(prefs::kSwReporterLastTimeSentReport,
                            Now().ToInternalValue());
    } else if (invocation->reporter_logs_upload_enabled()) {
      // Security measure in case this has been changed to true somewhere else,
      // so that reporter logs are not accidentally uploaded without user
      // consent.
      NOTREACHED();
      invocation->set_reporter_logs_upload_enabled(false);
    }

    if (ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled()) {
      invocation->mutable_command_line().AppendSwitch(
          chrome_cleaner::kEnableCrashReportingSwitch);
    }
  }

  // Adds switches to be sent to the Software Reporter when the user opted into
  // extended Safe Browsing reporting and is not incognito.
  void AddSwitchesForExtendedReportingUser(SwReporterInvocation* invocation) {
    invocation->mutable_command_line().AppendSwitch(
        chrome_cleaner::kExtendedSafeBrowsingEnabledSwitch);
    invocation->mutable_command_line().AppendSwitchASCII(
        chrome_cleaner::kChromeVersionSwitch, version_info::GetVersionNumber());
    invocation->mutable_command_line().AppendSwitchNative(
        chrome_cleaner::kChromeChannelSwitch,
        base::NumberToWString(ChannelAsInt()));
  }

  void SendResultAndDeleteSelf(SwReporterInvocationResult result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_EQ(instance_, this);

    NotifySequenceDone(result);

    delete this;
  }

  void NotifySequenceDone(SwReporterInvocationResult result) {
    if (on_sequence_done_)
      std::move(on_sequence_done_).Run(result);
  }

  SwReporterInvocationType invocation_type() const { return invocation_type_; }

  // Not null if there is a sequence currently running.
  static ReporterRunner* instance_;

  scoped_refptr<base::TaskRunner> blocking_task_runner_ =
      base::ThreadPool::CreateTaskRunner(
          // LaunchAndWaitForExit creates (MayBlock()) and joins
          // (WithBaseSyncPrimitives()) a process.
          {base::MayBlock(), base::WithBaseSyncPrimitives(),
           base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  SwReporterInvocationType invocation_type_;

  // The queue of invocations that are currently running.
  SwReporterInvocationSequence invocations_;

  // Invoked once when the |invocations_| sequence run finishes or when it's
  // aborted with an error, whichever comes first.
  base::OnceCallback<void(SwReporterInvocationResult result)> on_sequence_done_;

  // Last and upcoming reporter runs and logs uploading.
  ReporterRunTimeInfo time_info_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// static
ReporterRunner* ReporterRunner::instance_ = nullptr;

void SetSwReporterTestingDelegate(SwReporterTestingDelegate* delegate) {
  g_testing_delegate_ = delegate;
}

bool ReporterTerminatesOnBrowserExit() {
  // Windows 7 does not allow nested job objects, and the process may already
  // be in a job (for example when running under a debugger or in Terminal
  // Server) so only enable this on Windows 8+. The reporter will finish its
  // scan and upload reports if the user has opted in, but not be able to
  // prompt for cleanup if UwS is found.
  return base::win::GetVersion() >= base::win::Version::WIN8;
}

// This function is called from a worker thread to launch the SwReporter and
// wait for termination to collect its exit code. This task could be
// interrupted by a shutdown at any time, so it shouldn't depend on anything
// external that could be shut down beforehand.
ReporterRunResult LaunchAndWaitForExit(const SwReporterInvocation& invocation) {
  TRACE_EVENT("safe_browsing", "ReporterRunner::LaunchAndWaitForExit");

  // This exit code is used to identify that a reporter run didn't happen, so
  // the result should be ignored and a rerun scheduled for the usual delay.
  ReporterRunResult result{.exit_code = kReporterNotLaunchedExitCode};

  ReporterHistogramRecorder uma(invocation.suffix());

  base::FilePath tmpdir;
  if (!base::GetTempDir(&tmpdir)) {
    return result;
  }

  // The reporter runs from the system tmp directory. This is to avoid
  // unnecessarily holding on to the installation directory while running as it
  // prevents uninstallation of chrome.
  base::LaunchOptions launch_options;
  launch_options.current_directory = tmpdir;

  // Assign the reporter process to a job. If the browser exits before the
  // reporter, the OS will close the job handle and the reporter process.
  base::win::ScopedHandle job;
  if (ReporterTerminatesOnBrowserExit()) {
    job.Set(::CreateJobObject(nullptr, nullptr));
    if (job.IsValid()) {
      base::SetJobObjectLimitFlags(job.Get(),
                                   JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE);
      launch_options.job_handle = job.Get();
    }
  }

  base::Time start_time = Now();

  // QueryUnbiasedInterruptTime does not include time spent in sleep or
  // hibernation.
  ULONGLONG start_time_without_sleep;
  ::QueryUnbiasedInterruptTime(&start_time_without_sleep);

  base::Process reporter_process =
      g_testing_delegate_
          ? g_testing_delegate_->LaunchReporterProcess(invocation,
                                                       launch_options)
          : base::LaunchProcess(invocation.command_line(), launch_options);

  if (!reporter_process.IsValid()) {
    return result;
  }

  constexpr base::TimeDelta kPollingTime = base::Seconds(10);
  bool exited = false;
  while (!exited) {
    TRACE_EVENT("safe_browsing", "ReporterRunning");
    if (g_testing_delegate_) {
      exited = g_testing_delegate_->WaitForReporterExit(
          reporter_process, kPollingTime, &result.exit_code);
    } else {
      exited = reporter_process.WaitForExitWithTimeout(kPollingTime,
                                                       &result.exit_code);
      // Wait should only fail on a timeout.
      DCHECK(exited || reporter_process.IsRunning());
    }
  }

  result.running_time = Now() - start_time;

  ULONGLONG now_without_sleep;
  ::QueryUnbiasedInterruptTime(&now_without_sleep);

  // QueryUnbiasedInterruptTime returns units of 100 nanoseconds. See
  // https://docs.microsoft.com/en-us/windows/win32/api/realtimeapiset/nf-realtimeapiset-queryunbiasedinterrupttime
  result.running_time_without_sleep =
      base::Nanoseconds(100 * (now_without_sleep - start_time_without_sleep));

  // After the reporter process has exited the job object is no longer needed.
  // It will be closed when it goes out of scope here.
  return result;
}

}  // namespace internal

bool IsUserInitiated(SwReporterInvocationType invocation_type) {
  return invocation_type ==
             SwReporterInvocationType::kUserInitiatedWithLogsAllowed ||
         invocation_type ==
             SwReporterInvocationType::kUserInitiatedWithLogsDisallowed;
}

void MaybeStartSwReporter(SwReporterInvocationType invocation_type,
                          SwReporterInvocationSequence&& invocations) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!invocations.container().empty());

  internal::ReporterRunner::MaybeStartInvocations(invocation_type,
                                                  std::move(invocations));
}

bool SwReporterIsAllowedByPolicy() {
  static auto is_allowed = []() {
    PrefService* local_state = g_browser_process->local_state();
    return !local_state ||
           !local_state->IsManagedPreference(prefs::kSwReporterEnabled) ||
           local_state->GetBoolean(prefs::kSwReporterEnabled);
  }();
  return is_allowed;
}

bool SwReporterReportingIsAllowedByPolicy(Profile* profile) {
  // Reporting is allowed when cleanup is enabled by policy and when the
  // specific reporting policy is allowed.  While the former policy is not
  // dynamic, the latter one is.
  bool is_allowed = SwReporterIsAllowedByPolicy();
  if (is_allowed) {
    PrefService* profile_prefs = profile->GetPrefs();
    DCHECK(profile_prefs);

    is_allowed = !profile_prefs ||
                 !profile_prefs->IsManagedPreference(
                     prefs::kSwReporterReportingEnabled) ||
                 profile_prefs->GetBoolean(prefs::kSwReporterReportingEnabled);
  }
  return is_allowed;
}

}  // namespace safe_browsing
