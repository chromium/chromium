// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_attributes_impl.h"

#include <string>

#include "base/check_deref.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/handlers/device_name_policy_handler.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"

namespace policy {

DeviceAttributesImpl::DeviceAttributesImpl(
    BrowserPolicyConnectorAsh* browser_policy_connector)
    : browser_policy_connector_(CHECK_DEREF(browser_policy_connector)) {}
DeviceAttributesImpl::~DeviceAttributesImpl() = default;

std::string DeviceAttributesImpl::GetEnterpriseEnrollmentDomain() const {
  return browser_policy_connector_->GetEnterpriseEnrollmentDomain();
}

std::string DeviceAttributesImpl::GetEnterpriseDomainManager() const {
  return browser_policy_connector_->GetEnterpriseDomainManager();
}

std::string DeviceAttributesImpl::GetSSOProfile() const {
  return browser_policy_connector_->GetSSOProfile();
}

std::string DeviceAttributesImpl::GetDeviceAssetID() const {
  return browser_policy_connector_->GetDeviceAssetID();
}

std::string DeviceAttributesImpl::GetDeviceSerialNumber() const {
  return std::string(
      ash::system::StatisticsProvider::GetInstance()->GetMachineID().value_or(
          ""));
}

std::string DeviceAttributesImpl::GetMachineName() const {
  return browser_policy_connector_->GetMachineName();
}

std::string DeviceAttributesImpl::GetDeviceAnnotatedLocation() const {
  return browser_policy_connector_->GetDeviceAnnotatedLocation();
}

std::optional<std::string> DeviceAttributesImpl::GetDeviceHostname() const {
  return browser_policy_connector_->GetDeviceNamePolicyHandler()
      ->GetHostnameChosenByAdministrator();
}

std::string DeviceAttributesImpl::GetDirectoryApiID() const {
  return browser_policy_connector_->GetDirectoryApiID();
}

std::string DeviceAttributesImpl::GetObfuscatedCustomerID() const {
  return browser_policy_connector_->GetObfuscatedCustomerID();
}

std::string DeviceAttributesImpl::GetCustomerLogoURL() const {
  return browser_policy_connector_->GetCustomerLogoURL();
}

MarketSegment DeviceAttributesImpl::GetEnterpriseMarketSegment() const {
  return browser_policy_connector_->GetEnterpriseMarketSegment();
}

}  // namespace policy
