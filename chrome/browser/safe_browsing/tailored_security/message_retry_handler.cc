// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/tailored_security/message_retry_handler.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/tailored_security_service/tailored_security_service_util.h"
#include "components/safe_browsing/core/common/safe_browsing_policy_handler.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace safe_browsing {
MessageRetryHandler::MessageRetryHandler(
    Profile* profile,
    const std::string& retry_state_pref,
    const std::string& next_retry_timestamp_pref,
    base::TimeDelta retry_attempt_startup_delay,
    base::TimeDelta retry_next_attempt_delay,
    base::TimeDelta waiting_period_interval,
    RetryCallback retry_callback,
    const std::string& retry_histogram_name,
    const std::string& last_update_timestamp_pref,
    const std::string& retry_state_dependent_pref)
    : profile_(profile),
      retry_state_pref_(retry_state_pref),
      next_retry_timestamp_pref_(next_retry_timestamp_pref),
      retry_attempt_startup_delay_(retry_attempt_startup_delay),
      retry_next_attempt_delay_(retry_next_attempt_delay),
      waiting_period_interval_(waiting_period_interval),
      retry_callback_(std::move(retry_callback)),
      retry_histogram_name_(retry_histogram_name),
      last_update_timestamp_pref_(last_update_timestamp_pref),
      retry_state_dependent_pref_(retry_state_dependent_pref) {}

MessageRetryHandler::~MessageRetryHandler() = default;

void MessageRetryHandler::StartRetryTimer() {
  retry_timer_.Start(FROM_HERE, retry_attempt_startup_delay_, this,
                     &MessageRetryHandler::RetryAction);
}

bool MessageRetryHandler::ShouldRetry() {
  PrefService* prefs = profile_->GetPrefs();

  // Get the retry state from prefs.
  auto retry_state =
      static_cast<RetryState>(prefs->GetInteger(retry_state_pref_));

  if (prefs->GetTime(last_update_timestamp_pref_) == base::Time()) {
    // Do nothing because the user has not updated the preference yet.
    return false;
  }

  if (retry_state == RetryState::NO_RETRY_NEEDED) {
    return false;
  } else if (retry_state == RetryState::RETRY_NEEDED) {
    if (base::Time::Now() >= prefs->GetTime(next_retry_timestamp_pref_)) {
      // Set the next attempt time.
      prefs->SetTime(next_retry_timestamp_pref_,
                     base::Time::Now() + retry_next_attempt_delay_);
      LogShouldRetryOutcome(ShouldRetryOutcome::kRetryNeededDoRetry);
      return true;
    } else {
      LogShouldRetryOutcome(ShouldRetryOutcome::kRetryNeededKeepWaiting);
      return false;
    }
  } else if (retry_state == RetryState::UNSET &&
             !prefs->GetBoolean(retry_state_dependent_pref_)) {
    if (prefs->GetTime(next_retry_timestamp_pref_) == base::Time()) {
      prefs->SetTime(next_retry_timestamp_pref_,
                     base::Time::Now() + waiting_period_interval_);
      LogShouldRetryOutcome(ShouldRetryOutcome::kUnsetInitializeWaitingPeriod);
      return false;
    } else if (base::Time::Now() >=
               prefs->GetTime(next_retry_timestamp_pref_)) {
      prefs->SetTime(next_retry_timestamp_pref_,
                     base::Time::Now() + retry_next_attempt_delay_);
      LogShouldRetryOutcome(ShouldRetryOutcome::kUnsetRetryBecauseDoneWaiting);
      return true;
    } else {
      LogShouldRetryOutcome(ShouldRetryOutcome::kUnsetStillWaiting);
      return false;
    }
  }

  return false;
}

void MessageRetryHandler::SaveRetryState(RetryState state) {
  profile_->GetPrefs()->SetInteger(retry_state_pref_, static_cast<int>(state));
}

void MessageRetryHandler::RetryAction() {
  if (ShouldRetry()) {
    std::move(retry_callback_).Run();
  }
}
void MessageRetryHandler::LogShouldRetryOutcome(ShouldRetryOutcome outcome) {
  base::UmaHistogramEnumeration(retry_histogram_name_, outcome);
}

}  // namespace safe_browsing
