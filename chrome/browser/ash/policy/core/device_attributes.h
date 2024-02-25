// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_ATTRIBUTES_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_ATTRIBUTES_H_

#include <optional>
#include <string>

#include "components/policy/core/common/cloud/cloud_policy_constants.h"

namespace policy {

// Interface for accessing device attributes of Chrome OS managed devices.
class DeviceAttributes {
 public:
  virtual ~DeviceAttributes() = default;

  // Returns the enterprise enrollment domain if device is managed.
  virtual std::string GetEnterpriseEnrollmentDomain() const = 0;

  // Returns the manager of the domain for use in UI if specified, otherwise the
  // enterprise display domain.
  // The policy needs to be loaded before the display manager can be used.
  virtual std::string GetEnterpriseDomainManager() const = 0;

  // Returns the SSO profile id for the managing OU of this device. Currently
  // identifies the SAML settings for the device.
  virtual std::string GetSSOProfile() const = 0;

  // Returns the device asset ID if it is set.
  virtual std::string GetDeviceAssetID() const = 0;

  // Returns the device serial number if it is found.
  virtual std::string GetDeviceSerialNumber() const = 0;

  // Returns the machine name if it is set.
  virtual std::string GetMachineName() const = 0;

  // Returns the device annotated location if it is set.
  virtual std::string GetDeviceAnnotatedLocation() const = 0;

  // Returns the device's hostname as set by DeviceHostnameTemplate policy or
  // null if no policy is set by admin.
  virtual std::optional<std::string> GetDeviceHostname() const = 0;

  // Returns the cloud directory API ID or an empty string if it is not set.
  virtual std::string GetDirectoryApiID() const = 0;

  // Returns the obfuscated customer's ID or an empty string if it not set.
  virtual std::string GetObfuscatedCustomerID() const = 0;

  // Returns the organization logo URL or an empty string if it is not set.
  virtual std::string GetCustomerLogoURL() const = 0;

  // Returns device's market segment.
  virtual MarketSegment GetEnterpriseMarketSegment() const = 0;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_ATTRIBUTES_H_
