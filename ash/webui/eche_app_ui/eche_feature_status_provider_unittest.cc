// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_feature_status_provider.h"

#include <memory>
#include <vector>

#include "base/test/task_environment.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/phonehub/fake_phone_hub_manager.h"
#include "chromeos/ash/components/phonehub/phone_hub_manager.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_connection_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace eche_app {
namespace {

using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;
using multidevice_setup::mojom::HostStatus;

multidevice::RemoteDeviceRef CreateLocalDevice(bool supports_eche_client) {
  multidevice::RemoteDeviceRefBuilder builder;
  builder.SetSoftwareFeatureState(
      multidevice::SoftwareFeature::kEcheClient,
      supports_eche_client ? multidevice::SoftwareFeatureState::kSupported
                           : multidevice::SoftwareFeatureState::kNotSupported);
  return builder.Build();
}

multidevice::RemoteDeviceRef CreatePhoneDevice(bool eche_host_supported,
                                               bool eche_host_enabled) {
  multidevice::SoftwareFeatureState state =
      multidevice::SoftwareFeatureState::kNotSupported;
  if (eche_host_enabled) {
    state = multidevice::SoftwareFeatureState::kEnabled;
  } else if (eche_host_supported) {
    state = multidevice::SoftwareFeatureState::kSupported;
  }
  multidevice::RemoteDeviceRefBuilder builder;
  builder.SetSoftwareFeatureState(
      multidevice::SoftwareFeature::kBetterTogetherHost,
      multidevice::SoftwareFeatureState::kSupported);
  builder.SetSoftwareFeatureState(multidevice::SoftwareFeature::kEcheHost,
                                  state);
  return builder.Build();
}

class FakeObserver : public FeatureStatusProvider::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_calls() const { return num_calls_; }

  // FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override { ++num_calls_; }

 private:
  size_t num_calls_ = 0;
};

}  // namespace

class EcheFeatureStatusProviderTest : public testing::Test {
 protected:
  EcheFeatureStatusProviderTest() = default;
  EcheFeatureStatusProviderTest(const EcheFeatureStatusProviderTest&) = delete;
  EcheFeatureStatusProviderTest& operator=(
      const EcheFeatureStatusProviderTest&) = delete;
  ~EcheFeatureStatusProviderTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_phone_hub_manager.fake_feature_status_provider()->SetStatus(
        phonehub::FeatureStatus::kEnabledAndConnected);
    eche_connection_status_handler_ =
        std::make_unique<EcheConnectionStatusHandler>();
    provider_ = std::make_unique<EcheFeatureStatusProvider>(
        &fake_phone_hub_manager, &fake_multidevice_setup_client_,
        &fake_connection_manager_, eche_connection_status_handler_.get());
    provider_->AddObserver(&fake_observer_);
  }

  void SetDeviceSyncClientReady() { fake_device_sync_client_.NotifyReady(); }

  void SetSyncedDevices(
      const std::optional<multidevice::RemoteDeviceRef>& local_device,
      const std::vector<std::optional<multidevice::RemoteDeviceRef>>
          phone_devices) {
    fake_device_sync_client_.set_local_device_metadata(local_device);

    multidevice::RemoteDeviceRefList synced_devices;
    if (local_device)
      synced_devices.push_back(*local_device);
    for (const auto& phone_device : phone_devices) {
      if (phone_device)
        synced_devices.push_back(*phone_device);
    }
    fake_device_sync_client_.set_synced_devices(synced_devices);

    fake_device_sync_client_.NotifyNewDevicesSynced();
  }

  void SetEligibleSyncedDevices() {
    SetSyncedDevices(CreateLocalDevice(/*supports_eche_client=*/true),
                     {CreatePhoneDevice(/*eche_host_supported=*/true,
                                        /*eche_host_enabled=*/true)});
  }

  void SetMultiDeviceState(HostStatus host_status,
                           FeatureState feature_state,
                           bool eche_host_supported,
                           bool eche_host_enabled) {
    fake_multidevice_setup_client_.SetHostStatusWithDevice(std::make_pair(
        host_status,
        CreatePhoneDevice(eche_host_supported, eche_host_enabled)));
    fake_multidevice_setup_client_.SetFeatureState(Feature::kEche,
                                                   feature_state);
  }

  void SetHostStatusWithDevice(
      HostStatus host_status,
      const std::optional<multidevice::RemoteDeviceRef>& host_device) {
    fake_multidevice_setup_client_.SetHostStatusWithDevice(
        std::make_pair(host_status, host_device));
  }

  void SetHostStatus(HostStatus host_status) {
    fake_multidevice_setup_client_.SetHostStatusWithDevice(
        std::make_pair(host_status, std::nullopt /* host_device */));
  }

  void SetConnectionStatus(secure_channel::ConnectionManager::Status status) {
    fake_connection_manager_.SetStatus(status);
  }

  void SetFeatureState(FeatureState feature_state) {
    fake_multidevice_setup_client_.SetFeatureState(Feature::kEche,
                                                   feature_state);
  }

  void SetPhoneHubFeatureStatus(phonehub::FeatureStatus feature_status) {
    fake_phone_hub_manager.fake_feature_status_provider()->SetStatus(
        feature_status);
  }

  FeatureStatus GetStatus() const { return provider_->GetStatus(); }

  size_t GetNumObserverCalls() const { return fake_observer_.num_calls(); }

 private:
  base::test::TaskEnvironment task_environment_;

  device_sync::FakeDeviceSyncClient fake_device_sync_client_;
  multidevice_setup::FakeMultiDeviceSetupClient fake_multidevice_setup_client_;
  secure_channel::FakeConnectionManager fake_connection_manager_;
  std::unique_ptr<EcheConnectionStatusHandler> eche_connection_status_handler_;

  phonehub::FakePhoneHubManager fake_phone_hub_manager;
  FakeObserver fake_observer_;
  std::unique_ptr<EcheFeatureStatusProvider> provider_;
};

