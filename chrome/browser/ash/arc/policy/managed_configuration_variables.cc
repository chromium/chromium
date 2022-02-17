// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/policy/managed_configuration_variables.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/system/statistics_provider.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace arc {

namespace {

// Part before "@" of the given |email| address.
// "some_email@domain.com" => "some_email"
//
// Returns empty string if |email| does not contain an "@".
const std::string EmailName(const std::string& email) {
  size_t at_sign_pos = email.find("@");
  if (at_sign_pos == std::string::npos)
    return "";
  return email.substr(0, at_sign_pos);
}

// Part after "@" of an email address.
// "some_email@domain.com" => "domain.com"
//
// Returns empty string if |email| does not contain an "@".
const std::string EmailDomain(const std::string& email) {
  size_t at_sign_pos = email.find("@");
  if (at_sign_pos == std::string::npos)
    return "";
  return email.substr(at_sign_pos + 1);
}

const std::string SignedInUserEmail(const Profile* profile) {
  DCHECK(profile);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfileIfExists(profile);
  CoreAccountInfo info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  return info.email;
}

const std::string DeviceDirectoryId() {
  return g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->GetDirectoryApiID();
}

const std::string DeviceAssetId() {
  return g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->GetDeviceAssetID();
}

const std::string DeviceAnnotatedLocation() {
  return g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->GetDeviceAnnotatedLocation();
}

const std::string DeviceSerialNumber() {
  return chromeos::system::StatisticsProvider::GetInstance()
      ->GetEnterpriseMachineID();
}

}  // namespace

const char kUserEmail[] = "${USER_EMAIL}";
const char kUserEmailName[] = "${USER_EMAIL_NAME}";
const char kUserEmailDomain[] = "${USER_EMAIL_DOMAIN}";
const char kDeviceDirectoryId[] = "${DEVICE_DIRECTORY_ID}";
const char kDeviceSerialNumber[] = "${DEVICE_SERIAL_NUMBER}";
const char kDeviceAssetId[] = "${DEVICE_ASSET_ID}";
const char kDeviceAnnotatedLocation[] = "${DEVICE_ANNOTATED_LOCATION}";

void RecursivelyReplaceManagedConfigurationVariables(
    const Profile* profile,
    base::Value* managedConfiguration) {
  // Recursive call for dictionary values.
  if (managedConfiguration->is_dict()) {
    for (auto kv : managedConfiguration->DictItems()) {
      RecursivelyReplaceManagedConfigurationVariables(profile, &kv.second);
    }
    return;
  }
  // Exit early for non string values.
  if (!managedConfiguration->is_string())
    return;

  // Replace matches on template variables.
  if (managedConfiguration->GetString() == kUserEmail) {
    *managedConfiguration = base::Value(SignedInUserEmail(profile));
  } else if (managedConfiguration->GetString() == kUserEmailName) {
    *managedConfiguration = base::Value(EmailName(SignedInUserEmail(profile)));
  } else if (managedConfiguration->GetString() == kUserEmailDomain) {
    *managedConfiguration =
        base::Value(EmailDomain(SignedInUserEmail(profile)));
  } else if (managedConfiguration->GetString() == kDeviceDirectoryId) {
    *managedConfiguration = base::Value(DeviceDirectoryId());
  } else if (managedConfiguration->GetString() == kDeviceSerialNumber) {
    *managedConfiguration = base::Value(DeviceSerialNumber());
  } else if (managedConfiguration->GetString() == kDeviceAssetId) {
    *managedConfiguration = base::Value(DeviceAssetId());
  } else if (managedConfiguration->GetString() == kDeviceAnnotatedLocation) {
    *managedConfiguration = base::Value(DeviceAnnotatedLocation());
  }
}

}  // namespace arc
