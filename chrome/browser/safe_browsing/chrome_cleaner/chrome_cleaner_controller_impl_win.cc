// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_impl_win.h"

#include <windows.h>

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/sw_reporter_installer_win.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_fetcher_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_navigation_util_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_reboot_dialog_controller_impl_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_runner_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/reporter_runner_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/settings_resetter_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_client_info_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/installer/util/scoped_token_privilege.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "components/chrome_cleaner/public/proto/chrome_prompt.pb.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "net/http/http_status_code.h"
#include "ui/base/window_open_disposition.h"

namespace safe_browsing {

namespace {

using ::content::BrowserThread;
using PromptUserResponse = chrome_cleaner::PromptUserResponse;

// The global singleton instance. Exposed outside of GetInstance() so that it
// can be reset by tests.
ChromeCleanerControllerImpl* g_controller = nullptr;

// TODO(alito): Move these shared exit codes to the chrome_cleaner component.
// https://crbug.com/727956
constexpr int kRebootRequiredExitCode = 15;
constexpr int kRebootNotRequiredExitCode = 0;

// These values are used to send UMA information and are replicated in the
// enums.xml file, so the order MUST NOT CHANGE.
enum CleanupResultHistogramValue {
  CLEANUP_RESULT_SUCCEEDED = 0,
  CLEANUP_RESULT_REBOOT_REQUIRED = 1,
  CLEANUP_RESULT_FAILED = 2,

  CLEANUP_RESULT_MAX,
};

// These values are used to send UMA information and are replicated in the
// enums.xml file, so the order MUST NOT CHANGE.
enum IPCDisconnectedHistogramValue {
  IPC_DISCONNECTED_SUCCESS = 0,
  IPC_DISCONNECTED_LOST_WHILE_SCANNING = 1,
  IPC_DISCONNECTED_LOST_USER_PROMPTED = 2,

  IPC_DISCONNECTED_MAX,
};

// Attempts to change the Chrome Cleaner binary's suffix to ".exe". Will return
// an empty FilePath on failure. Should be called on a sequence with traits
// appropriate for IO operations.
base::FilePath VerifyAndRenameDownloadedCleaner(
    base::FilePath downloaded_path,
    ChromeCleanerFetchStatus fetch_status) {
  if (downloaded_path.empty() || !base::PathExists(downloaded_path))
    return base::FilePath();

  if (fetch_status != ChromeCleanerFetchStatus::kSuccess) {
    base::DeleteFile(downloaded_path, /*recursive=*/false);
    return base::FilePath();
  }

  base::FilePath executable_path(
      downloaded_path.ReplaceExtension(FILE_PATH_LITERAL("exe")));

  if (!base::ReplaceFile(downloaded_path, executable_path, nullptr)) {
    base::DeleteFile(downloaded_path, /*recursive=*/false);
    return base::FilePath();
  }

  return executable_path;
}

void OnChromeCleanerFetched(
    ChromeCleanerControllerDelegate::FetchedCallback fetched_callback,
    base::FilePath downloaded_path,
    ChromeCleanerFetchStatus fetch_status) {
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(VerifyAndRenameDownloadedCleaner, downloaded_path,
                     fetch_status),
      std::move(fetched_callback));
}

ChromeCleanerController::IdleReason IdleReasonWhenConnectionClosedTooSoon(
    ChromeCleanerController::State current_state) {
  DCHECK(current_state == ChromeCleanerController::State::kScanning ||
         current_state == ChromeCleanerController::State::kInfected);

  return current_state == ChromeCleanerController::State::kScanning
             ? ChromeCleanerController::IdleReason::kScanningFailed
             : ChromeCleanerController::IdleReason::kConnectionLost;
}

void RecordScannerLogsAcceptanceHistogram(bool logs_accepted) {
  UMA_HISTOGRAM_BOOLEAN("SoftwareReporter.ScannerLogsAcceptance",
                        logs_accepted);
}

void RecordCleanerLogsAcceptanceHistogram(bool logs_accepted) {
  UMA_HISTOGRAM_BOOLEAN("SoftwareReporter.CleanerLogsAcceptance",
                        logs_accepted);
}

void RecordCleanupResultHistogram(CleanupResultHistogramValue result) {
  UMA_HISTOGRAM_ENUMERATION("SoftwareReporter.Cleaner.CleanupResult", result,
                            CLEANUP_RESULT_MAX);
}

void RecordIPCDisconnectedHistogram(IPCDisconnectedHistogramValue error) {
  UMA_HISTOGRAM_ENUMERATION("SoftwareReporter.IPCDisconnected", error,
                            IPC_DISCONNECTED_MAX);
}

void RecordReporterSequenceTypeHistogram(
    SwReporterInvocationType invocation_type) {
  UMA_HISTOGRAM_ENUMERATION("SoftwareReporter.ReporterSequenceType",
                            static_cast<int>(invocation_type),
                            static_cast<int>(SwReporterInvocationType::kMax));
}

void RecordReporterSequenceResultHistogram(
    SwReporterInvocationType invocation_type,
    SwReporterInvocationResult result) {
  if (invocation_type == SwReporterInvocationType::kPeriodicRun) {
    UMA_HISTOGRAM_ENUMERATION(
        "SoftwareReporter.ReporterSequenceResult_Periodic",
        static_cast<int>(result),
        static_cast<int>(SwReporterInvocationResult::kMax));
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "SoftwareReporter.ReporterSequenceResult_UserInitiated",
        static_cast<int>(result),
        static_cast<int>(SwReporterInvocationResult::kMax));
  }
}

