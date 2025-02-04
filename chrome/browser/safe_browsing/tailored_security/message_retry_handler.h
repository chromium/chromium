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

  // Callback signature for retrieving the history sync state.
  using HistorySyncStateCallback = base::OnceCallback<bool()>;

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

  // The amount of time to wait after construction before checking if a retry is
  // needed.
  static constexpr const base::TimeDelta kRetryAttemptStartupDelay =
      base::Minutes(2);

  // The amount of time to wait between retry attempts.
  static constexpr const base::TimeDelta kRetryNextAttemptDelay = base::Days(1);

  // Length of time that the retry mechanism will wait before running. This
  // delay is used for the case where the service can't tell
  // if it succeeded in the past.
  static constexpr const base::TimeDelta kWaitingPeriodInterval =
      base::Days(90);

  // Constructor.
  // |profile|: The profile associated with this retry handler. Used to access
  //     preferences and other profile-specific data.
  // |retry_state_pref|: The name of the preference used to store the retry
  //     state (e.g., "my_feature.retry_state").
  // |next_retry_timestamp_pref|: The name of the preference used to store the
  //     timestamp of the next retry attempt (e.g.,
  //     "my_feature.next_retry_time").
  // |retry_delay|: The time to wait before retrying the action (e.g.,
  //     base::Days(1)).
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
                      base::TimeDelta retry_delay,
                      RetryCallback retry_callback,
                      HistorySyncStateCallback history_sync_state_callback,
                      const std::string& histogram_name,
                      const std::string& last_update_timestamp_pref,
                      const std::string& retry_state_dependent_pref);

  MessageRetryHandler(const MessageRetryHandler&) = delete;
  MessageRetryHandler& operator=(const MessageRetryHandler&) = delete;

  ~MessageRetryHandler();

  // Starts the retry logic if needed. This function checks if the required
  // conditions are met (e.g., history sync is enabled and Safe Browsing is not
  // controlled by policy) and starts a timer to retry the action if necessary.
  // This function should be called when the feature is first initialized.
  void MaybeStartRetryTimer();

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

  // The time to wait before retrying.
  base::TimeDelta retry_delay_;

  // The callback to be executed when retrying the action.
  RetryCallback retry_callback_;

  // The callback to retrieve the history sync state.
  HistorySyncStateCallback history_sync_state_callback_;

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
