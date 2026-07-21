// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_ATTRIBUTES_IMPL_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_ATTRIBUTES_IMPL_H_

#include <string>

#include "base/memory/raw_ref.h"
#include "chrome/browser/ash/policy/core/device_attributes.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"

namespace policy {

class BrowserPolicyConnectorAsh;

// This implementation of DeviceAttributes forwards calls to
// |BrowserPolicyConnectorAsh| to retrieve device attributes of Chrome OS
// managed devices.
class DeviceAttributesImpl : public DeviceAttributes {
 public:
  // `browser_policy_connector` must not be null and must outlive this object.
  explicit DeviceAttributesImpl(
      BrowserPolicyConnectorAsh* browser_policy_connector);
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

 private:
  const raw_ref<BrowserPolicyConnectorAsh> browser_policy_connector_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_ATTRIBUTES_IMPL_H_
