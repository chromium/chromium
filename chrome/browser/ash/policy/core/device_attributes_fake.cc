// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_attributes_fake.h"

#include <optional>
#include <string>

#include "components/policy/core/common/cloud/cloud_policy_constants.h"

namespace policy {

FakeDeviceAttributes::FakeDeviceAttributes() = default;
FakeDeviceAttributes::~FakeDeviceAttributes() = default;

std::string FakeDeviceAttributes::GetEnterpriseEnrollmentDomain() const {
  return fake_enterprise_enrollment_domain_;
}

std::string FakeDeviceAttributes::GetEnterpriseDomainManager() const {
  return fake_enterprise_domain_manager_;
}

std::string FakeDeviceAttributes::GetSSOProfile() const {
  return fake_sso_profile_;
}

std::string FakeDeviceAttributes::GetDeviceAssetID() const {
  return fake_device_asset_id_;
}

std::string FakeDeviceAttributes::GetDeviceSerialNumber() const {
  return fake_device_serial_number_;
}

std::string FakeDeviceAttributes::GetMachineName() const {
  return fake_machine_name_;
}

std::string FakeDeviceAttributes::GetDeviceAnnotatedLocation() const {
  return fake_device_annotated_location_;
}

std::optional<std::string> FakeDeviceAttributes::GetDeviceHostname() const {
  return fake_device_hostname_;
}

std::string FakeDeviceAttributes::GetDirectoryApiID() const {
  return fake_directory_api_id_;
}

std::string FakeDeviceAttributes::GetObfuscatedCustomerID() const {
  return fake_obfuscated_customer_id_;
}

std::string FakeDeviceAttributes::GetCustomerLogoURL() const {
  return fake_customer_logo_url_;
}

MarketSegment FakeDeviceAttributes::GetEnterpriseMarketSegment() const {
  return fake_market_segment_;
}

void FakeDeviceAttributes::SetFakeEnterpriseEnrollmentDomain(
    const std::string& enterprise_enrollment_domain) {
  fake_enterprise_enrollment_domain_ = enterprise_enrollment_domain;
}

void FakeDeviceAttributes::SetFakeEnterpriseDomainManager(
    const std::string& enterprise_domain_manager) {
  fake_enterprise_domain_manager_ = enterprise_domain_manager;
}

void FakeDeviceAttributes::SetFakeSsoProfile(const std::string& sso_profile) {
  fake_sso_profile_ = sso_profile;
}

void FakeDeviceAttributes::SetFakeDeviceAssetId(
    const std::string& device_asset_id) {
  fake_device_asset_id_ = device_asset_id;
}

void FakeDeviceAttributes::SetFakeDeviceSerialNumber(
    const std::string& device_serial_number) {
  fake_device_serial_number_ = device_serial_number;
}

void FakeDeviceAttributes::SetFakeMachineName(const std::string& machine_name) {
  fake_machine_name_ = machine_name;
}

void FakeDeviceAttributes::SetFakeDeviceAnnotatedLocation(
    const std::string& device_annotated_location) {
  fake_device_annotated_location_ = device_annotated_location;
}

void FakeDeviceAttributes::SetFakeDeviceHostname(
    std::optional<std::string> device_hostname) {
  fake_device_hostname_ = std::move(device_hostname);
}

void FakeDeviceAttributes::SetFakeDirectoryApiId(
    const std::string& directory_api_id) {
  fake_directory_api_id_ = directory_api_id;
}

void FakeDeviceAttributes::SetFakeObfuscatedCustomerId(
    const std::string& obfuscated_customer_id) {
  fake_obfuscated_customer_id_ = obfuscated_customer_id;
}

void FakeDeviceAttributes::SetFakeCustomerLogoUrl(
    const std::string& customer_logo_url) {
  fake_customer_logo_url_ = customer_logo_url;
}

void FakeDeviceAttributes::SetFakeMarketSegment(MarketSegment market_segment) {
  fake_market_segment_ = market_segment;
}

}  // namespace policy
