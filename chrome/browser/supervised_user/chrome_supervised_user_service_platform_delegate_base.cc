// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/chrome_supervised_user_service_platform_delegate_base.h"

#include "base/check_deref.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/metrics/profile_metrics_service_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_url_filtering_service_factory.h"
#include "chrome/common/channel_info.h"
#include "components/enterprise/isolated_mode/prefs.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/browser/supervised_user_log_record.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/storage_partition.h"

namespace {

// Helper struct for holding the user metrics actions for incognito and
// isolated mode.
struct SupervisedProfileActions {
  base::UserMetricsAction managed;
  base::UserMetricsAction unexpected;
  base::UserMetricsAction log_segment_prefs_mismatch;
};

constexpr SupervisedProfileActions kIncognitoActions = {
    base::UserMetricsAction("IncognitoMode_Started_Supervised_Managed"),
    base::UserMetricsAction("IncognitoMode_Started_Supervised_Unexpected"),
    base::UserMetricsAction(
        "IncognitoMode_Started_Supervised_LogSegment_Prefs_Mismatch")};

constexpr SupervisedProfileActions kIsolatedActions = {
    base::UserMetricsAction("IsolatedMode_Started_Supervised_Managed"),
    base::UserMetricsAction("IsolatedMode_Started_Supervised_Unexpected"),
    base::UserMetricsAction(
        "IsolatedMode_Started_Supervised_LogSegment_Prefs_Mismatch")};

// Records metrics related to the creation of a supervised profile, specifically
// focusing on incognito and isolated mode usage.
//
// Args:
//   user_log_segment: The user log segment of the supervised profile.
//   is_managed: Whether the profile is managed.
//   prefs: The profile's preferences.
//   actions: The user metrics actions to record.
void RecordSupervisedProfileCreatedMetrics(
    supervised_user::SupervisedUserLogRecord::Segment user_log_segment,
    bool is_managed,
    const PrefService& prefs,
    const SupervisedProfileActions& actions) {
  switch (user_log_segment) {
    case supervised_user::SupervisedUserLogRecord::Segment::
        kSupervisionEnabledLocally:
      if (is_managed) {
        // Incognito or Isolated mode managed by policy trumps local supervision
        // (it has higher priority than supervision features).
        base::RecordAction(actions.managed);
      } else {
        base::RecordAction(actions.unexpected);
      }
      break;
    case supervised_user::SupervisedUserLogRecord::Segment::
        kSupervisionEnabledByFamilyLinkPolicy:
    case supervised_user::SupervisedUserLogRecord::Segment::
        kSupervisionEnabledByFamilyLinkUser:
      // This is a supervised profile. It is not expected for incognito or
      // isolated mode to be available except in some edge cases. Output the
      // edge cases separately from the "unexpected" bucket.
      if (is_managed) {
        // An Enterprise policy has taken higher precedence than the parental
        // control settings.
        base::RecordAction(actions.managed);
      } else if (!supervised_user::IsSubjectToParentalControls(prefs)) {
        // This is unexpected, and suggests there's a mismatch between the UMA
        // log segment state based on capabilities and the parental supervision
        // status mastered in prefs.
        base::RecordAction(actions.log_segment_prefs_mismatch);
      } else {
        // Incognito or isolated mode is available for supervised profile for
        // some other reason.
        base::RecordAction(actions.unexpected);
      }
      break;

    case supervised_user::SupervisedUserLogRecord::Segment::kParent:
    case supervised_user::SupervisedUserLogRecord::Segment::kUnsupervised:
    case supervised_user::SupervisedUserLogRecord::Segment::kMixedProfile:
      // Incognito or isolated mode usage is expected, so don't output any more
      // detailed metrics.
      break;
  }
}

}  // namespace

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
  if (!off_the_record->IsPrimaryOTRProfileWithRegularParent()) {
    return;
  }

  // Add some detailed metrics to allow us to better spot any unexpected cases
  // where a supervised user can access incognito.
  std::optional<supervised_user::SupervisedUserLogRecord::Segment>
      user_log_segment =
          supervised_user::SupervisedUserLogRecord::Create(
              IdentityManagerFactory::GetForProfile(&profile_.get()),
              *profile_->GetPrefs(),
              *HostContentSettingsMapFactory::GetForProfile(&profile_.get()),
              supervised_user::SupervisedUserUrlFilteringServiceFactory::
                  GetForProfileIfExists(&profile_.get()),
              g_browser_process->device_parental_controls(),
              CHECK_DEREF(
                  ProfileMetricsServiceFactory::GetForProfile(&profile_.get())))
              .GetSupervisionStatusForPrimaryAccount();
  if (!user_log_segment.has_value()) {
    return;
  }

  const PrefService& prefs = *profile_->GetPrefs();
  bool is_incognito = off_the_record->IsIncognitoProfile();

  bool is_managed = is_incognito
      ? prefs.IsManagedPreference(
            policy::policy_prefs::kIncognitoModeAvailability)
      : prefs.IsManagedPreference(
            enterprise_isolated_mode::kEnterpriseIsolatedModeSettings);

  RecordSupervisedProfileCreatedMetrics(
      *user_log_segment, is_managed, prefs,
      is_incognito ? kIncognitoActions : kIsolatedActions);
}

bool ChromeSupervisedUserServicePlatformDelegateBase::ShouldCloseIncognitoTabs()
    const {
  return !IncognitoModePrefs::IsIncognitoAllowed(&profile_.get());
}
