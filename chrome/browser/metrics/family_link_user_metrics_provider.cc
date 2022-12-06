// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/family_link_user_metrics_provider.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/session_manager/core/session_manager.h"

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

FamilyLinkUserMetricsProvider::FamilyLinkUserMetricsProvider() {
  auto* factory = IdentityManagerFactory::GetInstance();
  if (factory)
    scoped_factory_observation_.Observe(factory);
}

FamilyLinkUserMetricsProvider::~FamilyLinkUserMetricsProvider() = default;

bool FamilyLinkUserMetricsProvider::ProvideHistograms() {
  // This function is called at unpredictable intervals throughout the Chrome
  // session, so guarantee it will never crash.
  if (!log_segment_)
    return false;
  base::UmaHistogramEnumeration(kFamilyLinkUserLogSegmentHistogramName,
                                log_segment_.value());
  return true;
}

void FamilyLinkUserMetricsProvider::IdentityManagerCreated(
    signin::IdentityManager* identity_manager) {
  CHECK(identity_manager);
  DCHECK(!identity_manager_);

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
  DCHECK_EQ(identity_manager, identity_manager_);
  identity_manager_ = nullptr;
  scoped_observation_.Reset();
}

void FamilyLinkUserMetricsProvider::OnExtendedAccountInfoUpdated(
    const AccountInfo& account_info) {
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

// static
const char* FamilyLinkUserMetricsProvider::GetHistogramNameForTesting() {
  return kFamilyLinkUserLogSegmentHistogramName;
}

void FamilyLinkUserMetricsProvider::SetLogSegment(LogSegment log_segment) {
  log_segment_ = log_segment;
}
