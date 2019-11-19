// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chromeos_metrics_provider.h"

#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client_impl.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/system/statistics_provider.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "device/bluetooth/dbus/fake_bluetooth_adapter_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_agent_manager_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_device_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_characteristic_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_descriptor_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_gatt_service_client.h"
#include "device/bluetooth/dbus/fake_bluetooth_input_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/system_profile.pb.h"

using bluez::BluetoothAdapterClient;
using bluez::BluetoothAgentManagerClient;
using bluez::BluetoothDeviceClient;
using bluez::BluetoothGattCharacteristicClient;
using bluez::BluetoothGattDescriptorClient;
using bluez::BluetoothGattServiceClient;
using bluez::BluetoothInputClient;
using bluez::BluezDBusManager;
using bluez::BluezDBusManagerSetter;
using bluez::FakeBluetoothAdapterClient;
using bluez::FakeBluetoothAgentManagerClient;
using bluez::FakeBluetoothDeviceClient;
using bluez::FakeBluetoothGattCharacteristicClient;
using bluez::FakeBluetoothGattDescriptorClient;
using bluez::FakeBluetoothGattServiceClient;
using bluez::FakeBluetoothInputClient;

namespace {

class FakeMultiDeviceSetupClientImplFactory
    : public chromeos::multidevice_setup::MultiDeviceSetupClientImpl::Factory {
 public:
  FakeMultiDeviceSetupClientImplFactory(
      std::unique_ptr<chromeos::multidevice_setup::FakeMultiDeviceSetupClient>
          fake_multidevice_setup_client)
      : fake_multidevice_setup_client_(
            std::move(fake_multidevice_setup_client)) {}

  ~FakeMultiDeviceSetupClientImplFactory() override = default;

  // chromeos::multidevice_setup::MultiDeviceSetupClientImpl::Factory:
  // NOTE: At most, one client should be created per-test.
  std::unique_ptr<chromeos::multidevice_setup::MultiDeviceSetupClient>
  BuildInstance(service_manager::Connector* connector) override {
    EXPECT_TRUE(fake_multidevice_setup_client_);
    return std::move(fake_multidevice_setup_client_);
  }

 private:
  std::unique_ptr<chromeos::multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;
};

// Wrapper around ChromeOSMetricsProvider that initializes
// Bluetooth and hardware class in the constructor.
class TestChromeOSMetricsProvider : public ChromeOSMetricsProvider {
 public:
  TestChromeOSMetricsProvider()
      : ChromeOSMetricsProvider(metrics::MetricsLogUploader::UMA) {
    AsyncInit(base::Bind(&TestChromeOSMetricsProvider::GetIdleCallback,
                         base::Unretained(this)));
    base::RunLoop().Run();
  }

  void GetIdleCallback() {
    ASSERT_TRUE(base::RunLoop::IsRunningOnCurrentThread());
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }
};

}  // namespace

class ChromeOSMetricsProviderTest : public testing::Test {
 public:
  ChromeOSMetricsProviderTest() {}

