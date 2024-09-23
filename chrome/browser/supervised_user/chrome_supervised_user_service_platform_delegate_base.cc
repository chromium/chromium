// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/chrome_supervised_user_service_platform_delegate_base.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/storage_partition.h"

ChromeSupervisedUserServicePlatformDelegateBase::
    ChromeSupervisedUserServicePlatformDelegateBase(Profile& profile)
    : profile_(profile) {
  profile_observations_.AddObservation(&profile);
}

ChromeSupervisedUserServicePlatformDelegateBase::
    ~ChromeSupervisedUserServicePlatformDelegateBase() = default;

std::string ChromeSupervisedUserServicePlatformDelegateBase::GetCountryCode()
    const {
  std::string country;
  variations::VariationsService* variations_service =
      g_browser_process->variations_service();
  if (variations_service) {
    country = variations_service->GetStoredPermanentCountry();
    if (country.empty()) {
      country = variations_service->GetLatestCountry();
    }
  }
  return country;
}

version_info::Channel
ChromeSupervisedUserServicePlatformDelegateBase::GetChannel() const {
  return chrome::GetChannel();
}

void ChromeSupervisedUserServicePlatformDelegateBase::
    OnOffTheRecordProfileCreated(Profile* off_the_record) {
  if (!off_the_record->IsIncognitoProfile()) {
    return;
  }

  // Add some detailed metrics to allow us to better spot any unexpected cases
  // where a supervised user can access incognito.
  supervised_user::SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfileIfExists(&profile_.get());
  std::optional<supervised_user::FamilyLinkUserLogRecord::Segment>
      user_log_segment =
          supervised_user::FamilyLinkUserLogRecord::Create(
              IdentityManagerFactory::GetForProfile(&profile_.get()),
              *profile_->GetPrefs(),
              *HostContentSettingsMapFactory::GetForProfile(&profile_.get()),
              supervised_user_service ? supervised_user_service->GetURLFilter()
                                      : nullptr)
              .GetSupervisionStatusForPrimaryAccount();
  if (!user_log_segment.has_value()) {
    return;
  }

  switch (user_log_segment.value()) {
    case supervised_user::FamilyLinkUserLogRecord::Segment::
        kSupervisionEnabledByPolicy:
    case supervised_user::FamilyLinkUserLogRecord::Segment::
        kSupervisionEnabledByUser:
      // This is a supervised profile. It is not expected for incognito to be
      // available except in some edge cases. Output the edge cases separately
      // from the "unexpected" bucket.
      if (profile_->GetPrefs()
              ->FindPreference(policy::policy_prefs::kIncognitoModeAvailability)
              ->IsManaged()) {
        // An Enterprise policy has taken higher precedence than the parental
        // control settings.
        base::RecordAction(base::UserMetricsAction(
            "IncognitoMode_Started_Supervised_Managed"));
      } else if (!supervised_user::IsSubjectToParentalControls(
                     *profile_->GetPrefs())) {
        // This is unexpected, and suggests there's a mismatch between the UMA
        // log segment state based on capabilities and the parental supervision
        // status mastered in prefs.
        base::RecordAction(base::UserMetricsAction(
            "IncognitoMode_Started_Supervised_LogSegment_Prefs_Mismatch"));
      } else {
        // Incognito mode is available for supervised profile for some other
        // reason.
        base::RecordAction(base::UserMetricsAction(
            "IncognitoMode_Started_Supervised_Unexpected"));
      }
      break;

    case supervised_user::FamilyLinkUserLogRecord::Segment::kUnsupervised:
    case supervised_user::FamilyLinkUserLogRecord::Segment::kMixedProfile:
      // Incognito usage is expected, so don't output any more detailed metrics.
      break;
  }
}
