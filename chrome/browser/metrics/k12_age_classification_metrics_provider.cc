// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/k12_age_classification_metrics_provider.h"

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"

namespace {

namespace em = enterprise_management;

K12AgeClassificationMetricsProvider::K12AgeClassificationSegment
GetK12UserAgeClassificationSegment(Profile* profile) {
  const policy::UserCloudPolicyManagerAsh* const user_cloud_policy_manager =
      profile->GetUserCloudPolicyManagerAsh();
  if (!user_cloud_policy_manager) {
    return K12AgeClassificationMetricsProvider::K12AgeClassificationSegment::
        kAgeUnspecified;
  }
  const em::PolicyData* const policy =
      user_cloud_policy_manager->core()->store()->policy();
  if (!policy || !policy->has_k12_age_classification_metrics_log_segment()) {
    return K12AgeClassificationMetricsProvider::K12AgeClassificationSegment::
        kAgeUnspecified;
  }
  switch (policy->k12_age_classification_metrics_log_segment()) {
    case em::PolicyData::AGE_UNDER18:
      return K12AgeClassificationMetricsProvider::K12AgeClassificationSegment::
          kAgeUnder18;
    case em::PolicyData::AGE_EQUAL_OR_OVER18:
      return K12AgeClassificationMetricsProvider::K12AgeClassificationSegment::
          kAgeEqualOrOver18;
    case em::PolicyData::AGE_UNSPECIFIED:
      [[fallthrough]];
    default:
      return K12AgeClassificationMetricsProvider::K12AgeClassificationSegment::
          kAgeUnspecified;
  }
}

}  // namespace

K12AgeClassificationMetricsProvider::K12AgeClassificationMetricsProvider() {
  auto* const session_manager = session_manager::SessionManager::Get();
  // The `session_manager` is nullptr only for unit tests.
  if (session_manager) {
    session_manager->AddObserver(this);
  } else {
    CHECK_IS_TEST();
  }
}

K12AgeClassificationMetricsProvider::~K12AgeClassificationMetricsProvider() {
  auto* const session_manager = session_manager::SessionManager::Get();
  // The `session_manager` is nullptr only for unit tests.
  if (session_manager) {
    session_manager->RemoveObserver(this);
  } else {
    CHECK_IS_TEST();
  }
}

bool K12AgeClassificationMetricsProvider::ProvideHistograms() {
  if (!segment_.has_value()) {
    return false;
  }

  base::UmaHistogramEnumeration(kHistogramName, segment_.value());
  return true;
}

void K12AgeClassificationMetricsProvider::OnUserSessionStarted(
    bool is_primary_user) {
  // Skip non-primary, demo, managed guest, kiosk users.
  if (!is_primary_user || profiles::IsDemoSession() ||
      chromeos::IsManagedGuestSession() || chromeos::IsKioskSession()) {
    return;
  }

  const session_manager::Session* const primary_session =
      session_manager::SessionManager::Get()->GetPrimarySession();
  CHECK(primary_session);
  Profile* const profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          primary_session->account_id()));
  CHECK(profile);

  // Skip unmanaged users.
  if (profile->IsOffTheRecord()) {
    return;
  }

  segment_ = GetK12UserAgeClassificationSegment(profile);
}
