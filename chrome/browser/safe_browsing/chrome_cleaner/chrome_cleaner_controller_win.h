// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_CONTROLLER_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_CONTROLLER_WIN_H_

#include <set>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_scanner_results_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/sw_reporter_invocation_win.h"

class Profile;

namespace extensions {
class ExtensionService;
}

namespace safe_browsing {

// These values are used to send UMA information and are replicated in the
// histograms.xml file, so the order MUST NOT CHANGE.
enum CleanupStartedHistogramValue {
  CLEANUP_STARTED_FROM_PROMPT_DIALOG = 0,
  CLEANUP_STARTED_FROM_PROMPT_IN_SETTINGS = 1,

  CLEANUP_STARTED_MAX,
};

// Records a SoftwareReporter.CleanupStarted histogram.
void RecordCleanupStartedHistogram(CleanupStartedHistogramValue value);

// Interface for the Chrome Cleaner controller class that keeps track of the
// execution of the Chrome Cleaner and the various states through which the
// execution will transition. Observers can register themselves to be notified
// of state changes. Intended to be used by the Chrome Cleaner webui page and
// the Chrome Cleaner prompt dialog.
//
// This class lives on, and all its members should be called only on, the UI
// thread.
class ChromeCleanerController {
 public:
  enum class State {
    // The default state where there is no Chrome Cleaner process or IPC to keep
    // track of and a reboot of the machine is not required to complete previous
    // cleaning attempts. This is also the state the controller will end up in
    // if any errors occur during execution of the Chrome Cleaner process.
    kIdle,
    // The Software Reporter tool is currently running and there is no pending
    // action corresponding to a cleaner execution.
    kReporterRunning,
    // All steps up to and including scanning the machine occur in this
    // state. The steps include downloading the Chrome Cleaner binary, setting
    // up an IPC between Chrome and the Cleaner process, and the actual
    // scanning done by the Cleaner.
    kScanning,
    // Scanning has been completed and harmful or unwanted software was
    // found. In this state, the controller is waiting to get a response from
    // the user on whether or not they want the cleaner to remove the harmful
    // software that was found.
    kInfected,
    // The Cleaner process is cleaning the machine.
    kCleaning,
    // The cleaning has finished, but a reboot of the machine is required to
    // complete cleanup. This state will persist across restarts of Chrome until
    // a reboot is detected.
    kRebootRequired,
  };

  enum class IdleReason {
    kInitial,
    kReporterFoundNothing,
    kReporterFailed,
    kScanningFoundNothing,
    kScanningFailed,
    kConnectionLost,
    kUserDeclinedCleanup,
    kCleaningFailed,
    kCleaningSucceeded,
    kCleanerDownloadFailed,
  };

  enum class UserResponse {
    // User accepted the cleanup operation and logs upload is enabled.
    kAcceptedWithLogs,
    // User accepted the cleanup operation and logs upload is not enabled.
    kAcceptedWithoutLogs,
    // User explicitly denied the cleanup operation, for example by clicking the
    // Cleaner dialog's cancel button.
    kDenied,
    // The cleanup operation was denied when the user dismissed the Cleaner
    // dialog, for example by pressing the ESC key.
    kDismissed,
  };

  class Observer {
   public:
    virtual void OnIdle(IdleReason idle_reason) {}
    virtual void OnReporterRunning() {}
    virtual void OnScanning() {}
    virtual void OnInfected(
        bool is_powered_by_partner,
        const ChromeCleanerScannerResults& scanner_results) {}
    virtual void OnCleaning(
        bool is_powered_by_partner,
        const ChromeCleanerScannerResults& scanner_results) {}
    virtual void OnRebootRequired() {}
    virtual void OnRebootFailed() {}

   protected:
    virtual ~Observer() = default;
  };

  // Returns the global controller object.
  static ChromeCleanerController* GetInstance();

  virtual State state() const = 0;
  virtual IdleReason idle_reason() const = 0;

  // Called by Chrome Cleaner's UI when the user changes Cleaner logs upload
  // permissions. Observers are notified if |logs_enabled| is different from the
  // current permission state.
  virtual void SetLogsEnabled(Profile* profile, bool logs_enabled) = 0;
  virtual bool logs_enabled(Profile* profile) const = 0;

