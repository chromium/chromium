// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_device_attributes/enterprise_device_attributes_api_ash.h"

#include "base/values.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/hostname_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/enterprise_device_attributes.h"
#include "chromeos/system/statistics_provider.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

namespace extensions {

namespace {

// TODO(http://crbug.com/1056550): Return an error if the user is not permitted
// to get device attributes instead of an empty string.

// Checks for the current browser context if the user is affiliated or belongs
// to the sign-in profile.
bool CanGetDeviceAttributesForBrowserContext(content::BrowserContext* context) {
  const Profile* profile = Profile::FromBrowserContext(context);

  if (chromeos::ProfileHelper::IsSigninProfile(profile))
    return true;

  if (!profile->IsRegularProfile())
    return false;

  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  return user->IsAffiliated();
}

}  //  namespace

EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction::
    EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction() {}

EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction::
    ~EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction() {}

ExtensionFunction::ResponseAction
EnterpriseDeviceAttributesGetDirectoryDeviceIdFunction::Run() {
  std::string device_id;
  if (CanGetDeviceAttributesForBrowserContext(browser_context())) {
    device_id = g_browser_process->platform_part()
                    ->browser_policy_connector_chromeos()
                    ->GetDirectoryApiID();
  }
  return RespondNow(ArgumentList(
      api::enterprise_device_attributes::GetDirectoryDeviceId::Results::Create(
          device_id)));
}

EnterpriseDeviceAttributesGetDeviceSerialNumberFunction::
    EnterpriseDeviceAttributesGetDeviceSerialNumberFunction() {}

EnterpriseDeviceAttributesGetDeviceSerialNumberFunction::
    ~EnterpriseDeviceAttributesGetDeviceSerialNumberFunction() {}

ExtensionFunction::ResponseAction
EnterpriseDeviceAttributesGetDeviceSerialNumberFunction::Run() {
  std::string serial_number;
  if (CanGetDeviceAttributesForBrowserContext(browser_context())) {
    serial_number = chromeos::system::StatisticsProvider::GetInstance()
                        ->GetEnterpriseMachineID();
  }
  return RespondNow(ArgumentList(
      api::enterprise_device_attributes::GetDeviceSerialNumber::Results::Create(
          serial_number)));
}

EnterpriseDeviceAttributesGetDeviceAssetIdFunction::
    EnterpriseDeviceAttributesGetDeviceAssetIdFunction() {}

EnterpriseDeviceAttributesGetDeviceAssetIdFunction::
    ~EnterpriseDeviceAttributesGetDeviceAssetIdFunction() {}

ExtensionFunction::ResponseAction
EnterpriseDeviceAttributesGetDeviceAssetIdFunction::Run() {
  std::string asset_id;
  if (CanGetDeviceAttributesForBrowserContext(browser_context())) {
    asset_id = g_browser_process->platform_part()
                   ->browser_policy_connector_chromeos()
                   ->GetDeviceAssetID();
  }
  return RespondNow(ArgumentList(
      api::enterprise_device_attributes::GetDeviceAssetId::Results::Create(
          asset_id)));
}

EnterpriseDeviceAttributesGetDeviceAnnotatedLocationFunction::
    EnterpriseDeviceAttributesGetDeviceAnnotatedLocationFunction() {}

EnterpriseDeviceAttributesGetDeviceAnnotatedLocationFunction::
    ~EnterpriseDeviceAttributesGetDeviceAnnotatedLocationFunction() {}

ExtensionFunction::ResponseAction
EnterpriseDeviceAttributesGetDeviceAnnotatedLocationFunction::Run() {
  std::string annotated_location;
  if (CanGetDeviceAttributesForBrowserContext(browser_context())) {
    annotated_location = g_browser_process->platform_part()
                             ->browser_policy_connector_chromeos()
                             ->GetDeviceAnnotatedLocation();
  }
  return RespondNow(ArgumentList(
      api::enterprise_device_attributes::GetDeviceAnnotatedLocation::Results::
          Create(annotated_location)));
}

EnterpriseDeviceAttributesGetDeviceHostnameFunction::
    EnterpriseDeviceAttributesGetDeviceHostnameFunction() = default;

EnterpriseDeviceAttributesGetDeviceHostnameFunction::
    ~EnterpriseDeviceAttributesGetDeviceHostnameFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseDeviceAttributesGetDeviceHostnameFunction::Run() {
  std::string hostname;
  if (CanGetDeviceAttributesForBrowserContext(browser_context())) {
    hostname = g_browser_process->platform_part()
                   ->browser_policy_connector_chromeos()
                   ->GetHostnameHandler()
                   ->GetDeviceHostname();
  }
  return RespondNow(ArgumentList(
      api::enterprise_device_attributes::GetDeviceHostname::Results::Create(
          hostname)));
}

}  // namespace extensions
