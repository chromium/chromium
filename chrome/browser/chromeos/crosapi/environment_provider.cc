// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/environment_provider.h"

#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace crosapi {

EnvironmentProvider::EnvironmentProvider() = default;
EnvironmentProvider::~EnvironmentProvider() = default;

mojom::SessionType EnvironmentProvider::GetSessionType() {
  const user_manager::User* const user =
      user_manager::UserManager::Get()->GetActiveUser();
  const Profile* const profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(user);
  if (profile->IsGuestSession()) {
    return mojom::SessionType::kGuestSession;
  }
  if (profiles::IsPublicSession()) {
    return mojom::SessionType::kPublicSession;
  }
  return mojom::SessionType::kRegularSession;
}

mojom::DeviceMode EnvironmentProvider::GetDeviceMode() {
  policy::DeviceMode mode = chromeos::InstallAttributes::Get()->GetMode();
  switch (mode) {
    case policy::DEVICE_MODE_PENDING:
      // "Pending" is an internal detail of InstallAttributes and doesn't need
      // its own mojom value.
      return mojom::DeviceMode::kNotSet;
    case policy::DEVICE_MODE_NOT_SET:
      return mojom::DeviceMode::kNotSet;
    case policy::DEVICE_MODE_CONSUMER:
      return mojom::DeviceMode::kConsumer;
    case policy::DEVICE_MODE_ENTERPRISE:
      return mojom::DeviceMode::kEnterprise;
    case policy::DEVICE_MODE_ENTERPRISE_AD:
      return mojom::DeviceMode::kEnterpriseActiveDirectory;
    case policy::DEVICE_MODE_LEGACY_RETAIL_MODE:
      return mojom::DeviceMode::kLegacyRetailMode;
    case policy::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH:
      return mojom::DeviceMode::kConsumerKioskAutolaunch;
    case policy::DEVICE_MODE_DEMO:
      return mojom::DeviceMode::kDemo;
  }
}

}  // namespace crosapi
