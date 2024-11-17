// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_ATTRIBUTES_FAKE_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_ATTRIBUTES_FAKE_H_

#include <optional>
#include <string>

#include "chrome/browser/ash/policy/core/device_attributes.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"

namespace policy {

// Fake implementation of DeviceAttributes designed to be used from unit tests.
class FakeDeviceAttributes : public DeviceAttributes {
 public:
  FakeDeviceAttributes();
  ~FakeDeviceAttributes() override;

  // Not copyable nor movable.
  FakeDeviceAttributes(const FakeDeviceAttributes&) = delete;
  FakeDeviceAttributes& operator=(const FakeDeviceAttributes&) = delete;
  FakeDeviceAttributes(const FakeDeviceAttributes&&) = delete;
  FakeDeviceAttributes& operator=(const FakeDeviceAttributes&&) = delete;

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

  void SetFakeEnterpriseEnrollmentDomain(
      const std::string& enterprise_enrollment_domain);

  void SetFakeEnterpriseDomainManager(
      const std::string& enterprise_domain_manager);

  void SetFakeSsoProfile(const std::string& sso_profile);

  void SetFakeDeviceAssetId(const std::string& device_asset_id);

  void SetFakeDeviceSerialNumber(const std::string& device_serial_number);

  void SetFakeMachineName(const std::string& machine_name);

  void SetFakeDeviceAnnotatedLocation(
      const std::string& device_annotated_location);

  void SetFakeDeviceHostname(std::optional<std::string> device_hostname);

  void SetFakeDirectoryApiId(const std::string& directory_api_id);

  void SetFakeObfuscatedCustomerId(const std::string& obfuscated_customer_id);

  void SetFakeCustomerLogoUrl(const std::string& customer_logo_url);

  void SetFakeMarketSegment(MarketSegment market_segment);

 private:
  // Fake public fields returned on DeviceAttributes getters.
  std::string fake_enterprise_enrollment_domain_;
  std::string fake_enterprise_domain_manager_;
  std::string fake_sso_profile_;
  std::string fake_device_asset_id_;
  std::string fake_device_serial_number_;
  std::string fake_machine_name_;
  std::string fake_device_annotated_location_;
  std::optional<std::string> fake_device_hostname_;
  std::string fake_directory_api_id_;
  std::string fake_obfuscated_customer_id_;
  std::string fake_customer_logo_url_;
  MarketSegment fake_market_segment_ = MarketSegment::UNKNOWN;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_DEVICE_ATTRIBUTES_FAKE_H_