void RecordOnDemandUpdateRequiredHistogram(bool value) {
  UMA_HISTOGRAM_BOOLEAN("SoftwareReporter.OnDemandUpdateRequired", value);
}

}  // namespace

ChromeCleanerControllerDelegate::ChromeCleanerControllerDelegate() = default;

ChromeCleanerControllerDelegate::~ChromeCleanerControllerDelegate() = default;

void ChromeCleanerControllerDelegate::FetchAndVerifyChromeCleaner(
    FetchedCallback fetched_callback) {
  FetchChromeCleaner(
      base::BindOnce(&OnChromeCleanerFetched, base::Passed(&fetched_callback)),
      g_browser_process->system_network_context_manager()
          ->GetURLLoaderFactory());
}

bool ChromeCleanerControllerDelegate::IsMetricsAndCrashReportingEnabled() {
  return ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled();
}

void ChromeCleanerControllerDelegate::TagForResetting(Profile* profile) {
  if (PostCleanupSettingsResetter::IsEnabled())
    PostCleanupSettingsResetter().TagForResetting(profile);
}

void ChromeCleanerControllerDelegate::ResetTaggedProfiles(
    std::vector<Profile*> profiles,
    base::OnceClosure continuation) {
  if (PostCleanupSettingsResetter::IsEnabled()) {
    PostCleanupSettingsResetter().ResetTaggedProfiles(
        std::move(profiles), std::move(continuation),
        std::make_unique<PostCleanupSettingsResetter::Delegate>());
  }
}

void ChromeCleanerControllerDelegate::StartRebootPromptFlow(
    ChromeCleanerController* controller) {
  // The controller object decides if and when a prompt should be shown.
  ChromeCleanerRebootDialogControllerImpl::Create(controller);
}

// static
ChromeCleanerControllerImpl* ChromeCleanerControllerImpl::GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!g_controller) {
    g_controller = new ChromeCleanerControllerImpl();
  }

  return g_controller;
}

// static
ChromeCleanerController* ChromeCleanerController::GetInstance() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return ChromeCleanerControllerImpl::GetInstance();
}

ChromeCleanerController::State ChromeCleanerControllerImpl::state() const {
  return state_;
}

ChromeCleanerController::IdleReason ChromeCleanerControllerImpl::idle_reason()
    const {
  return idle_reason_;
}

