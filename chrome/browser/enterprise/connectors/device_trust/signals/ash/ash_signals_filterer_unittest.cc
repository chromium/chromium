// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/enterprise/connectors/device_trust/signals/ash/ash_signals_filterer.h"

#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {
constexpr int kFakeSafeBrowsingProtectionLevel = 1;
constexpr char kFakeDeviceHostName[] = "fake_device_host_name";
constexpr char kFakeDeviceModel[] = "fake_device_model";
constexpr char kFakeDisplayName[] = "fake_display_name";
constexpr char kFakeImei[] = "fake_imei";
constexpr char kFakeMacAddress[] = "fake_mac_address";
constexpr char kFakeMeid[] = "fake_meid";
constexpr char kFakeSerialNumber[] = "fake_serial_number";
constexpr char kFakeSystemDnsServer[] = "fake_system_dns_server";
}  // namespace

// Creates a signal payload which contains all device identifying signals and
// some non device identifying signals.
base::Value::Dict CreateDeviceSignals() {
  base::Value::Dict signals;
  signals.Set(device_signals::names::kSafeBrowsingProtectionLevel,
              kFakeSafeBrowsingProtectionLevel);
  signals.Set(device_signals::names::kDeviceHostName, kFakeDeviceHostName);
  signals.Set(device_signals::names::kDeviceModel, kFakeDeviceModel);
  signals.Set(device_signals::names::kDisplayName, kFakeDisplayName);
  signals.Set(device_signals::names::kSerialNumber, kFakeSerialNumber);

  base::Value::List imei_list;
  imei_list.Append(kFakeImei);
  signals.Set(device_signals::names::kImei, std::move(imei_list));

  base::Value::List mac_addresses_list;
  mac_addresses_list.Append(kFakeMacAddress);
  signals.Set(device_signals::names::kMacAddresses,
              std::move(mac_addresses_list));

  base::Value::List meid_list;
  meid_list.Append(kFakeMeid);
  signals.Set(device_signals::names::kMeid, std::move(meid_list));

  base::Value::List system_dns_list;
  system_dns_list.Append(kFakeSystemDnsServer);
  signals.Set(device_signals::names::kSystemDnsServers,
              std::move(system_dns_list));

  return signals;
}

// Creates a small signal payload which does not contain all device identifying
// signals.
base::Value::Dict CreateIncompleteDeviceSignals() {
  base::Value::Dict signals;

  signals.Set(device_signals::names::kSerialNumber, kFakeSerialNumber);
  signals.Set(device_signals::names::kDeviceModel, kFakeDeviceModel);

  return signals;
}

void ValidateNonDeviceIdentifyingSignals(base::Value::Dict& signals) {
  EXPECT_EQ(
      *signals.FindInt(device_signals::names::kSafeBrowsingProtectionLevel),
      kFakeSafeBrowsingProtectionLevel);
  EXPECT_EQ(*signals.FindString(device_signals::names::kDeviceModel),
            kFakeDeviceModel);
}

void ValidateDeviceIdentifyingSignalsExist(base::Value::Dict& signals) {
  EXPECT_EQ(*signals.FindString(device_signals::names::kDeviceHostName),
            kFakeDeviceHostName);
  EXPECT_EQ(*signals.FindString(device_signals::names::kDisplayName),
            kFakeDisplayName);
  EXPECT_EQ(*signals.FindString(device_signals::names::kSerialNumber),
            kFakeSerialNumber);

  base::Value::List* imei_list = signals.FindList(device_signals::names::kImei);
  EXPECT_EQ(imei_list->size(), 1u);
  EXPECT_EQ(imei_list->front(), kFakeImei);

  base::Value::List* mac_addresses_list =
      signals.FindList(device_signals::names::kMacAddresses);
  EXPECT_EQ(mac_addresses_list->size(), 1u);
  EXPECT_EQ(mac_addresses_list->front(), kFakeMacAddress);

  base::Value::List* meid_list = signals.FindList(device_signals::names::kMeid);
  EXPECT_EQ(meid_list->size(), 1u);
  EXPECT_EQ(meid_list->front(), kFakeMeid);

  base::Value::List* system_dns_servers_list =
      signals.FindList(device_signals::names::kSystemDnsServers);
  EXPECT_EQ(system_dns_servers_list->size(), 1u);
  EXPECT_EQ(system_dns_servers_list->front(), kFakeSystemDnsServer);
}

void ValidateDeviceIdentifyingSignalsRemoved(base::Value::Dict& signals) {
  EXPECT_EQ(signals.Find(device_signals::names::kDeviceHostName), nullptr);
  EXPECT_EQ(signals.Find(device_signals::names::kDisplayName), nullptr);
  EXPECT_EQ(signals.Find(device_signals::names::kImei), nullptr);
  EXPECT_EQ(signals.Find(device_signals::names::kMacAddresses), nullptr);
  EXPECT_EQ(signals.Find(device_signals::names::kMeid), nullptr);
  EXPECT_EQ(signals.Find(device_signals::names::kSerialNumber), nullptr);
  EXPECT_EQ(signals.Find(device_signals::names::kSystemDnsServers), nullptr);
}

class AshSignalsFiltererTest : public testing::Test {
 public:
  void SetDeviceManagement(bool is_managed) {
    if (is_managed) {
      StubInstallAttributes()->SetCloudManaged("test_domain", "test_device_id");
    } else {
      StubInstallAttributes()->SetConsumerOwned();
    }
  }

  ash::StubInstallAttributes* StubInstallAttributes() {
    return stub_install_attributes_.Get();
  }

  ash::ScopedStubInstallAttributes stub_install_attributes_;
};

// Test that nothing is filtered if the device is managed
TEST_F(AshSignalsFiltererTest, Filter_DeviceManaged) {
  SetDeviceManagement(true);
  base::Value::Dict signals = CreateDeviceSignals();

  AshSignalsFilterer filterer;
  filterer.Filter(signals);

  ValidateNonDeviceIdentifyingSignals(signals);
  ValidateDeviceIdentifyingSignalsExist(signals);
}

// Test that all device identifying signals are filtered if the device is
// unmanaged.
TEST_F(AshSignalsFiltererTest, Filter_DeviceUnmanaged) {
  SetDeviceManagement(false);
  base::Value::Dict signals = CreateDeviceSignals();

  AshSignalsFilterer filterer;
  filterer.Filter(signals);

  ValidateNonDeviceIdentifyingSignals(signals);
  ValidateDeviceIdentifyingSignalsRemoved(signals);
}

// Test that all device identifying signals are filtered if the device is
// unmanaged even if it does not contain all device identifying signals.
TEST_F(AshSignalsFiltererTest, Filter_DeviceUnmanaged_IncompleteSignals) {
  SetDeviceManagement(false);
  base::Value::Dict signals = CreateIncompleteDeviceSignals();

  AshSignalsFilterer filterer;
  filterer.Filter(signals);

  ValidateDeviceIdentifyingSignalsRemoved(signals);
}

}  // namespace enterprise_connectors
