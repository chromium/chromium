// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_notice_service.h"

#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_notice_constants.h"
#include "components/privacy_sandbox/privacy_sandbox_notice_storage.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"

namespace privacy_sandbox {
namespace {

base::Time GetTimeFromPref(PrefService* prefs, const std::string& pref_name) {
  return base::ValueToTime(prefs->GetValue(pref_name)).value_or(base::Time());
}

}  // namespace

PrivacySandboxNoticeService::PrivacySandboxNoticeService(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  notice_storage_ = std::make_unique<PrivacySandboxNoticeStorage>();
  CHECK(pref_service_);
  CHECK(notice_storage_);
  if (base::FeatureList::IsEnabled(
          kPrivacySandboxMigratePrefsToNoticeConsentDataModel)) {
    MigratePrivacySandboxPrefsToDataModel();
  }
}

PrivacySandboxNoticeService::~PrivacySandboxNoticeService() = default;

void PrivacySandboxNoticeService::Shutdown() {
  pref_service_ = nullptr;
  notice_storage_ = nullptr;
}

PrivacySandboxNoticeStorage* PrivacySandboxNoticeService::GetNoticeStorage() {
  return notice_storage_.get();
}

// TODO(crbug.com/333406690): Remove this once the old privacy sandbox prefs are
// migrated to the new data model.
void PrivacySandboxNoticeService::MigratePrivacySandboxPrefsToDataModel() {
  // TopicsConsentModal
  {
// TODO(crbug.com/352577199): Once CCTs are refactored using the storage
// service, change this migration code to take into account CCTs.
#if BUILDFLAG(IS_ANDROID)
    std::string topics_notice_name = kTopicsConsentModalClankBrApp;
#else
    std::string topics_notice_name = kTopicsConsentModal;
#endif  // BUILDFLAG(IS_ANDROID)

    const auto consent_update_time = GetTimeFromPref(
        pref_service_, prefs::kPrivacySandboxTopicsConsentLastUpdateTime);
    // Topics can be updated through the settings (kSettings) page or a notice
    // (kConfirmation).
    const auto* update_reason = pref_service_->GetUserPrefValue(
        prefs::kPrivacySandboxTopicsConsentLastUpdateReason);

    // Only prefs set from updating a notice are migrated. If the
    // `update_reason` isn't set at all we will leave the pref settings to their
    // default values.
    if (update_reason &&
        static_cast<TopicsConsentUpdateSource>(update_reason->GetInt()) ==
            TopicsConsentUpdateSource::kConfirmation) {
      // We need to use kPrivacySandboxTopicsConsentGiven as it stores the
      // consent status of the user ignoring overrides.
      auto* consent_decision_given = pref_service_->GetUserPrefValue(
          prefs::kPrivacySandboxTopicsConsentGiven);
      if (consent_decision_given) {
        PrivacySandboxNoticeData notice_data;
        notice_data.notice_action_taken = consent_decision_given->GetBool()
                                              ? NoticeActionTaken::kOptIn
                                              : NoticeActionTaken::kOptOut;
        notice_data.notice_action_taken_time = consent_update_time;
        notice_storage_->MigratePrivacySandboxNoticeData(
            pref_service_, notice_data, topics_notice_name);
      }
    } else if (update_reason && static_cast<TopicsConsentUpdateSource>(
                                    update_reason->GetInt()) ==
                                    TopicsConsentUpdateSource::kSettings) {
      // Prefs set by settings are mapped to 'kUnknownActionPreMigration'
      // since it's unknown what action the user took on a notice, if any
      PrivacySandboxNoticeData notice_data;
      notice_data.notice_action_taken =
          NoticeActionTaken::kUnknownActionPreMigration;
      notice_data.notice_action_taken_time = base::Time();
      notice_storage_->MigratePrivacySandboxNoticeData(
          pref_service_, notice_data, topics_notice_name);
    }
  }
}

}  // namespace privacy_sandbox
