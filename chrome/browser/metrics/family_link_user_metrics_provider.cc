// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/family_link_user_metrics_provider.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_finder.h"
#endif

namespace {

constexpr char kFamilyLinkUserLogSegmentHistogramName[] =
    "FamilyLinkUser.LogSegment";

bool AreParentalSupervisionCapabilitiesKnown(
    const AccountCapabilities& capabilities) {
  return capabilities.can_stop_parental_supervision() !=
             signin::Tribool::kUnknown &&
         capabilities.is_subject_to_parental_controls() !=
             signin::Tribool::kUnknown;
}
}  // namespace

FamilyLinkUserMetricsProvider::~FamilyLinkUserMetricsProvider() = default;

bool FamilyLinkUserMetricsProvider::ProvideHistograms() {
  // This function is called at unpredictable intervals throughout the Chrome
  // session, so guarantee it will never crash.

    ProfileManager* profile_manager = g_browser_process->profile_manager();
    std::vector<Profile*> profile_list = profile_manager->GetLoadedProfiles();
    absl::optional<FamilyLinkUserMetricsProvider::LogSegment>
        merged_log_segment;
    for (Profile* profile : profile_list) {
#if !BUILDFLAG(IS_ANDROID)
      // TODO(b/274889379): Mock call to GetBrowserCount().
      if (!FamilyLinkUserMetricsProvider::
              skip_active_browser_count_for_unittesting_ &&
          chrome::GetBrowserCount(profile) == 0) {
        // The profile is loaded, but there's no opened browser for this
        // profile.
        continue;
      }
#endif
      signin::IdentityManager* identity_manager_ =
          IdentityManagerFactory::GetForProfile(profile);
      AccountInfo account_info = identity_manager_->FindExtendedAccountInfo(
          identity_manager_->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin));
      absl::optional<FamilyLinkUserMetricsProvider::LogSegment> profileStatus =
          SupervisionStatusOfProfile(account_info);
      if (merged_log_segment.has_value() && profileStatus.has_value() &&
          merged_log_segment.value() != profileStatus.value()) {
        base::UmaHistogramEnumeration(kFamilyLinkUserLogSegmentHistogramName,
                                      LogSegment::kMixedProfile);
        return true;
      }
      merged_log_segment = profileStatus;
    }

    if (merged_log_segment.has_value()) {
      base::UmaHistogramEnumeration(kFamilyLinkUserLogSegmentHistogramName,
                                    merged_log_segment.value());
      return true;
    }

  return false;
}

absl::optional<FamilyLinkUserMetricsProvider::LogSegment>
FamilyLinkUserMetricsProvider::SupervisionStatusOfProfile(
    const AccountInfo& account_info) {
  if (!AreParentalSupervisionCapabilitiesKnown(account_info.capabilities)) {
    return absl::nullopt;
  }
  auto is_subject_to_parental_controls =
      account_info.capabilities.is_subject_to_parental_controls();
  if (is_subject_to_parental_controls == signin::Tribool::kTrue) {
    auto can_stop_supervision =
        account_info.capabilities.can_stop_parental_supervision();
    if (can_stop_supervision == signin::Tribool::kTrue) {
      return FamilyLinkUserMetricsProvider::LogSegment::
          kSupervisionEnabledByUser;
    } else {
      // Log as a supervised user that has parental supervision enabled
      // by a policy applied to their account, e.g. Unicorn accounts.
      return FamilyLinkUserMetricsProvider::LogSegment::
          kSupervisionEnabledByPolicy;
    }
  } else {
    // Log as unsupervised user if the account is not subject to parental
    // controls.
    return FamilyLinkUserMetricsProvider::LogSegment::kUnsupervised;
  }
}

// static
const char* FamilyLinkUserMetricsProvider::GetHistogramNameForTesting() {
  return kFamilyLinkUserLogSegmentHistogramName;
}
