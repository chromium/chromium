// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/password_change_hats.h"

#include "base/strings/to_string.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"

PasswordChangeHats::PasswordChangeHats(Profile* profile)
    : hats_service_(
          HatsServiceFactory::GetForProfile(profile,
                                            /*create_if_necessary=*/true)) {
  if (!hats_service_) {
    // No point in fetching the data if `hats_service_` is nullptr.
    return;
  }

  password_manager::PasswordStoreInterface* profile_store =
      ProfilePasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS)
          .get();
  password_manager::PasswordStoreInterface* account_store =
      AccountPasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS)
          .get();
  if (profile_store) {
    profile_store->GetAllLogins(weak_ptr_factory_.GetWeakPtr());
  }
  if (account_store) {
    account_store->GetAllLogins(weak_ptr_factory_.GetWeakPtr());
  }
}

PasswordChangeHats::~PasswordChangeHats() = default;

void PasswordChangeHats::MaybeLaunchSurvey(
    const std::string& trigger,
    base::TimeDelta password_change_duration,
    content::WebContents* web_contents) {
  if (!hats_service_) {
    return;
  }

  // TODO(crbug.com/429595214): Although not likely, the data might not be
  // fetched from password store yet. There's no point to add some waiting logic
  // as then survey might be launched when the user is doing something unrelated
  // to password change. Figure out with the UXR whether e.g. "-1" should be
  // passed in that case instead, so the data is not analysed as empty password
  // store.
  int64_t bucketed_passwords_count =
      ukm::GetExponentialBucketMinForCounts1000(passwords_count_);
  int64_t bucketed_leaked_passwords_count =
      ukm::GetExponentialBucketMinForCounts1000(leaked_passwords_count_);
  int64_t bucketed_runtime = ukm::GetSemanticBucketMinForDurationTiming(
      password_change_duration.InMilliseconds());

  hats_service_->LaunchDelayedSurveyForWebContents(
      trigger, web_contents,
      /*timeout_ms=*/0, /*product_specific_bits_data=*/
      {{password_manager::features_util::
            kPasswordChangeSuggestedPasswordsAdoption,
        adopted_generated_passwords_}},
      /*product_specific_string_data=*/
      {{password_manager::features_util::kPasswordChangeBreachedPasswordsCount,
        base::ToString(bucketed_leaked_passwords_count)},
       {password_manager::features_util::kPasswordChangeSavedPasswordsCount,
        base::ToString(bucketed_passwords_count)},
       {password_manager::features_util::kPasswordChangeRuntime,
        base::ToString(bucketed_runtime)}});
}

void PasswordChangeHats::OnGetPasswordStoreResultsOrErrorFrom(
    password_manager::PasswordStoreInterface* store,
    password_manager::LoginsResultOrError results_or_error) {
  if (std::holds_alternative<password_manager::PasswordStoreBackendError>(
          results_or_error)) {
    return;
  }

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
