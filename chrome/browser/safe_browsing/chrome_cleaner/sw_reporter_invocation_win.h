// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_SW_REPORTER_INVOCATION_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_SW_REPORTER_INVOCATION_WIN_H_

#include <stdint.h>

#include <queue>
#include <string>

#include "base/command_line.h"
#include "base/version.h"
#include "components/chrome_cleaner/public/constants/constants.h"

namespace safe_browsing {

// These values are used to send UMA information and are replicated in the
// enums.xml file, so the order MUST NOT CHANGE.
enum class SwReporterInvocationResult {
  kUnspecified,
  // Tried to start a new run, but a user-initiated run was already
  // happening. The UI should never allow this to happen.
  kNotScheduled,
  // The reporter process timed-out while running.
  kTimedOut,
  // The on-demand reporter run failed to download a new version of the reporter
  // component.
  kComponentNotAvailable,
  // The reporter failed to start.
  kProcessFailedToLaunch,
  // The reporter ended with a failure.
  kGeneralFailure,
  // The reporter ran successfully, but didn't find cleanable unwanted software.
  kNothingFound,
  // A periodic reporter sequence ran successfully and found cleanable unwanted
  // software, but the user shouldn't be prompted at this time.
  kCleanupNotOffered,
  // The reporter ran successfully and found cleanable unwanted software, and
  // a cleanup should be offered. A notification with this result should be
  // immediately followed by an attempt to run the cleaner in scanning mode.
  kCleanupToBeOffered,

  kMax,
};

// Identifies if an invocation was created during periodic reporter runs
// or because the user explicitly initiated a cleanup. The invocation type
// controls whether a prompt dialog will be shown to the user and under what
// conditions logs may be uploaded to Google.
//
// These values are used to send UMA information and are replicated in the
// enums.xml file, so the order MUST NOT CHANGE.
enum class SwReporterInvocationType {
  // Default value that should never be used for valid invocations.
  kUnspecified,
  // Periodic runs of the reporter are initiated by Chrome after startup.
  // If removable unwanted software is found the user may be prompted to
  // run the Chrome Cleanup tool. Logs from the software reporter will only
  // be uploaded if the user has opted-into SBER2 and if unwanted software
  // is found on the system. The cleaner process in scanning mode will not
  // upload logs.
  kPeriodicRun,
  // User-initiated runs in which the user has opted-out of sending details
  // to Google. Those runs are intended to be completely driven from the
  // Settings page, so a prompt dialog will not be shown to the user if
  // removable unwanted software is found. Logs will not be uploaded from the
  // reporter, even if the user has opted into SBER2, and cleaner logs will not
  // be uploaded.
  kUserInitiatedWithLogsDisallowed,
  // User-initiated runs in which the user has not opted-out of sending
  // details to Google. Those runs are intended to be completely driven from
  // the Settings page, so a prompt dialog will not be shown to the user if
  // removable unwanted software is found. Logs will be uploaded from both
  // the reporter and the cleaner in scanning mode (which will only run if
  // unwanted software is found by the reporter).
  kUserInitiatedWithLogsAllowed,

  kMax,
};

// Parameters used to invoke the sw_reporter component.
class SwReporterInvocation {
 public:
  // Flags to control behaviours the Software Reporter should support by
  // default. These flags are set in the Reporter installer, and experimental
  // versions of the reporter will turn on the behaviours that are not yet
  // supported.
  using Behaviours = uint32_t;
  enum : Behaviours {
    BEHAVIOUR_LOG_EXIT_CODE_TO_PREFS = 0x2,
    BEHAVIOUR_TRIGGER_PROMPT = 0x4,

    BEHAVIOURS_ENABLED_BY_DEFAULT =
        BEHAVIOUR_LOG_EXIT_CODE_TO_PREFS | BEHAVIOUR_TRIGGER_PROMPT,
  };

  explicit SwReporterInvocation(const base::CommandLine& command_line);
  SwReporterInvocation(const SwReporterInvocation& invocation);
  void operator=(const SwReporterInvocation& invocation);

  // Fluent interface methods, intended to be used during initialization.
  // Sample usage:
  //   auto invocation = SwReporterInvocation(command_line)
  //       .WithSuffix("MySuffix")
  //       .WithSupportedBehaviours(
  //           SwReporterInvocation::Behaviours::BEHAVIOUR_TRIGGER_PROMPT);
  SwReporterInvocation& WithSuffix(const std::string& suffix);
  SwReporterInvocation& WithSupportedBehaviours(
      Behaviours supported_behaviours);

  bool operator==(const SwReporterInvocation& other) const;

  const base::CommandLine& command_line() const;
  base::CommandLine& mutable_command_line();

  Behaviours supported_behaviours() const;
  bool BehaviourIsSupported(Behaviours intended_behaviour) const;

  // Experimental versions of the reporter will write metrics to registry keys
  // ending in |suffix_|. Those metrics should be copied to UMA histograms also
  // ending in |suffix_|. For the canonical version, |suffix_| will be empty.
  std::string suffix() const;

  // Indicates if the invocation type allows logs to be uploaded by the
  // reporter process.
  bool reporter_logs_upload_enabled() const;
  void set_reporter_logs_upload_enabled(bool reporter_logs_upload_enabled);

  // Indicates if the invocation type allows logs to be uploaded by the
  // cleaner process in scanning mode.
  bool cleaner_logs_upload_enabled() const;
  void set_cleaner_logs_upload_enabled(bool cleaner_logs_upload_enabled);

  chrome_cleaner::ChromePromptValue chrome_prompt() const;
  void set_chrome_prompt(chrome_cleaner::ChromePromptValue chrome_prompt);

 private:
  base::CommandLine command_line_;

  Behaviours supported_behaviours_ = BEHAVIOURS_ENABLED_BY_DEFAULT;

  std::string suffix_;

  bool reporter_logs_upload_enabled_ = false;
  bool cleaner_logs_upload_enabled_ = false;

  chrome_cleaner::ChromePromptValue chrome_prompt_ =
      chrome_cleaner::ChromePromptValue::kUnspecified;
};

class SwReporterInvocationSequence {
 public:
  using Queue = std::queue<SwReporterInvocation>;

  explicit SwReporterInvocationSequence(
      const base::Version& version = base::Version());
  SwReporterInvocationSequence(SwReporterInvocationSequence&& queue);
  SwReporterInvocationSequence(
      const SwReporterInvocationSequence& invocations_sequence);
  virtual ~SwReporterInvocationSequence();

  void PushInvocation(const SwReporterInvocation& invocation);

  void operator=(SwReporterInvocationSequence&& queue);

  base::Version version() const;

  const Queue& container() const;
  Queue& mutable_container();

 private:
  base::Version version_;
  Queue container_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_SW_REPORTER_INVOCATION_WIN_H_