void ChromeCleanerControllerImpl::SetLogsEnabled(Profile* profile,
                                                 bool logs_enabled) {
  PrefService* profile_prefs = profile->GetPrefs();
  profile_prefs->SetBoolean(prefs::kSwReporterReportingEnabled, logs_enabled);
}

bool ChromeCleanerControllerImpl::logs_enabled(Profile* profile) const {
  PrefService* profile_prefs = profile->GetPrefs();
  return profile_prefs->GetBoolean(prefs::kSwReporterReportingEnabled);
}

void ChromeCleanerControllerImpl::ResetIdleState() {
  if (state() != State::kIdle || idle_reason() == IdleReason::kInitial)
    return;

  idle_reason_ = IdleReason::kInitial;

  // SetStateAndNotifyObservers doesn't allow transitions to the same state.
  // Notify observers directly instead.
  for (auto& observer : observer_list_)
    NotifyObserver(&observer);
}

void ChromeCleanerControllerImpl::SetDelegateForTesting(
    ChromeCleanerControllerDelegate* delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_ = delegate ? delegate : real_delegate_.get();
  DCHECK(delegate_);
}

void ChromeCleanerControllerImpl::SetStateForTesting(State state) {
  state_ = state;
  if (state_ == State::kIdle)
    idle_reason_ = IdleReason::kInitial;
}

// static
void ChromeCleanerControllerImpl::ResetInstanceForTesting() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (g_controller) {
    delete g_controller;
    g_controller = nullptr;
  }
}

void ChromeCleanerControllerImpl::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observer_list_.AddObserver(observer);
  NotifyObserver(observer);
}

void ChromeCleanerControllerImpl::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observer_list_.RemoveObserver(observer);
}

void ChromeCleanerControllerImpl::OnReporterSequenceStarted() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  RecordReporterSequenceTypeHistogram(pending_invocation_type_);

  if (state() == State::kIdle)
    SetStateAndNotifyObservers(State::kReporterRunning);
}

void ChromeCleanerControllerImpl::OnReporterSequenceDone(
    SwReporterInvocationResult result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(SwReporterInvocationResult::kUnspecified, result);

  RecordReporterSequenceResultHistogram(pending_invocation_type_, result);

  // Ignore if any interaction with cleaner runs is ongoing. This can happen
  // in two situations:
  //  - The controller is currently handling the cleanup flow (states: infected,
  //    cleaning, reboot required);
  //  - The controller was handling the cleanup flow when the reporter sequence
  //    started, and we didn't transition to the reporter running state.
  //
  // That situation can happen, for example, if a new version of the reporter
  // component becomes available while the controller is handling the cleanup
  // flow. The UI should block any attempt of starting a new user-initiated scan
  // if the controller is not on an idle state, which includes when a reporter
  // sequence is currently running.
  if (state() != State::kReporterRunning)
    return;

  switch (result) {
    case SwReporterInvocationResult::kNotScheduled:
      // This can happen if a new periodic reporter run tried to start (for
      // example, because a new reporter component version became available) and
      // there is another reporter sequence currently running.
      // Ignore and wait until the other sequence completes to update state.
      return;

    case SwReporterInvocationResult::kTimedOut:
    case SwReporterInvocationResult::kComponentNotAvailable:
    case SwReporterInvocationResult::kProcessFailedToLaunch:
    case SwReporterInvocationResult::kGeneralFailure:
      idle_reason_ = IdleReason::kReporterFailed;
      break;

    case SwReporterInvocationResult::kNothingFound:
      idle_reason_ = IdleReason::kReporterFoundNothing;
      break;

    case SwReporterInvocationResult::kCleanupNotOffered:
      idle_reason_ = IdleReason::kReporterFoundNothing;
      break;

    case SwReporterInvocationResult::kCleanupToBeOffered:
      // A request to scan will immediately follow this message, so no state
      // transition will be needed.
      return;

    default:
      NOTREACHED();
  }

  SetStateAndNotifyObservers(State::kIdle);
}

