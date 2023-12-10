// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_ATTRIBUTES_IMPL_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_ATTRIBUTES_IMPL_H_

#include <string>

#include "chrome/browser/ash/policy/core/device_attributes.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"

namespace policy {

// This implementation of DeviceAttributes forwards calls to
// |BrowserPolicyConnectorAsh| to retrieve device attributes of Chrome OS
// managed devices.
class DeviceAttributesImpl : public DeviceAttributes {
 public:
  DeviceAttributesImpl();
  ~DeviceAttributesImpl() override;

  // Not copyable nor movable.
  DeviceAttributesImpl(const DeviceAttributesImpl&) = delete;
  DeviceAttributesImpl& operator=(const DeviceAttributesImpl&) = delete;
  DeviceAttributesImpl(const DeviceAttributesImpl&&) = delete;
  DeviceAttributesImpl& operator=(const DeviceAttributesImpl&&) = delete;

  // DeviceAttributes overrides.

  std::string GetEnterpriseEnrollmentDomain() const override;

  std::string GetEnterpriseDomainManager() const override;

  std::string GetSSOProfile() const override;

  std::string GetDeviceAssetID() const override;

  std::string GetDeviceSerialNumber() const override;

  std::string GetMachineName() const override;

  std::string GetDeviceAnnotatedLocation() const override;

  std::optional<std::string> GetDeviceHostname() const override;

  std::string GetDirectoryApiID() const override;

  std::string GetObfuscatedCustomerID() const override;

  std::string GetCustomerLogoURL() const override;

  MarketSegment GetEnterpriseMarketSegment() const override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_ATTRIBUTES_IMPL_H_