// Tests conditions for kIneligible status, including missing local
// device and/or phone and various missing properties of these devices.
TEST_F(EcheFeatureStatusProviderTest, IneligibleForFeature) {
  SetSyncedDevices(CreateLocalDevice(/*supports_eche_client=*/true),
                   {CreatePhoneDevice(/*eche_host_supported=*/true,
                                      /*eche_host_enabled=*/true)});
  EXPECT_EQ(FeatureStatus::kIneligible, GetStatus());

  SetDeviceSyncClientReady();
  SetSyncedDevices(/*local_device=*/std::nullopt,
                   /*phone_devices=*/{std::nullopt});
  EXPECT_EQ(FeatureStatus::kIneligible, GetStatus());

  SetSyncedDevices(CreateLocalDevice(/*supports_eche_client=*/false),
                   /*phone_devices=*/{std::nullopt});
  EXPECT_EQ(FeatureStatus::kIneligible, GetStatus());

  SetSyncedDevices(CreateLocalDevice(/*supports_eche_client=*/true),
                   /*phone_devices=*/{std::nullopt});
  EXPECT_EQ(FeatureStatus::kIneligible, GetStatus());

  SetSyncedDevices(CreateLocalDevice(/*supports_eche_client=*/true),
                   {CreatePhoneDevice(/*eche_host_supported=*/false,
                                      /*eche_host_enabled=*/false)});
  EXPECT_EQ(FeatureStatus::kIneligible, GetStatus());

  SetSyncedDevices(CreateLocalDevice(/*supports_eche_client=*/true),
                   {CreatePhoneDevice(/*eche_host_supported=*/true,
                                      /*eche_host_enabled=*/false)});
  EXPECT_EQ(FeatureStatus::kIneligible, GetStatus());

  // Set all properties to true so that there is an eligible phone. Since
  // |fake_multidevice_setup_client_| defaults to kProhibitedByPolicy, the
  // status should still be kIneligible.
  SetSyncedDevices(CreateLocalDevice(/*supports_eche_client=*/true),
                   {CreatePhoneDevice(/*eche_host_supported=*/true,
                                      /*eche_host_enabled=*/true)});
  EXPECT_EQ(FeatureStatus::kIneligible, GetStatus());
}

TEST_F(EcheFeatureStatusProviderTest, NoEligiblePhones) {
  SetDeviceSyncClientReady();
  SetMultiDeviceState(HostStatus::kNoEligibleHosts,
                      FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts,
                      /*eche_host_supported=*/true,
                      /*eche_host_enabled=*/false);
  EXPECT_EQ(FeatureStatus::kIneligible, GetStatus());
}

TEST_F(EcheFeatureStatusProviderTest, Disabled) {
  SetDeviceSyncClientReady();
  SetEligibleSyncedDevices();

  SetMultiDeviceState(HostStatus::kHostVerified, FeatureState::kDisabledByUser,
                      /*eche_host_supported=*/true,
                      /*eche_host_enabled=*/true);
  EXPECT_EQ(FeatureStatus::kDisabled, GetStatus());

  SetMultiDeviceState(HostStatus::kHostVerified,
                      FeatureState::kNotSupportedByChromebook,
                      /*eche_host_supported=*/true,
                      /*eche_host_enabled=*/true);
  EXPECT_EQ(FeatureStatus::kIneligible, GetStatus());

  SetMultiDeviceState(HostStatus::kHostVerified,
                      FeatureState::kUnavailableTopLevelFeatureDisabled,
                      /*eche_host_supported=*/true,
                      /*eche_host_enabled=*/true);
  EXPECT_EQ(FeatureStatus::kDisabled, GetStatus());
}