void ChromeCleanerControllerImpl::OnSwReporterReady(
    SwReporterInvocationSequence&& invocations) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!invocations.container().empty());

  SwReporterInvocationType invocation_type =
      SwReporterInvocationType::kPeriodicRun;
  {
    base::AutoLock autolock(lock_);
    // Cache a copy of the invocations.
    cached_reporter_invocations_ =
        std::make_unique<SwReporterInvocationSequence>(invocations);
    std::swap(pending_invocation_type_, invocation_type);
  }
  safe_browsing::MaybeStartSwReporter(invocation_type, std::move(invocations));
}

void ChromeCleanerControllerImpl::RequestUserInitiatedScan(Profile* profile) {
  base::AutoLock autolock(lock_);
  DCHECK(IsAllowedByPolicy());
  DCHECK(pending_invocation_type_ !=
             SwReporterInvocationType::kUserInitiatedWithLogsAllowed &&
         pending_invocation_type_ !=
             SwReporterInvocationType::kUserInitiatedWithLogsDisallowed);

  const bool logs_enabled = this->logs_enabled(profile);
  RecordScannerLogsAcceptanceHistogram(logs_enabled);

  SwReporterInvocationType invocation_type =
      logs_enabled ? SwReporterInvocationType::kUserInitiatedWithLogsAllowed
                   : SwReporterInvocationType::kUserInitiatedWithLogsDisallowed;

  if (cached_reporter_invocations_) {
    SwReporterInvocationSequence copied_sequence(*cached_reporter_invocations_);

    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(
            &safe_browsing::MaybeStartSwReporter, invocation_type,
            // The invocations will be modified by the |ReporterRunner|.
            // Give it a copy to keep the cached invocations pristine.
            base::Passed(&copied_sequence)));

    RecordOnDemandUpdateRequiredHistogram(false);
  } else {
    pending_invocation_type_ = invocation_type;
    OnReporterSequenceStarted();

    // Creation of the |SwReporterOnDemandFetcher| automatically starts fetching
    // the SwReporter component. |OnSwReporterReady| will be called if the
    // component is successfully installed. Otherwise, |OnReporterSequenceDone|
    // will be called.
    on_demand_sw_reporter_fetcher_ =
        std::make_unique<component_updater::SwReporterOnDemandFetcher>(
            g_browser_process->component_updater(),
            base::BindOnce(&ChromeCleanerController::OnReporterSequenceDone,
                           base::Unretained(this),
                           SwReporterInvocationResult::kComponentNotAvailable));

    RecordOnDemandUpdateRequiredHistogram(true);
  }
}

void ChromeCleanerControllerImpl::Scan(
    const SwReporterInvocation& reporter_invocation) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsAllowedByPolicy());
  DCHECK(reporter_invocation.BehaviourIsSupported(
      SwReporterInvocation::BEHAVIOUR_TRIGGER_PROMPT));

  if (state() != State::kIdle && state() != State::kReporterRunning)
    return;

  DCHECK(!reporter_invocation_);
  reporter_invocation_ =
      std::make_unique<SwReporterInvocation>(reporter_invocation);

  const std::string& reporter_engine =
      reporter_invocation_->command_line().GetSwitchValueASCII(
          chrome_cleaner::kEngineSwitch);
  // Currently, only engine=2 corresponds to a partner-powered engine. This
  // condition should be updated if other partner-powered engines are added.
  powered_by_partner_ = !reporter_engine.empty() && reporter_engine == "2";

  SetStateAndNotifyObservers(State::kScanning);
  // base::Unretained is safe because the ChromeCleanerController instance is
  // guaranteed to outlive the UI thread.
  delegate_->FetchAndVerifyChromeCleaner(base::BindOnce(
      &ChromeCleanerControllerImpl::OnChromeCleanerFetchedAndVerified,
      base::Unretained(this)));
}

