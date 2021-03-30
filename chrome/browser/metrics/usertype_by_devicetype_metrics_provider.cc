// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/usertype_by_devicetype_metrics_provider.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace {

namespace em = enterprise_management;

using UserSegment = UserTypeByDeviceTypeMetricsProvider::UserSegment;

constexpr char kHistogramName[] = "ChromeOS.UserTypeByDeviceType.LogSegment";
const int kMgsOnUnmanagedDevice =
    UserTypeByDeviceTypeMetricsProvider::ConstructUmaValue(
        UserSegment::kManagedGuestSession,
        policy::MarketSegment::UNKNOWN);

// Returns user's segment for metrics logging.
UserSegment GetUserSegment(Profile* profile) {
  // Check for Managed Guest Session
  if (profiles::IsPublicSession()) {
    return UserSegment::kManagedGuestSession;
  }

  // Check for incognito profiles.
  if (!profile->IsRegularProfile()) {
    return UserSegment::kUnmanaged;
  }

  const policy::UserCloudPolicyManagerChromeOS* user_cloud_policy_manager =
      profile->GetUserCloudPolicyManagerChromeOS();
  if (!user_cloud_policy_manager)
    return UserSegment::kUnmanaged;
  const em::PolicyData* policy =
      user_cloud_policy_manager->core()->store()->policy();
  if (!policy || !policy->has_metrics_log_segment())
    return UserSegment::kUnmanaged;
  switch (policy->metrics_log_segment()) {
    case em::PolicyData::UNSPECIFIED:
      return UserSegment::kUnmanaged;
    case em::PolicyData::K12:
      return UserSegment::kK12;
    case em::PolicyData::UNIVERSITY:
      return UserSegment::kUniversity;
    case em::PolicyData::NONPROFIT:
      return UserSegment::kNonProfit;
    case em::PolicyData::ENTERPRISE:
      return UserSegment::kEnterprise;
  }
  NOTREACHED();
  return UserSegment::kUnmanaged;
}

}  // namespace

UserTypeByDeviceTypeMetricsProvider::UserTypeByDeviceTypeMetricsProvider() {
  session_manager::SessionManager* session_manager =
      session_manager::SessionManager::Get();
  // The |session_manager| is nullptr only for unit tests.
  if (session_manager)
    session_manager->AddObserver(this);
}

UserTypeByDeviceTypeMetricsProvider::~UserTypeByDeviceTypeMetricsProvider() {
  session_manager::SessionManager* session_manager =
      session_manager::SessionManager::Get();
  // The |session_manager| is nullptr only for unit tests.
  if (session_manager)
    session_manager->RemoveObserver(this);
}

void UserTypeByDeviceTypeMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto_unused) {
  if (!user_segment_ || !device_segment_)
    return;

  int uma_val =
      ConstructUmaValue(user_segment_.value(), device_segment_.value());

  if (uma_val == kMgsOnUnmanagedDevice) {
    LOG(WARNING) << "Can't have MGS on unmanaged device!";
    return;
  }

  base::UmaHistogramSparse(kHistogramName, uma_val);
}

void UserTypeByDeviceTypeMetricsProvider::OnUserSessionStarted(
    bool is_primary_user) {
  if (!is_primary_user)
    return;

  if (!device_segment_) {
    // Calculate the device enrollment type. Should never change during this
    // session, so should only need to do it once.
    policy::BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    device_segment_ = connector->GetEnterpriseMarketSegment();
  }

  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  DCHECK(primary_user);
  DCHECK(primary_user->is_profile_created());
  Profile* profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(primary_user);
  DCHECK(profile);

  user_segment_ = GetUserSegment(profile);
}

// static
int UserTypeByDeviceTypeMetricsProvider::ConstructUmaValue(
    UserSegment user,
    policy::MarketSegment device) {
  int user_val = static_cast<int>(user);
  int device_val = static_cast<int>(device);
  DCHECK(user_val < 0x10000) << "user_val is too high! " << user_val;
  DCHECK(device_val < 0x10000) << "device_val is too high! " << device_val;
  return user_val | (device_val << 16);
}

// static
const char* UserTypeByDeviceTypeMetricsProvider::GetHistogramNameForTesting() {
  return kHistogramName;
}
