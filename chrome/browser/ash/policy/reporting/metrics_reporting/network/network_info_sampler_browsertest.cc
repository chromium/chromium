// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/mock_clock.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace reporting {
namespace {

using ::chromeos::MissiveClientTestObserver;
using ::reporting::Destination;
using ::reporting::MetricData;
using ::reporting::Priority;
using ::reporting::Record;
using ::testing::Eq;

constexpr char kEid0[] = "1234";
constexpr char kEid1[] = "5678";
constexpr char kEthernetPath[] = "ethernet/path";
constexpr char kEthernetMac[] = "ethernet_mac";
constexpr char kWifiPath[] = "wifi/path";
constexpr char kWifiMac[] = "wifi_mac";
constexpr char kCellularPath[] = "cellular/path";
constexpr char kMeid[] = "12343";
constexpr char kImei[] = "5689";
constexpr char kIccid[] = "9876563";
constexpr char kMdn[] = "134345";

class NetworkDevice {
 public:
  NetworkDevice(std::string path,
                std::string type,
                std::string name,
                std::string mac_address)
      : path_(std::move(path)),
        type_(std::move(type)),
        name_(std::move(name)),
        mac_address_(mac_address) {}

  NetworkDevice(std::string path,
                std::string type,
                std::string name,
                std::string meid,
                std::string imei,
                std::string iccid,
                std::string mdn)
      : path_(std::move(path)),
        type_(std::move(type)),
        name_(std::move(name)),
        meid_(std::move(meid)),
        imei_(std::move(imei)),
        iccid_(std::move(iccid)),
        mdn_(std::move(mdn)) {}

  const std::string& path() const { return path_; }
  const std::string& type() const { return type_; }
  const std::string& name() const { return name_; }
  const std::string& mac_address() const { return mac_address_; }
  const std::string& meid() const { return meid_; }
  const std::string& imei() const { return imei_; }
  const std::string& iccid() const { return iccid_; }
  const std::string& mdn() const { return mdn_; }

  ::reporting::NetworkDeviceType GetReportingDeviceType() const {
    if (type_ == shill::kTypeEthernet) {
      return ::reporting::NetworkDeviceType::ETHERNET_DEVICE;
    } else if (type_ == shill::kTypeWifi) {
      return ::reporting::NetworkDeviceType::WIFI_DEVICE;
    } else if (type_ == shill::kTypeCellular) {
      return ::reporting::NetworkDeviceType::CELLULAR_DEVICE;
    } else {
      return ::reporting::NetworkDeviceType::NETWORK_DEVICE_TYPE_UNSPECIFIED;
    }
  }

 private:
  const std::string path_;
  const std::string type_;
  const std::string name_;
  const std::string mac_address_;
  const std::string meid_;
  const std::string imei_;
  const std::string iccid_;
  const std::string mdn_;
};

// TODO(b/237810858): Add a test for situations where
// kReportDeviceNetworkConfiguration being disabled and where no network
// interface is available.
class NetworkInfoSamplerBrowserTest
    : public policy::DevicePolicyCrosBrowserTest {
 protected:
  NetworkInfoSamplerBrowserTest() { test::MockClock::Get(); }
  ~NetworkInfoSamplerBrowserTest() override = default;
  void SetUpOnMainThread() override {
    device_client_ = ::ash::ShillDeviceClient::Get()->GetTestInterface();
    device_client_->ClearDevices();
    ::ash::HermesManagerClient::Get()->GetTestInterface()->AddEuicc(
        dbus::ObjectPath("path0"), kEid0, true, 1);
    ::ash::HermesManagerClient::Get()->GetTestInterface()->AddEuicc(
        dbus::ObjectPath("path1"), kEid1, true, 2);
  }

  void EnableReportingNetworkInterfaces() {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ::ash::kReportDeviceNetworkConfiguration, true);
  }

  void AddDevice(const NetworkDevice& device) {
    device_client_->AddDevice(device.path(), device.type(), device.name());
    if (!device.mac_address().empty()) {
      device_client_->SetDeviceProperty(device.path(), shill::kAddressProperty,
                                        base::Value(device.mac_address()),
                                        /*notify_changed=*/true);
    }

    // cellular devices have some unique properties
    if (device.type() == shill::kTypeCellular) {
      device_client_->SetDeviceProperty(device.path(), shill::kMeidProperty,
                                        base::Value(device.meid()),
                                        /*notify_changed=*/true);
      device_client_->SetDeviceProperty(device.path(), shill::kImeiProperty,
                                        base::Value(device.imei()),
                                        /*notify_changed=*/true);
      device_client_->SetDeviceProperty(device.path(), shill::kIccidProperty,
                                        base::Value(device.iccid()),
                                        /*notify_changed=*/true);
      device_client_->SetDeviceProperty(device.path(), shill::kMdnProperty,
                                        base::Value(device.mdn()),
                                        /*notify_changed=*/true);
    }
  }