void ChromeCleanerControllerImpl::ReplyWithUserResponse(
    Profile* profile,
    extensions::ExtensionService* extension_service,
    UserResponse user_response) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (state() != State::kInfected)
    return;

  DCHECK(prompt_user_reply_callback_);

  PromptUserResponse::PromptAcceptance acceptance = PromptUserResponse::DENIED;
  State new_state = State::kIdle;
  switch (user_response) {
    case UserResponse::kAcceptedWithLogs:
      acceptance = PromptUserResponse::ACCEPTED_WITH_LOGS;
      SetLogsEnabled(profile, true);
      RecordCleanerLogsAcceptanceHistogram(true);
      new_state = State::kCleaning;
      delegate_->TagForResetting(profile);
      extension_service_ = extension_service;
      extension_registry_ = extensions::ExtensionRegistry::Get(profile);
      break;
    case UserResponse::kAcceptedWithoutLogs:
      acceptance = PromptUserResponse::ACCEPTED_WITHOUT_LOGS;
      SetLogsEnabled(profile, false);
      RecordCleanerLogsAcceptanceHistogram(false);
      new_state = State::kCleaning;
      delegate_->TagForResetting(profile);
      extension_service_ = extension_service;
      extension_registry_ = extensions::ExtensionRegistry::Get(profile);
      break;
    case UserResponse::kDenied:  // Fallthrough
    case UserResponse::kDismissed:
      acceptance = PromptUserResponse::DENIED;
      idle_reason_ = IdleReason::kUserDeclinedCleanup;
      new_state = State::kIdle;
      break;
  }

  std::move(prompt_user_reply_callback_).Run(acceptance);

  if (new_state == State::kCleaning)
    time_cleanup_started_ = base::Time::Now();

  // The transition to a new state should happen only after the response has
  // been posted on the UI thread so that if we transition to the kIdle state,
  // the response callback is not cleared before it has been posted.
  SetStateAndNotifyObservers(new_state);
}

void ChromeCleanerControllerImpl::Reboot() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (state() != State::kRebootRequired)
    return;

  UMA_HISTOGRAM_BOOLEAN("SoftwareReporter.Cleaner.RebootResponse", true);
  InitiateReboot();
}

bool ChromeCleanerControllerImpl::IsAllowedByPolicy() {
  return safe_browsing::SwReporterIsAllowedByPolicy();
}

bool ChromeCleanerControllerImpl::IsReportingManagedByPolicy(Profile* profile) {
  // Logs are considered managed if the logs themselves are managed or if the
  // entire cleanup feature is disabled by policy.
  PrefService* profile_prefs = profile->GetPrefs();
  return !IsAllowedByPolicy() ||
         (profile_prefs && profile_prefs->IsManagedPreference(
                               prefs::kSwReporterReportingEnabled));
}

ChromeCleanerControllerImpl::ChromeCleanerControllerImpl()
    : real_delegate_(std::make_unique<ChromeCleanerControllerDelegate>()),
      delegate_(real_delegate_.get()) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

ChromeCleanerControllerImpl::~ChromeCleanerControllerImpl() = default;

void ChromeCleanerControllerImpl::NotifyObserver(Observer* observer) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  switch (state_) {
    case State::kIdle:
      observer->OnIdle(idle_reason_);
      break;
    case State::kReporterRunning:
      observer->OnReporterRunning();
      break;
    case State::kScanning:
      observer->OnScanning();
      break;
    case State::kInfected:
      observer->OnInfected(powered_by_partner_, scanner_results_);
      break;
    case State::kCleaning:
      observer->OnCleaning(powered_by_partner_, scanner_results_);
      break;
    case State::kRebootRequired:
      observer->OnRebootRequired();
      break;
  }
}

void ChromeCleanerControllerImpl::SetStateAndNotifyObservers(State state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(state_, state);

  state_ = state;

  if (state_ == State::kIdle || state_ == State::kRebootRequired)
    ResetCleanerDataAndInvalidateWeakPtrs();

  for (auto& observer : observer_list_)
    NotifyObserver(&observer);
}

