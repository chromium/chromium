// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_CONTROLLER_IMPL_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_CONTROLLER_IMPL_WIN_H_

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_win.h"

#include <memory>
#include <set>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_runner_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_scanner_results_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_prompt_actions_win.h"
#include "components/component_updater/component_updater_service.h"

namespace extensions {
class ExtensionRegistry;
}

namespace safe_browsing {

// Delegate class that provides services to the ChromeCleanerController class
// and can be overridden by tests via
// SetChromeCleanerControllerDelegateForTesting().
class ChromeCleanerControllerDelegate {
 public:
  using FetchedCallback = base::OnceCallback<void(base::FilePath)>;

  ChromeCleanerControllerDelegate();
  virtual ~ChromeCleanerControllerDelegate();

  // Fetches and verifies the Chrome Cleaner binary and passes the name of the
  // executable to |fetched_callback|. The file name will have the ".exe"
  // extension. If the operation fails, the file name passed to
  // |fetched_callback| will be empty.
  virtual void FetchAndVerifyChromeCleaner(FetchedCallback fetched_callback);
  virtual bool IsMetricsAndCrashReportingEnabled();

  // Auxiliary methods for tagging and resetting open profiles.
  virtual void TagForResetting(Profile* profile);
  virtual void ResetTaggedProfiles(std::vector<Profile*> profiles,
                                   base::OnceClosure continuation);

  // Starts the reboot prompt flow if a cleanup requires a machine restart.
  virtual void StartRebootPromptFlow(ChromeCleanerController* controller);
};

class ChromeCleanerControllerImpl : public ChromeCleanerController {
 public:
  // Returns the global controller object.
  static ChromeCleanerControllerImpl* GetInstance();

  // ChromeCleanerController overrides.
  State state() const override;
  IdleReason idle_reason() const override;
  void SetLogsEnabled(Profile* profile, bool logs_enabled) override;
  bool logs_enabled(Profile* profile) const override;
  void ResetIdleState() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void OnReporterSequenceStarted() override;
  void OnReporterSequenceDone(SwReporterInvocationResult result) override;
  void RequestUserInitiatedScan(Profile* profile) override;
  void OnSwReporterReady(SwReporterInvocationSequence&& invocations) override;
  void Scan(const SwReporterInvocation& reporter_invocation) override;
  void ReplyWithUserResponse(Profile* profile,
                             extensions::ExtensionService* extension_service,
                             UserResponse user_response) override;
  void Reboot() override;
  bool IsAllowedByPolicy() override;
  bool IsReportingManagedByPolicy(Profile* profile) override;

  static void ResetInstanceForTesting();
  // Passing in a nullptr as |delegate| resets the delegate to a default
  // production version.
  void SetDelegateForTesting(ChromeCleanerControllerDelegate* delegate);

  // Force the current controller's state for tests that check the effect of
  // starting and completing reporter runs.
  void SetStateForTesting(State state);

 private:
  ChromeCleanerControllerImpl();
  ~ChromeCleanerControllerImpl() override;

  void NotifyObserver(Observer* observer) const;
  void SetStateAndNotifyObservers(State state);
  // Used to invalidate weak pointers and reset accumulated data that is no
  // longer needed when entering the kIdle or kRebootRequired states.
  void ResetCleanerDataAndInvalidateWeakPtrs();

  // Callback that is called when the Chrome Cleaner binary has been fetched and
  // verified. An empty |executable_path| signals failure. A non-empty
  // |executable_path| must have the '.exe' file extension.
  void OnChromeCleanerFetchedAndVerified(base::FilePath executable_path);

  // Callback that checks if the weak pointer |controller| is still valid, and
  // if so will call OnPromptuser(). If |controller| is no longer valid, will
  // immediately send an IPC response denying the cleanup operation.
  //
  // The other callbacks below do not need corresponding "weak" callbacks,
  // because for those cases nothing needs to be done if the weak pointer
  // referencing the controller instance is no longer valid (Chrome's Callback
  // objects become no-ops if the bound weak pointer is not valid).
  static void WeakOnPromptUser(
      const base::WeakPtr<ChromeCleanerControllerImpl>& controller,
      ChromeCleanerScannerResults&& reported_results,
      ChromePromptActions::PromptUserReplyCallback reply_callback);

  void OnPromptUser(
      ChromeCleanerScannerResults&& reported_results,
      ChromePromptActions::PromptUserReplyCallback reply_callback);
  void OnConnectionClosed();
  void OnCleanerProcessDone(ChromeCleanerRunner::ProcessStatus process_status);
  void InitiateReboot();

  std::unique_ptr<ChromeCleanerControllerDelegate> real_delegate_;
  // Pointer to either real_delegate_ or one set by tests.
  ChromeCleanerControllerDelegate* delegate_;

  extensions::ExtensionService* extension_service_ = nullptr;
  extensions::ExtensionRegistry* extension_registry_ = nullptr;

  State state_ = State::kIdle;
  // Whether Cleanup is powered by an external partner.
  bool powered_by_partner_ = false;
  IdleReason idle_reason_ = IdleReason::kInitial;
  std::unique_ptr<SwReporterInvocation> reporter_invocation_;
  ChromeCleanerScannerResults scanner_results_;
  // Callback that should be called to send a response to the Chrome Cleaner
  // process.
  ChromePromptActions::PromptUserReplyCallback prompt_user_reply_callback_;

  // For metrics reporting.
  base::Time time_scanning_started_;
  base::Time time_cleanup_started_;

  base::ObserverList<Observer>::Unchecked observer_list_;

  // Mutex that guards |pending_invocation_type_|,
  // |on_demand_sw_reporter_fetcher_| and |cached_reporter_invocations_|.
  mutable base::Lock lock_;
  SwReporterInvocationType pending_invocation_type_ =
      SwReporterInvocationType::kPeriodicRun;
  std::unique_ptr<component_updater::SwReporterOnDemandFetcher>
      on_demand_sw_reporter_fetcher_;
  // Note: SwReporterInvocationSequence is mutable and should not be used more
  // than once. Special care must be taken that the invocations are not sent
  // to a |ReporterRunner| more than once.
  std::unique_ptr<SwReporterInvocationSequence> cached_reporter_invocations_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<ChromeCleanerControllerImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ChromeCleanerControllerImpl);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_CONTROLLER_IMPL_WIN_H_