  template <typename DeviceSequence>
  void AddDevices(const DeviceSequence& devices) {
    for (const auto& device : devices) {
      AddDevice(device);
    }
  }

  static bool IsRecordNetworkInterface(const Record& record) {
    if (record.destination() != Destination::INFO_METRIC) {
      return false;
    }

    MetricData record_data;
    EXPECT_TRUE(record_data.ParseFromString(record.data()));
    if (!record_data.has_info_data()) {
      return false;
    }

    return record_data.info_data().has_networks_info();
  }

  template <typename DeviceSequence>
  static void AssertNetworkInterfaces(const DeviceSequence& expected_devices,
                                      MissiveClientTestObserver* observer) {
    auto [priority, record] = observer->GetNextEnqueuedRecord();
    EXPECT_THAT(priority, Eq(Priority::SLOW_BATCH));
    EXPECT_THAT(record.destination(), Eq(Destination::INFO_METRIC));
    ASSERT_TRUE(record.has_source_info());
    EXPECT_THAT(record.source_info().source(), Eq(SourceInfo::ASH));

    MetricData record_data;
    ASSERT_TRUE(record_data.ParseFromString(record.data()));
    EXPECT_TRUE(record_data.has_timestamp_ms());
    EXPECT_FALSE(record_data.has_telemetry_data());
    ASSERT_TRUE(record_data.has_info_data());

    const auto& info_data = record_data.info_data();
    ASSERT_TRUE(info_data.has_networks_info());
    const auto& networks_info = info_data.networks_info();
    ASSERT_THAT(networks_info.network_interfaces_size(),
                Eq(static_cast<int>(expected_devices.size())));

    // Assert details of each network interface
    for (size_t i = 0u; i < expected_devices.size(); ++i) {
      const auto& network_interface = networks_info.network_interfaces(i);
      const auto& device = expected_devices[i];
      EXPECT_THAT(network_interface.type(),
                  Eq(device.GetReportingDeviceType()));
      EXPECT_THAT(network_interface.device_path(), Eq(device.path()));
      if (device.mac_address().empty()) {
        EXPECT_FALSE(network_interface.has_mac_address());
      } else {
        EXPECT_THAT(network_interface.mac_address(), Eq(device.mac_address()));
      }

      if (device.type() == shill::kTypeCellular) {
        EXPECT_THAT(network_interface.meid(), Eq(device.meid()));
        EXPECT_THAT(network_interface.imei(), Eq(device.imei()));
        EXPECT_THAT(network_interface.iccid(), Eq(device.iccid()));
        EXPECT_THAT(network_interface.mdn(), Eq(device.mdn()));
        // We always have 2 eids for all tests.
        EXPECT_THAT(network_interface.eids_size(), Eq(2));
        EXPECT_THAT(network_interface.eids(0), Eq(kEid0));
        EXPECT_THAT(network_interface.eids(1), Eq(kEid1));
      } else {
        EXPECT_FALSE(network_interface.has_meid());
        EXPECT_FALSE(network_interface.has_imei());
        EXPECT_FALSE(network_interface.has_iccid());
        EXPECT_FALSE(network_interface.has_mdn());
        EXPECT_THAT(network_interface.eids_size(), Eq(0));
      }
    }
  }

 private:
  raw_ptr<::ash::ShillDeviceClient::TestInterface, DanglingUntriaged>
      device_client_;
  ::ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

IN_PROC_BROWSER_TEST_F(NetworkInfoSamplerBrowserTest,
                       ReportNetworkInfoSingleNetworkDevice) {
  // Single network devices
  const std::array<NetworkDevice, 1u> devices{NetworkDevice(
      kEthernetPath, shill::kTypeEthernet, "ethernet", kEthernetMac)};
  AddDevices(devices);
  EnableReportingNetworkInterfaces();
  MissiveClientTestObserver observer(
      base::BindRepeating(&IsRecordNetworkInterface));

  test::MockClock::Get().Advance(metrics::kInitialCollectionDelay);
  AssertNetworkInterfaces(devices, &observer);
}

IN_PROC_BROWSER_TEST_F(NetworkInfoSamplerBrowserTest,
                       ReportNetworkInfoAllTypesOfNetworkDevices) {
  // All network devices
  const std::array<NetworkDevice, 3> devices{
      NetworkDevice(kEthernetPath, shill::kTypeEthernet, "ethernet",
                    kEthernetMac),
      NetworkDevice(kWifiPath, shill::kTypeWifi, "wifi", kWifiMac),
      NetworkDevice(kCellularPath, shill::kTypeCellular, "cellular", kMeid,
                    kImei, kIccid, kMdn)};
  AddDevices(devices);
  EnableReportingNetworkInterfaces();
  MissiveClientTestObserver observer(
      base::BindRepeating(&IsRecordNetworkInterface));

  test::MockClock::Get().Advance(metrics::kInitialCollectionDelay);
  AssertNetworkInterfaces(devices, &observer);
}

}  // namespace

}  // namespace reporting