void ChromeCleanerControllerImpl::ResetCleanerDataAndInvalidateWeakPtrs() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  weak_factory_.InvalidateWeakPtrs();
  reporter_invocation_.reset();
  prompt_user_reply_callback_.Reset();
}

void ChromeCleanerControllerImpl::OnChromeCleanerFetchedAndVerified(
    base::FilePath executable_path) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(State::kScanning, state());
  DCHECK(reporter_invocation_);

  if (executable_path.empty()) {
    idle_reason_ = IdleReason::kCleanerDownloadFailed;
    SetStateAndNotifyObservers(State::kIdle);
    RecordPromptNotShownWithReasonHistogram(
        NO_PROMPT_REASON_CLEANER_DOWNLOAD_FAILED);
    return;
  }

  DCHECK(executable_path.MatchesExtension(FILE_PATH_LITERAL(".exe")));

  ChromeCleanerRunner::ChromeMetricsStatus metrics_status =
      delegate_->IsMetricsAndCrashReportingEnabled()
          ? ChromeCleanerRunner::ChromeMetricsStatus::kEnabled
          : ChromeCleanerRunner::ChromeMetricsStatus::kDisabled;

  ChromeCleanerRunner::RunChromeCleanerAndReplyWithExitCode(
      extension_service_, extension_registry_, executable_path,
      *reporter_invocation_, metrics_status,
      base::Bind(&ChromeCleanerControllerImpl::WeakOnPromptUser,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ChromeCleanerControllerImpl::OnConnectionClosed,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ChromeCleanerControllerImpl::OnCleanerProcessDone,
                 weak_factory_.GetWeakPtr()),
      // Our callbacks should be dispatched to the UI thread only.
      base::ThreadTaskRunnerHandle::Get());

  time_scanning_started_ = base::Time::Now();
}

// static
void ChromeCleanerControllerImpl::WeakOnPromptUser(
    const base::WeakPtr<ChromeCleanerControllerImpl>& controller,
    ChromeCleanerScannerResults&& scanner_results,
    ChromePromptActions::PromptUserReplyCallback reply_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If the weak pointer has been invalidated, the controller is no longer able
  // to receive callbacks, so respond with DENIED immediately.
  if (!controller) {
    std::move(reply_callback).Run(PromptUserResponse::DENIED);
    return;
  }

  controller->OnPromptUser(std::move(scanner_results),
                           std::move(reply_callback));
}

void ChromeCleanerControllerImpl::OnPromptUser(
    ChromeCleanerScannerResults&& scanner_results,
    ChromePromptActions::PromptUserReplyCallback reply_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(State::kScanning, state());
  DCHECK(scanner_results_.files_to_delete().empty());
  DCHECK(scanner_results_.registry_keys().empty());
  DCHECK(scanner_results_.extension_ids().empty());
  DCHECK(!prompt_user_reply_callback_);
  DCHECK(!time_scanning_started_.is_null());

  UMA_HISTOGRAM_LONG_TIMES_100("SoftwareReporter.Cleaner.ScanningTime",
                               base::Time::Now() - time_scanning_started_);

  if (scanner_results.files_to_delete().empty()) {
    std::move(reply_callback).Run(PromptUserResponse::DENIED);
    idle_reason_ = IdleReason::kScanningFoundNothing;
    SetStateAndNotifyObservers(State::kIdle);
    RecordPromptNotShownWithReasonHistogram(NO_PROMPT_REASON_NOTHING_FOUND);
    return;
  }

  UMA_HISTOGRAM_COUNTS_1000("SoftwareReporter.NumberOfFilesToDelete",
                            scanner_results.files_to_delete().size());
  scanner_results_ = std::move(scanner_results);
  prompt_user_reply_callback_ = std::move(reply_callback);
  SetStateAndNotifyObservers(State::kInfected);
}