TEST_F(EcheFeatureStatusProviderTest, TransitionBetweenAllStatuses) {
  SetDeviceSyncClientReady();
  EXPECT_EQ(FeatureStatus::kIneligible, GetStatus());

  SetMultiDeviceState(HostStatus::kNoEligibleHosts,
                      FeatureState::kUnavailableNoVerifiedHost_NoEligibleHosts,
                      /*eche_host_supported=*/true,
                      /*eche_host_enabled=*/true);
  EXPECT_EQ(FeatureStatus::kIneligible, GetStatus());
  EXPECT_EQ(0u, GetNumObserverCalls());

  SetMultiDeviceState(
      HostStatus::kEligibleHostExistsButNoHostSet,
      FeatureState::kUnavailableNoVerifiedHost_HostExistsButNotSetAndVerified,
      /*eche_host_supported=*/true,
      /*eche_host_enabled=*/true);
  SetEligibleSyncedDevices();
  EXPECT_EQ(FeatureStatus::kIneligible, GetStatus());
  EXPECT_EQ(0u, GetNumObserverCalls());

  SetFeatureState(FeatureState::kNotSupportedByPhone);
  EXPECT_EQ(FeatureStatus::kIneligible, GetStatus());
  EXPECT_EQ(0u, GetNumObserverCalls());

  SetMultiDeviceState(HostStatus::kHostSetButNotYetVerified,
                      FeatureState::kNotSupportedByPhone,
                      /*eche_host_supported=*/true,
                      /*eche_host_enabled=*/true);
  EXPECT_EQ(FeatureStatus::kIneligible, GetStatus());
  EXPECT_EQ(0u, GetNumObserverCalls());

  SetMultiDeviceState(HostStatus::kHostVerified, FeatureState::kDisabledByUser,
                      /*eche_host_supported=*/true,
                      /*eche_host_enabled=*/true);
  EXPECT_EQ(FeatureStatus::kDisabled, GetStatus());
  EXPECT_EQ(1u, GetNumObserverCalls());

  SetMultiDeviceState(HostStatus::kHostVerified, FeatureState::kEnabledByUser,
                      /*eche_host_supported=*/true,
                      /*eche_host_enabled=*/true);
  EXPECT_EQ(2u, GetNumObserverCalls());
  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnecting);
  EXPECT_EQ(FeatureStatus::kConnecting, GetStatus());
  EXPECT_EQ(3u, GetNumObserverCalls());

  SetConnectionStatus(secure_channel::ConnectionManager::Status::kConnected);
  EXPECT_EQ(FeatureStatus::kConnected, GetStatus());
  EXPECT_EQ(4u, GetNumObserverCalls());

  SetConnectionStatus(secure_channel::ConnectionManager::Status::kDisconnected);
  EXPECT_EQ(FeatureStatus::kDisconnected, GetStatus());
  EXPECT_EQ(5u, GetNumObserverCalls());
}

TEST_F(EcheFeatureStatusProviderTest,
       TransitionWhenPhoneHubFeatureStatusChanged) {
  SetDeviceSyncClientReady();
  SetPhoneHubFeatureStatus(phonehub::FeatureStatus::kNotEligibleForFeature);
  EXPECT_EQ(FeatureStatus::kDependentFeature, GetStatus());

  SetPhoneHubFeatureStatus(phonehub::FeatureStatus::kEligiblePhoneButNotSetUp);
  EXPECT_EQ(FeatureStatus::kDependentFeature, GetStatus());

  SetPhoneHubFeatureStatus(
      phonehub::FeatureStatus::kPhoneSelectedAndPendingSetup);
  EXPECT_EQ(FeatureStatus::kDependentFeature, GetStatus());

  SetPhoneHubFeatureStatus(phonehub::FeatureStatus::kDisabled);
  EXPECT_EQ(FeatureStatus::kDependentFeature, GetStatus());

  SetPhoneHubFeatureStatus(phonehub::FeatureStatus::kUnavailableBluetoothOff);
  EXPECT_EQ(FeatureStatus::kDependentFeature, GetStatus());

  SetPhoneHubFeatureStatus(phonehub::FeatureStatus::kEnabledButDisconnected);
  EXPECT_EQ(FeatureStatus::kDependentFeaturePending, GetStatus());

  SetPhoneHubFeatureStatus(phonehub::FeatureStatus::kEnabledAndConnecting);
  EXPECT_EQ(FeatureStatus::kDependentFeaturePending, GetStatus());

  SetPhoneHubFeatureStatus(phonehub::FeatureStatus::kLockOrSuspended);
  EXPECT_EQ(FeatureStatus::kDependentFeaturePending, GetStatus());
}

}  // namespace eche_app
}  // namespace ash