 protected:
  void SetUp() override {
    // Set up the fake Bluetooth environment,
    std::unique_ptr<BluezDBusManagerSetter> bluez_dbus_setter =
        BluezDBusManager::GetSetterForTesting();
    bluez_dbus_setter->SetBluetoothAdapterClient(
        std::unique_ptr<BluetoothAdapterClient>(
            new FakeBluetoothAdapterClient));
    bluez_dbus_setter->SetBluetoothDeviceClient(
        std::unique_ptr<BluetoothDeviceClient>(new FakeBluetoothDeviceClient));
    bluez_dbus_setter->SetBluetoothGattCharacteristicClient(
        std::unique_ptr<BluetoothGattCharacteristicClient>(
            new FakeBluetoothGattCharacteristicClient));
    bluez_dbus_setter->SetBluetoothGattDescriptorClient(
        std::unique_ptr<BluetoothGattDescriptorClient>(
            new FakeBluetoothGattDescriptorClient));
    bluez_dbus_setter->SetBluetoothGattServiceClient(
        std::unique_ptr<BluetoothGattServiceClient>(
            new FakeBluetoothGattServiceClient));
    bluez_dbus_setter->SetBluetoothInputClient(
        std::unique_ptr<BluetoothInputClient>(new FakeBluetoothInputClient));
    bluez_dbus_setter->SetBluetoothAgentManagerClient(
        std::unique_ptr<BluetoothAgentManagerClient>(
            new FakeBluetoothAgentManagerClient));

    // Set up a PowerManagerClient instance for PerfProvider.
    chromeos::PowerManagerClient::InitializeFake();

    // Grab pointers to members of the thread manager for easier testing.
    fake_bluetooth_adapter_client_ = static_cast<FakeBluetoothAdapterClient*>(
        BluezDBusManager::Get()->GetBluetoothAdapterClient());
    fake_bluetooth_device_client_ = static_cast<FakeBluetoothDeviceClient*>(
        BluezDBusManager::Get()->GetBluetoothDeviceClient());

    chromeos::multidevice_setup::MultiDeviceSetupClientFactory::GetInstance()
        ->SetServiceIsNULLWhileTestingForTesting(false);
    auto fake_multidevice_setup_client = std::make_unique<
        chromeos::multidevice_setup::FakeMultiDeviceSetupClient>();
    fake_multidevice_setup_client_ = fake_multidevice_setup_client.get();
    fake_multidevice_setup_client_impl_factory_ =
        std::make_unique<FakeMultiDeviceSetupClientImplFactory>(
            std::move(fake_multidevice_setup_client));
    chromeos::multidevice_setup::MultiDeviceSetupClientImpl::Factory::
        SetInstanceForTesting(
            fake_multidevice_setup_client_impl_factory_.get());

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    TestingProfile* profile =
        profile_manager_->CreateTestingProfile("test_name");
    profile_manager_->UpdateLastUser(profile);

    // Set statistic provider for hardware class tests.
    chromeos::system::StatisticsProvider::SetTestProvider(
        &fake_statistics_provider_);

    // Initialize the login state trackers.
    if (!chromeos::LoginState::IsInitialized())
      chromeos::LoginState::Initialize();
  }

  void TearDown() override {
    // Destroy the login state tracker if it was initialized.
    chromeos::LoginState::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
    chromeos::multidevice_setup::MultiDeviceSetupClientImpl::Factory::
        SetInstanceForTesting(nullptr);
    profile_manager_.reset();
  }

 protected:
  FakeBluetoothAdapterClient* fake_bluetooth_adapter_client_;
  FakeBluetoothDeviceClient* fake_bluetooth_device_client_;
  chromeos::multidevice_setup::FakeMultiDeviceSetupClient*
      fake_multidevice_setup_client_;
  base::test::ScopedFeatureList scoped_feature_list_;
  chromeos::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<FakeMultiDeviceSetupClientImplFactory>
      fake_multidevice_setup_client_impl_factory_;

 private:
  content::BrowserTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(ChromeOSMetricsProviderTest);
};

TEST_F(ChromeOSMetricsProviderTest, MultiProfileUserCount) {
  const AccountId account_id1(AccountId::FromUserEmail("user1@example.com"));
  const AccountId account_id2(AccountId::FromUserEmail("user2@example.com"));
  const AccountId account_id3(AccountId::FromUserEmail("user3@example.com"));

  // |scoped_enabler| takes over the lifetime of |user_manager|.
  chromeos::FakeChromeUserManager* user_manager =
      new chromeos::FakeChromeUserManager();
  user_manager::ScopedUserManager scoped_enabler(
      base::WrapUnique(user_manager));
  user_manager->AddKioskAppUser(account_id1);
  user_manager->AddKioskAppUser(account_id2);
  user_manager->AddKioskAppUser(account_id3);

  user_manager->LoginUser(account_id1);
  user_manager->LoginUser(account_id3);

  TestChromeOSMetricsProvider provider;
  provider.OnDidCreateMetricsLog();
  metrics::SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_EQ(2u, system_profile.multi_profile_user_count());
}

TEST_F(ChromeOSMetricsProviderTest, MultiProfileCountInvalidated) {
  const AccountId account_id1(AccountId::FromUserEmail("user1@example.com"));
  const AccountId account_id2(AccountId::FromUserEmail("user2@example.com"));
  const AccountId account_id3(AccountId::FromUserEmail("user3@example.com"));

  // |scoped_enabler| takes over the lifetime of |user_manager|.
  chromeos::FakeChromeUserManager* user_manager =
      new chromeos::FakeChromeUserManager();
  user_manager::ScopedUserManager scoped_enabler(
      base::WrapUnique(user_manager));
  user_manager->AddKioskAppUser(account_id1);
  user_manager->AddKioskAppUser(account_id2);
  user_manager->AddKioskAppUser(account_id3);

  user_manager->LoginUser(account_id1);

  TestChromeOSMetricsProvider provider;
  provider.OnDidCreateMetricsLog();

  metrics::SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_EQ(1u, system_profile.multi_profile_user_count());

  user_manager->LoginUser(account_id2);
  provider.ProvideSystemProfileMetrics(&system_profile);
  EXPECT_EQ(0u, system_profile.multi_profile_user_count());
}

