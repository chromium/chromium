// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/family_link_user_metrics_provider.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "components/session_manager/core/session_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/signin/identity_manager_factory.h"
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

// This flag is used to controls two things:
// 1. Enables the metrics provider for all platforms
// 2. Updates the existing implementation on Android to calculate the value
// on-demand instead of with an observer

BASE_FEATURE(kExtendFamilyLinkUserLogSegmentToAllPlatforms,
             "ExtendFamilyLinkUserLogSegmentToAllPlatforms",
             base::FEATURE_ENABLED_BY_DEFAULT);

FamilyLinkUserMetricsProvider::FamilyLinkUserMetricsProvider() {
  auto* factory = IdentityManagerFactory::GetInstance();
  if (factory)
    scoped_factory_observation_.Observe(factory);
}

FamilyLinkUserMetricsProvider::~FamilyLinkUserMetricsProvider() = default;

bool FamilyLinkUserMetricsProvider::ProvideHistograms() {
  // This function is called at unpredictable intervals throughout the Chrome
  // session, so guarantee it will never crash.

  if (base::FeatureList::IsEnabled(
          kExtendFamilyLinkUserLogSegmentToAllPlatforms)) {
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
      identity_manager_ = IdentityManagerFactory::GetForProfile(profile);
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

  } else {
    if (!log_segment_) {
      return false;
    }
    base::UmaHistogramEnumeration(kFamilyLinkUserLogSegmentHistogramName,
                                  log_segment_.value());
    return true;
  }
  return false;
}

void FamilyLinkUserMetricsProvider::IdentityManagerCreated(
    signin::IdentityManager* identity_manager) {
  CHECK(identity_manager);
  DCHECK(!identity_manager_);
  if (base::FeatureList::IsEnabled(
          kExtendFamilyLinkUserLogSegmentToAllPlatforms)) {
    return;
  }
  identity_manager_ = identity_manager;
  scoped_observation_.Observe(identity_manager_);
  // The account may have been updated before registering the observer.
  // Set the log segment to the primary account info if it exists.
  AccountInfo primary_account_info = identity_manager_->FindExtendedAccountInfo(
      identity_manager_->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));

  if (!primary_account_info.IsEmpty())
    OnExtendedAccountInfoUpdated(primary_account_info);
}

void FamilyLinkUserMetricsProvider::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  if (base::FeatureList::IsEnabled(
          kExtendFamilyLinkUserLogSegmentToAllPlatforms)) {
    return;
  }
  signin::PrimaryAccountChangeEvent::Type event_type =
      event_details.GetEventTypeFor(signin::ConsentLevel::kSignin);
  switch (event_type) {
    case signin::PrimaryAccountChangeEvent::Type::kNone: {
      break;
    }
    case signin::PrimaryAccountChangeEvent::Type::kSet: {
      DCHECK(identity_manager_);
      AccountInfo account_info = identity_manager_->FindExtendedAccountInfo(
          event_details.GetCurrentState().primary_account);
      OnExtendedAccountInfoUpdated(account_info);
      break;
    }
    case signin::PrimaryAccountChangeEvent::Type::kCleared: {
      // Reset the log segment if the user signs out during the session.
      log_segment_.reset();
      break;
    }
  }
}

void FamilyLinkUserMetricsProvider::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  if (base::FeatureList::IsEnabled(
          kExtendFamilyLinkUserLogSegmentToAllPlatforms)) {
    return;
  }
  DCHECK_EQ(identity_manager, identity_manager_);
  identity_manager_ = nullptr;
  scoped_observation_.Reset();
}

void FamilyLinkUserMetricsProvider::OnExtendedAccountInfoUpdated(
    const AccountInfo& account_info) {
  if (base::FeatureList::IsEnabled(
          kExtendFamilyLinkUserLogSegmentToAllPlatforms)) {
    return;
  }
  if (identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin) !=
      account_info.account_id) {
    // Only record extended account information associated with the primary
    // account of the profile.
    return;
  }
  if (!AreParentalSupervisionCapabilitiesKnown(account_info.capabilities)) {
    // Because account info is fetched asynchronously it is possible for a
    // subset of the info to be updated that does not include account
    // capabilities. Only log metrics after the capability fetch completes.
    return;
  }
  auto is_subject_to_parental_controls =
      account_info.capabilities.is_subject_to_parental_controls();
  if (is_subject_to_parental_controls == signin::Tribool::kTrue) {
    auto can_stop_supervision =
        account_info.capabilities.can_stop_parental_supervision();
    if (can_stop_supervision == signin::Tribool::kTrue) {
      // Log as a supervised user that has chosen to enable parental
      // supervision on their account, e.g. Geller accounts.
      SetLogSegment(LogSegment::kSupervisionEnabledByUser);
    } else {
      // Log as a supervised user that has parental supervision enabled
      // by a policy applied to their account, e.g. Unicorn accounts.
      SetLogSegment(LogSegment::kSupervisionEnabledByPolicy);
    }
  } else {
    // Log as unsupervised user if the account is not subject to parental
    // controls.
    SetLogSegment(LogSegment::kUnsupervised);
  }
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

void FamilyLinkUserMetricsProvider::SetLogSegment(LogSegment log_segment) {
  if (base::FeatureList::IsEnabled(
          kExtendFamilyLinkUserLogSegmentToAllPlatforms)) {
    return;
  }
  log_segment_ = log_segment;
}