  // Called by the Chrome Cleaner's UI when the user dismisses the card while
  // in the kIdle state. Does nothing if the current state is not |kIdle|.
  virtual void ResetIdleState() = 0;

  // |AddObserver()| immediately notifies |observer| of the controller's state
  // by calling the corresponding |On*()| function.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Invoked by the reporter runner, notifies the controller that a reporter
  // sequence started. If there is no pending cleaner action (currently on the
  // kIdle state), then it will transition to the kReporterRunning state.
  virtual void OnReporterSequenceStarted() = 0;

  // Invoked by the reporter runner, notifies the controller that a reporter
  // sequence completed (or has not been scheduled). If there is no pending
  // cleaner action (currently on kIdle or kReporterRunning state), then it will
  // transition to either kScanning, if the reporter found removable UwS, or
  // kIdle otherwise.
  virtual void OnReporterSequenceDone(SwReporterInvocationResult result) = 0;

  // Attempts to start the reporter runner to scan the system for unwanted
  // software. Once the reporter runner has started (which may involve
  // downloading the SwReporter component), |OnReporterSequenceStarted| and
  // |OnReporterSequenceDone| will be called with the result.
  //
  // This can have adverse effects on the component updater subsystem and
  // should only be called from direct user action.
  virtual void RequestUserInitiatedScan(Profile* profile) = 0;

  // Calls |MaybeStartSwReporter| with the |invocation_type| of the next
  // scheduled run, which will be |SwReporterInvocationType::kPeriodicRun|
  // unless the user has manually requested a reporter run, in which case the
  // |SwReporterInvocationType::kUserInitiatedWithLogsAllowed| or
  // |SwReporterInvocationType::kUserInitiatedWithLogsDisallowed| types will be
  // passed.
  virtual void OnSwReporterReady(
      SwReporterInvocationSequence&& invocations) = 0;

  // Downloads the Chrome Cleaner binary, executes it and waits for the Cleaner
  // to communicate with Chrome about harmful software found on the
  // system. During this time, the controller will be in the kScanning state. If
  // any of the steps fail or if the Cleaner does not find harmful software on
  // the system, the controller will transition to the kIdle state, passing to
  // observers the reason for the transition. Otherwise, the scanner will
  // transition to the kInfected state.
  //
  // |reporter_invocation| is the invocation that was used to run the reporter
  // which found possible harmful software on the system.
  //
  // A call to Scan() will be a no-op if the controller is not in the kIdle
  // state. This gracefully handles cases where multiple user responses are
  // received, for example if a user manages to click on a "Scan" button
  // multiple times.
  virtual void Scan(const SwReporterInvocation& reporter_invocation) = 0;

  // Sends the user's response, as to whether or not they want the Chrome
  // Cleaner to remove harmful software that was found, to the Chrome Cleaner
  // process. If the user accepted the prompt, then tags |profile| for
  // post-cleanup settings reset.
  //
  // A call to ReplyWithUserResponse() will be a no-op if the controller is not
  // in the kInfected state. This gracefully handles cases where multiple user
  // responses are received, for example if a user manages to click on a
  // "Cleanup" button multiple times.
  virtual void ReplyWithUserResponse(
      Profile* profile,
      extensions::ExtensionService* extension_service,
      UserResponse user_response) = 0;

  // If the controller is in the kRebootRequired state, initiates a reboot of
  // the computer. Call this after obtaining permission from the user to
  // reboot.
  //
  // If initiating the reboot fails, observers will be notified via a call to
  // OnRebootFailed().
  //
  // Note that there are no guarantees that the reboot will in fact happen even
  // if the system calls to initiate a reboot return success.
  virtual void Reboot() = 0;

  // Returns true if the cleaner is allowed to run by enterprise policy.
  virtual bool IsAllowedByPolicy() = 0;

  // Returns true if cleaner reporting is managed by enterprise policy.
  virtual bool IsReportingManagedByPolicy(Profile* profile) = 0;

 protected:
  ChromeCleanerController();
  virtual ~ChromeCleanerController();

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeCleanerController);
};

//  These are used for debug output in tests.
std::ostream& operator<<(std::ostream& out,
                         ChromeCleanerController::State state);

std::ostream& operator<<(std::ostream& out,
                         ChromeCleanerController::UserResponse response);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_CONTROLLER_WIN_H_