void ChromeCleanerControllerImpl::OnConnectionClosed() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_NE(State::kIdle, state());
  DCHECK_NE(State::kRebootRequired, state());

  if (state() == State::kScanning || state() == State::kInfected) {
    if (state() == State::kScanning) {
      RecordPromptNotShownWithReasonHistogram(
          NO_PROMPT_REASON_IPC_CONNECTION_BROKEN);
      RecordIPCDisconnectedHistogram(IPC_DISCONNECTED_LOST_WHILE_SCANNING);
    } else {
      RecordIPCDisconnectedHistogram(IPC_DISCONNECTED_LOST_USER_PROMPTED);
    }

    idle_reason_ = IdleReasonWhenConnectionClosedTooSoon(state());
    SetStateAndNotifyObservers(State::kIdle);
    return;
  }
  // Nothing to do if OnConnectionClosed() is called in other states:
  // - This function will not be called in the kIdle and kRebootRequired
  //   states since we invalidate all weak pointers when we enter those states.
  // - In the kCleaning state, we don't care about the connection to the Chrome
  //   Cleaner process since communication via IPC is complete and only the
  //   exit code of the process is of any use to us (for deciding whether we
  //   need to reboot).
  RecordIPCDisconnectedHistogram(IPC_DISCONNECTED_SUCCESS);
}

void ChromeCleanerControllerImpl::OnCleanerProcessDone(
    ChromeCleanerRunner::ProcessStatus process_status) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (state() == State::kScanning || state() == State::kInfected) {
    idle_reason_ = IdleReasonWhenConnectionClosedTooSoon(state());
    SetStateAndNotifyObservers(State::kIdle);
    return;
  }

  DCHECK_EQ(State::kCleaning, state());
  DCHECK_NE(ChromeCleanerRunner::LaunchStatus::kLaunchFailed,
            process_status.launch_status);

  if (process_status.launch_status ==
      ChromeCleanerRunner::LaunchStatus::kSuccess) {
    if (process_status.exit_code == kRebootRequiredExitCode ||
        process_status.exit_code == kRebootNotRequiredExitCode) {
      DCHECK(!time_cleanup_started_.is_null());
      UMA_HISTOGRAM_CUSTOM_TIMES("SoftwareReporter.Cleaner.CleaningTime",
                                 base::Time::Now() - time_cleanup_started_,
                                 base::TimeDelta::FromMilliseconds(1),
                                 base::TimeDelta::FromHours(5), 100);
    }

    if (process_status.exit_code == kRebootRequiredExitCode) {
      RecordCleanupResultHistogram(CLEANUP_RESULT_REBOOT_REQUIRED);
      SetStateAndNotifyObservers(State::kRebootRequired);

      // Start the reboot prompt flow.
      delegate_->StartRebootPromptFlow(this);
      return;
    }

    if (process_status.exit_code == kRebootNotRequiredExitCode) {
      RecordCleanupResultHistogram(CLEANUP_RESULT_SUCCEEDED);
      delegate_->ResetTaggedProfiles(
          g_browser_process->profile_manager()->GetLoadedProfiles(),
          base::DoNothing());
      idle_reason_ = IdleReason::kCleaningSucceeded;
      SetStateAndNotifyObservers(State::kIdle);
      return;
    }
  }

  RecordCleanupResultHistogram(CLEANUP_RESULT_FAILED);
  idle_reason_ = IdleReason::kCleaningFailed;
  SetStateAndNotifyObservers(State::kIdle);
}

void ChromeCleanerControllerImpl::InitiateReboot() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  installer::ScopedTokenPrivilege scoped_se_shutdown_privilege(
      SE_SHUTDOWN_NAME);
  if (!scoped_se_shutdown_privilege.is_enabled() ||
      !::ExitWindowsEx(EWX_REBOOT, SHTDN_REASON_MAJOR_SOFTWARE |
                                       SHTDN_REASON_MINOR_OTHER |
                                       SHTDN_REASON_FLAG_PLANNED)) {
    for (auto& observer : observer_list_)
      observer.OnRebootFailed();
  }
}

}  // namespace safe_browsing