TEST_F(ChromeOSMetricsProviderTest, BluetoothHardwareDisabled) {
  TestChromeOSMetricsProvider provider;
  provider.OnDidCreateMetricsLog();
  metrics::SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);

  EXPECT_TRUE(system_profile.has_hardware());
  EXPECT_TRUE(system_profile.hardware().has_bluetooth());

  EXPECT_TRUE(system_profile.hardware().bluetooth().is_present());
  EXPECT_FALSE(system_profile.hardware().bluetooth().is_enabled());
}

TEST_F(ChromeOSMetricsProviderTest, BluetoothHardwareEnabled) {
  FakeBluetoothAdapterClient::Properties* properties =
      fake_bluetooth_adapter_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath));
  properties->powered.ReplaceValue(true);

  TestChromeOSMetricsProvider provider;
  metrics::SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);

  EXPECT_TRUE(system_profile.has_hardware());
  EXPECT_TRUE(system_profile.hardware().has_bluetooth());

  EXPECT_TRUE(system_profile.hardware().bluetooth().is_present());
  EXPECT_TRUE(system_profile.hardware().bluetooth().is_enabled());
}

TEST_F(ChromeOSMetricsProviderTest, BluetoothPairedDevices) {
  // The fake bluetooth adapter class already claims to be paired with two
  // device when initialized. Add a third and fourth fake device to it so we
  // can test the cases where a device is not paired (LE device, generally)
  // and a device that does not have Device ID information.
  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kRequestPinCodePath));

  fake_bluetooth_device_client_->CreateDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(FakeBluetoothDeviceClient::kConfirmPasskeyPath));

  FakeBluetoothDeviceClient::Properties* properties =
      fake_bluetooth_device_client_->GetProperties(
          dbus::ObjectPath(FakeBluetoothDeviceClient::kConfirmPasskeyPath));
  properties->paired.ReplaceValue(true);

  TestChromeOSMetricsProvider provider;
  provider.OnDidCreateMetricsLog();
  metrics::SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);

  ASSERT_TRUE(system_profile.has_hardware());
  ASSERT_TRUE(system_profile.hardware().has_bluetooth());

  // Only three of the devices should appear.
  EXPECT_EQ(3, system_profile.hardware().bluetooth().paired_device_size());

  typedef metrics::SystemProfileProto::Hardware::Bluetooth::PairedDevice
      PairedDevice;
  // As BluetoothAdapter keeps the device list without ordering,
  // it's not appropriate to use fixed positional indices to index into the
  // system_profile.hardware().bluetooth().paired_device list.
  // Instead, directly find the two devices we're interested in.
  PairedDevice device1;
  PairedDevice device2;
  for (int i = 0;
       i < system_profile.hardware().bluetooth().paired_device_size(); ++i) {
    const PairedDevice& device =
        system_profile.hardware().bluetooth().paired_device(i);
    if (device.bluetooth_class() ==
            FakeBluetoothDeviceClient::kPairedDeviceClass &&
        device.vendor_prefix() == 0x001122U) {
      // Found the Paired Device object.
      device1 = device;
    } else if (device.bluetooth_class() ==
               FakeBluetoothDeviceClient::kConfirmPasskeyClass) {
      // Found the Confirm Passkey object.
      device2 = device;
    }
  }

  // The Paired Device object, complete with parsed Device ID information.
  EXPECT_EQ(FakeBluetoothDeviceClient::kPairedDeviceClass,
            device1.bluetooth_class());
  EXPECT_EQ(PairedDevice::DEVICE_COMPUTER, device1.type());
  EXPECT_EQ(0x001122U, device1.vendor_prefix());
  EXPECT_EQ(PairedDevice::VENDOR_ID_USB, device1.vendor_id_source());
  EXPECT_EQ(0x05ACU, device1.vendor_id());
  EXPECT_EQ(0x030DU, device1.product_id());
  EXPECT_EQ(0x0306U, device1.device_id());

  // The Confirm Passkey object, this has no Device ID information.
  EXPECT_EQ(FakeBluetoothDeviceClient::kConfirmPasskeyClass,
            device2.bluetooth_class());
  EXPECT_EQ(PairedDevice::DEVICE_PHONE, device2.type());
  EXPECT_EQ(0x207D74U, device2.vendor_prefix());
  EXPECT_EQ(PairedDevice::VENDOR_ID_UNKNOWN, device2.vendor_id_source());
}

