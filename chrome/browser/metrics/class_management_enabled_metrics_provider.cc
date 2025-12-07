// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/class_management_enabled_metrics_provider.h"

#include "base/check_is_test.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace {

constexpr std::string_view kClassManagementStudent = "student";
constexpr std::string_view kClassManagementTeacher = "teacher";
constexpr std::string_view kHistogramName = "ChromeOS.ClassManagementEnabled";
constexpr std::string_view kClassManagementEnabledName =
    "ClassManagementEnabled";

ClassManagementEnabledMetricsProvider::ClassManagementEnabled
GetClassManagementEnabled(Profile* profile) {
  const policy::UserCloudPolicyManagerAsh* const user_cloud_policy_manager =
      profile->GetUserCloudPolicyManagerAsh();
  if (!user_cloud_policy_manager) {
    return ClassManagementEnabledMetricsProvider::ClassManagementEnabled::
        kDisabled;
  }

  const base::Value* const policy =
      user_cloud_policy_manager->core()->store()->policy_map().GetValue(
          std::string(kClassManagementEnabledName), base::Value::Type::STRING);

  if (!policy) {
    return ClassManagementEnabledMetricsProvider::ClassManagementEnabled::
        kDisabled;
  }
  const std::string& policy_str = policy->GetString();
  if (policy_str == kClassManagementStudent) {
    return ClassManagementEnabledMetricsProvider::ClassManagementEnabled::
        kStudent;
  }
  if (policy_str == kClassManagementTeacher) {
    return ClassManagementEnabledMetricsProvider::ClassManagementEnabled::
        kTeacher;
  }
  return ClassManagementEnabledMetricsProvider::ClassManagementEnabled::
      kDisabled;
}
}  // namespace

ClassManagementEnabledMetricsProvider::ClassManagementEnabledMetricsProvider() {
  auto* const session_manager = session_manager::SessionManager::Get();
  // The `session_manager` is nullptr only for unit tests.
  if (session_manager) {
    session_manager->AddObserver(this);
  } else {
    CHECK_IS_TEST();
  }
}

ClassManagementEnabledMetricsProvider::
    ~ClassManagementEnabledMetricsProvider() {
  auto* const session_manager = session_manager::SessionManager::Get();
  // The `session_manager` is nullptr only for unit tests.
  if (session_manager) {
    session_manager->RemoveObserver(this);
  } else {
    CHECK_IS_TEST();
  }
}

bool ClassManagementEnabledMetricsProvider::ProvideHistograms() {
  if (!segment_.has_value()) {
    return false;
  }

  base::UmaHistogramEnumeration(kHistogramName, segment_.value());
  return true;
}

void ClassManagementEnabledMetricsProvider::OnUserSessionStarted(
    bool is_primary_user) {
  // Skip non-primary, demo, managed guest and kiosk users.
  if (!is_primary_user || profiles::IsDemoSession() ||
      chromeos::IsManagedGuestSession() || chromeos::IsKioskSession()) {
    return;
  }

  const user_manager::User* const primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  CHECK(primary_user);
  CHECK(primary_user->is_profile_created());
  Profile* const profile =
      ash::ProfileHelper::Get()->GetProfileByUser(primary_user);
  CHECK(profile);

  // Skip unmanaged and unaffiliated users.
  if (profile->IsOffTheRecord() || primary_user->IsAffiliated()) {
    return;
  }

  segment_ = GetClassManagementEnabled(profile);
}
