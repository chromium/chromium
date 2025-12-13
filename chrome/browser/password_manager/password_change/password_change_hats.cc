// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/password_change_hats.h"

#include "base/strings/to_string.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"

PasswordChangeHats::PasswordChangeHats(
    HatsService* hats_service,
    password_manager::PasswordStoreInterface* profile_store,
    password_manager::PasswordStoreInterface* account_store)
    : hats_service_(hats_service) {
  if (!hats_service_) {
    // No point in fetching the data if `hats_service_` is nullptr.
    return;
  }

  if (profile_store) {
    fetch_initiated_count_++;
    profile_store->GetAllLogins(weak_ptr_factory_.GetWeakPtr());
  }
  if (account_store) {
    fetch_initiated_count_++;
    account_store->GetAllLogins(weak_ptr_factory_.GetWeakPtr());
  }
}

PasswordChangeHats::~PasswordChangeHats() = default;

void PasswordChangeHats::MaybeLaunchSurvey(
    const std::string& trigger,
    std::optional<base::TimeDelta> password_change_duration,
    std::optional<bool> blocking_challenge_detected,
    content::WebContents* web_contents) {
  if (!hats_service_) {
    return;
  }

  // Product-specific data should use bucketing.
  int64_t bucketed_passwords_count =
      ukm::GetExponentialBucketMinForCounts1000(passwords_count_);
  int64_t bucketed_leaked_passwords_count =
      ukm::GetExponentialBucketMinForCounts1000(leaked_passwords_count_);

  // Hats service requires defined product-specific data to be non-empty.
  // Pass -1 if the data is not fetched yet, so it can be filtered out.
  if (fetch_initiated_count_ != fetch_successful_count_) {
    bucketed_passwords_count = -1;
    bucketed_leaked_passwords_count = -1;
  }

  SurveyStringData survey_string_data = {
      {password_manager::features_util::kPasswordChangeBreachedPasswordsCount,
       base::ToString(bucketed_leaked_passwords_count)},
      {password_manager::features_util::kPasswordChangeSavedPasswordsCount,
       base::ToString(bucketed_passwords_count)}};
  if (password_change_duration.has_value()) {
    int64_t bucketed_runtime = ukm::GetSemanticBucketMinForDurationTiming(
        password_change_duration->InMilliseconds());
    survey_string_data
        [password_manager::features_util::kPasswordChangeRuntime] =
            base::ToString(bucketed_runtime);
  }

  SurveyBitsData survey_bits_data = {
      {password_manager::features_util::
           kPasswordChangeSuggestedPasswordsAdoption,
       adopted_generated_passwords_}};
  if (blocking_challenge_detected.has_value()) {
    survey_bits_data[password_manager::features_util::
                         kPasswordChangeBlockingChallengeDetected] =
        *blocking_challenge_detected;
  }

  hats_service_->LaunchDelayedSurveyForWebContents(
      trigger, web_contents,
      /*timeout_ms=*/0, survey_bits_data, survey_string_data);
}

void PasswordChangeHats::OnGetPasswordStoreResultsOrErrorFrom(
    password_manager::PasswordStoreInterface* store,
    password_manager::LoginsResultOrError results_or_error) {
  if (std::holds_alternative<password_manager::PasswordStoreBackendError>(
          results_or_error)) {
    return;
  }

  fetch_successful_count_++;
  std::vector<password_manager::PasswordForm> forms =
      std::get<password_manager::LoginsResult>(results_or_error);
  passwords_count_ += static_cast<int64_t>(forms.size());
  leaked_passwords_count_ += std::ranges::count_if(forms, [](const auto& form) {
    return form.password_issues.contains(
        password_manager::InsecureType::kLeaked);
  });
  if (std::ranges::any_of(forms, [](const auto& form) {
        return form.type == password_manager::PasswordForm::Type::kGenerated;
      })) {
    adopted_generated_passwords_ = true;
  }
}