TEST_F(ChromeOSMetricsProviderTest, NoLinkedAndroidPhone) {
  fake_multidevice_setup_client_->SetHostStatusWithDevice(std::make_pair(
      chromeos::multidevice_setup::mojom::HostStatus::kNoEligibleHosts,
      base::nullopt /* host_device */));

  TestChromeOSMetricsProvider provider;
  metrics::SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);

  EXPECT_FALSE(system_profile.has_linked_android_phone_data());
}

TEST_F(ChromeOSMetricsProviderTest, HasLinkedAndroidPhoneAndEnabledFeatures) {
  fake_multidevice_setup_client_->SetHostStatusWithDevice(std::make_pair(
      chromeos::multidevice_setup::mojom::HostStatus::kHostVerified,
      chromeos::multidevice::CreateRemoteDeviceRefForTest()));
  fake_multidevice_setup_client_->SetFeatureState(
      chromeos::multidevice_setup::mojom::Feature::kInstantTethering,
      chromeos::multidevice_setup::mojom::FeatureState::kEnabledByUser);
  fake_multidevice_setup_client_->SetFeatureState(
      chromeos::multidevice_setup::mojom::Feature::kSmartLock,
      chromeos::multidevice_setup::mojom::FeatureState::kEnabledByUser);
  fake_multidevice_setup_client_->SetFeatureState(
      chromeos::multidevice_setup::mojom::Feature::kMessages,
      chromeos::multidevice_setup::mojom::FeatureState::kFurtherSetupRequired);

  TestChromeOSMetricsProvider provider;
  metrics::SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);

  EXPECT_TRUE(system_profile.has_linked_android_phone_data());
  EXPECT_TRUE(
      system_profile.linked_android_phone_data().has_phone_model_name_hash());
  EXPECT_TRUE(system_profile.linked_android_phone_data()
                  .is_instant_tethering_enabled());
  EXPECT_TRUE(
      system_profile.linked_android_phone_data().is_smartlock_enabled());
  EXPECT_FALSE(
      system_profile.linked_android_phone_data().is_messages_enabled());
}

TEST_F(ChromeOSMetricsProviderTest, DisableUmaShortHwClass) {
  const std::string expected_full_hw_class = "feature_disabled";
  scoped_feature_list_.InitAndDisableFeature(features::kUmaShortHWClass);
  fake_statistics_provider_.SetMachineStatistic("hardware_class",
                                                expected_full_hw_class);

  TestChromeOSMetricsProvider provider;
  provider.OnDidCreateMetricsLog();
  metrics::SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);

  ASSERT_TRUE(system_profile.has_hardware());
  std::string proto_full_hw_class =
      system_profile.hardware().full_hardware_class();

  // If disabled, the two hardware classes should be equal to each other.
  EXPECT_EQ(system_profile.hardware().hardware_class(), proto_full_hw_class);
  EXPECT_EQ(expected_full_hw_class, proto_full_hw_class);
}

TEST_F(ChromeOSMetricsProviderTest, EnableUmaShortHwClass) {
  const std::string expected_full_hw_class = "feature_enabled";
  scoped_feature_list_.InitAndEnableFeature(features::kUmaShortHWClass);
  fake_statistics_provider_.SetMachineStatistic("hardware_class",
                                                expected_full_hw_class);

  TestChromeOSMetricsProvider provider;
  provider.OnDidCreateMetricsLog();
  metrics::SystemProfileProto system_profile;
  provider.ProvideSystemProfileMetrics(&system_profile);

  ASSERT_TRUE(system_profile.has_hardware());
  std::string proto_full_hw_class =
      system_profile.hardware().full_hardware_class();

  // If enabled, the two hardware strings should be different.
  EXPECT_NE(system_profile.hardware().hardware_class(), proto_full_hw_class);
  EXPECT_EQ(expected_full_hw_class, proto_full_hw_class);
}
