// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_MESSAGE_RETRY_HANDLER_H_
#define CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_MESSAGE_RETRY_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

class Profile;

namespace safe_browsing {

// Handles retry logic for a specific action. This class tracks the success of
// the action, maintains the retry state, and retries the action if necessary.
class MessageRetryHandler {
 public:
  // Callback signature for the retry action.
  using RetryCallback = base::OnceClosure;

  // Enum for the retry state preference.
  enum class RetryState {
    // Initialization value meaning that the service  has not
    // touched this value.
    UNSET = 0,
    // The flow started but has not completed yet. Note that the flow may never
    // complete because Chrome can exit before the logic is able to record a
    // different value. RUNNING was not selected as the name for this state
    // because the service flow may or may not be running when this
    // state is observed.
    UNKNOWN = 1,
    // Retry is needed. This could be because the notification flow failed.
    RETRY_NEEDED = 2,
    // No retry is needed. This could be because either the notification was
    // shown to the user or the flow found a state that a notification is not
    // shown for, for example: if the account is controlled by a policy.
    NO_RETRY_NEEDED = 3
  };

  // Enum for the retry outcome.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ShouldRetryOutcome {
    kUnknownType = 0,
    kRetryNeededDoRetry = 1,
    kRetryNeededKeepWaiting = 2,
    kUnsetInitializeWaitingPeriod = 3,
    kUnsetRetryBecauseDoneWaiting = 4,
    kUnsetStillWaiting = 5,
    kMaxValue = kUnsetStillWaiting,
  };

  // Constructor.
  // |profile|: The profile associated with this retry handler. Used to access
  //     preferences and other profile-specific data.
  // |retry_state_pref|: The name of the preference used to store the retry
  //     state (e.g., "my_feature.retry_state").
  // |next_retry_timestamp_pref|: The name of the preference used to store the
  //     timestamp of the next retry attempt (e.g.,
  //     "my_feature.next_retry_time").
  // |retry_attempt_startup_delay|: The amount of time to wait after
  //     construction before checking if a retry is needed. To avoid resource
  //     contention at startup and to try to wait for a tab to be available,
  //     this delay is used for the initial check after the MessageRetryHandler
  //     is created.
  // |retry_next_attempt_delay|: The amount of time to wait between subsequent
  //     retry attempts. This delay is used when a retry is necessary but the
  //     previous attempt failed.
  // |waiting_period_interval|: Time that the retry mechanism will wait before
  //     checking the retry state again. This delay is used in the case where
  //     the service hasn't yet determined whether a retry is necessary (e.g.,
  //     when the initial state is UNSET and the retry_state_dependent_pref_ (is
  //     false). This delay is different from retry_next_attempt_delay because
  //     retry_next_attempt_delay is used when a retry is necessary but the
  //     previous attempt failed, while waiting_period_interval is used when the
  //     service hasn't yet determined whether a retry is necessary.
  // |retry_callback|: The callback to be executed when retrying the action.
  //     This callback should perform the action that needs to be retried.
  // |history_sync_enabled|: Whether history sync is enabled for the user.
  //     This can be used to determine if the retry logic should be triggered.
  // |histogram_name|: The name of the histogram to use for logging retry
  //     outcomes (e.g., "MyFeature.RetryOutcome").
  // |last_update_timestamp_pref|: The name of the preference that stores the
  //     timestamp of the last successful update (e.g.,
  //     "my_feature.last_update_time").
  // |retry_state_dependent_pref|: The name of the preference that is used to
  //     determine if the retry logic should be triggered (e.g.,
  //     "my_feature.retry_state_dependent_pref").
  MessageRetryHandler(Profile* profile,
                      const std::string& retry_state_pref,
                      const std::string& next_retry_timestamp_pref,
                      base::TimeDelta retry_attempt_startup_delay,
                      base::TimeDelta retry_next_attempt_delay,
                      base::TimeDelta waiting_period_interval,
                      RetryCallback retry_callback,
                      const std::string& histogram_name,
                      const std::string& last_update_timestamp_pref,
                      const std::string& retry_state_dependent_pref);

  MessageRetryHandler(const MessageRetryHandler&) = delete;
  MessageRetryHandler& operator=(const MessageRetryHandler&) = delete;

  ~MessageRetryHandler();

  // Starts the retry logic. This function should be called when your feature is
  // first initialized.
  void StartRetryTimer();

  // Checks if the retry_callback should be retried.
  bool ShouldRetry();

  // Saves the retry state in prefs.
  void SaveRetryState(RetryState state);

 private:
  friend class MessageRetryHandlerTest;
  // Called by the retry timer to retry the action.
  void RetryAction();

  // Logs the retry outcome.
  void LogShouldRetryOutcome(ShouldRetryOutcome outcome);

  // The profile associated with this retry handler.
  raw_ptr<Profile> profile_;

  // The preference for storing the retry state.
  std::string retry_state_pref_;

  // The preference for storing the timestamp of the next retry attempt.
  std::string next_retry_timestamp_pref_;

  // Time to wait initially.
  base::TimeDelta retry_attempt_startup_delay_;

  // Time to wait between retries.
  base::TimeDelta retry_next_attempt_delay_;

  // Time before checking state again.
  base::TimeDelta waiting_period_interval_;

  // The callback to be executed when retrying the action.
  RetryCallback retry_callback_;

  // The timer used to retry the action.
  base::OneShotTimer retry_timer_;

  // The name of the retry histogram for logging.
  std::string retry_histogram_name_;

  // The preference name for the last update timestamp.
  std::string last_update_timestamp_pref_;

  // The preference name that is used to determine if the retry logic should be
  // triggered.
  std::string retry_state_dependent_pref_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_TAILORED_SECURITY_MESSAGE_RETRY_HANDLER_H_
