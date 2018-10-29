// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/tether/tether_service.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/ash/network/tether_notification_presenter.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/components/tether/fake_notification_presenter.h"
#include "chromeos/components/tether/fake_tether_component.h"
#include "chromeos/components/tether/fake_tether_host_fetcher.h"
#include "chromeos/components/tether/tether_component_impl.h"
#include "chromeos/components/tether/tether_host_fetcher_impl.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "chromeos/dbus/fake_shill_manager_client.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "chromeos/dbus/power_manager_client.h"
#include "chromeos/dbus/shill_device_client.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test.h"
#include "chromeos/network/network_type_pattern.h"
#include "chromeos/services/device_sync/public/cpp/device_sync_client_impl.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client_impl.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "chromeos/services/secure_channel/public/cpp/client/secure_channel_client_impl.h"
#include "components/cryptauth/cryptauth_device_manager.h"
#include "components/cryptauth/cryptauth_enroller.h"
#include "components/cryptauth/cryptauth_enrollment_manager.h"
#include "components/cryptauth/fake_cryptauth_enrollment_manager.h"
#include "components/cryptauth/fake_cryptauth_service.h"
#include "components/cryptauth/fake_remote_device_provider.h"
#include "components/cryptauth/remote_device_provider_impl.h"
#include "components/cryptauth/remote_device_test_util.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

using testing::Invoke;
using testing::NiceMock;
using testing::Return;

namespace {

const char kTestUserPrivateKey[] = "kTestUserPrivateKey";
const size_t kNumTestDevices = 5;

cryptauth::RemoteDeviceRefList CreateTestDevices() {
  cryptauth::RemoteDeviceRefList list;
  for (size_t i = 0; i < kNumTestDevices; ++i) {
    list.push_back(cryptauth::RemoteDeviceRefBuilder()
                       .SetSupportsMobileHotspot(true)
                       .SetSoftwareFeatureState(
                           cryptauth::SoftwareFeature::BETTER_TOGETHER_HOST,
                           cryptauth::SoftwareFeatureState::kSupported)
                       .Build());
  }
  return list;
}

class MockExtendedBluetoothAdapter : public device::MockBluetoothAdapter {
 public:
  void SetAdvertisingInterval(
      const base::TimeDelta& min,
      const base::TimeDelta& max,
      const base::Closure& callback,
      const AdvertisementErrorCallback& error_callback) override {
    if (is_ble_advertising_supported_) {
      callback.Run();
    } else {
      error_callback.Run(device::BluetoothAdvertisement::ErrorCode::
                             ERROR_INVALID_ADVERTISEMENT_INTERVAL);
    }
  }

  void set_is_ble_advertising_supported(bool is_ble_advertising_supported) {
    is_ble_advertising_supported_ = is_ble_advertising_supported;
  }

 protected:
  ~MockExtendedBluetoothAdapter() override {}

 private:
  bool is_ble_advertising_supported_ = true;
};

class TestTetherService : public TetherService {
 public:
  TestTetherService(
      Profile* profile,
      chromeos::PowerManagerClient* power_manager_client,
      cryptauth::CryptAuthService* cryptauth_service,
      chromeos::device_sync::DeviceSyncClient* device_sync_client,
      chromeos::secure_channel::SecureChannelClient* secure_channel_client,
      chromeos::multidevice_setup::MultiDeviceSetupClient*
          multidevice_setup_client,
      chromeos::NetworkStateHandler* network_state_handler,
      session_manager::SessionManager* session_manager)
      : TetherService(profile,
                      power_manager_client,
                      cryptauth_service,
                      device_sync_client,
                      secure_channel_client,
                      multidevice_setup_client,
                      network_state_handler,
                      session_manager) {}
  ~TestTetherService() override {}

  int updated_technology_state_count() {
    return updated_technology_state_count_;
  }

 protected:
  void UpdateTetherTechnologyState() override {
    updated_technology_state_count_++;
    TetherService::UpdateTetherTechnologyState();
  }

 private:
  int updated_technology_state_count_ = 0;
};

class FakeTetherComponentWithDestructorCallback
    : public chromeos::tether::FakeTetherComponent {
 public:
  FakeTetherComponentWithDestructorCallback(
      const base::Closure& destructor_callback)
      : FakeTetherComponent(false /* has_asynchronous_shutdown */),
        destructor_callback_(destructor_callback) {}

  ~FakeTetherComponentWithDestructorCallback() override {
    destructor_callback_.Run();
  }

 private:
  base::Closure destructor_callback_;
};

class TestTetherComponentFactory final
    : public chromeos::tether::TetherComponentImpl::Factory {
 public:
  TestTetherComponentFactory() {}

  // Returns nullptr if no TetherComponent has been created or if the last one
  // that was created has already been deleted.
  FakeTetherComponentWithDestructorCallback* active_tether_component() {
    return active_tether_component_;
  }

  // chromeos::tether::TetherComponentImpl::Factory:
  std::unique_ptr<chromeos::tether::TetherComponent> BuildInstance(
      cryptauth::CryptAuthService* cryptauth_service,
      chromeos::device_sync::DeviceSyncClient* device_sync_client,
      chromeos::secure_channel::SecureChannelClient* secure_channel_client,
      chromeos::tether::TetherHostFetcher* tether_host_fetcher,
      chromeos::tether::NotificationPresenter* notification_presenter,
      chromeos::tether::GmsCoreNotificationsStateTrackerImpl*
          gms_core_notifications_state_tracker,
      PrefService* pref_service,
      chromeos::NetworkStateHandler* network_state_handler,
      chromeos::ManagedNetworkConfigurationHandler*
          managed_network_configuration_handler,
      chromeos::NetworkConnect* network_connect,
      chromeos::NetworkConnectionHandler* network_connection_handler,
      scoped_refptr<device::BluetoothAdapter> adapter,
      session_manager::SessionManager* session_manager) override {
    active_tether_component_ = new FakeTetherComponentWithDestructorCallback(
        base::Bind(&TestTetherComponentFactory::OnActiveTetherComponentDeleted,
                   base::Unretained(this)));
    was_tether_component_active_ = true;
    return base::WrapUnique(active_tether_component_);
  }

  bool was_tether_component_active() { return was_tether_component_active_; }

  const chromeos::tether::TetherComponent::ShutdownReason&
  last_shutdown_reason() {
    return last_shutdown_reason_;
  }

 private:
  void OnActiveTetherComponentDeleted() {
    last_shutdown_reason_ = *active_tether_component_->last_shutdown_reason();
    active_tether_component_ = nullptr;
  }

  FakeTetherComponentWithDestructorCallback* active_tether_component_ = nullptr;
  bool was_tether_component_active_ = false;
  chromeos::tether::TetherComponent::ShutdownReason last_shutdown_reason_;
};

class FakeRemoteDeviceProviderFactory
    : public cryptauth::RemoteDeviceProviderImpl::Factory {
 public:
  FakeRemoteDeviceProviderFactory() = default;
  ~FakeRemoteDeviceProviderFactory() override = default;

  // cryptauth::RemoteDeviceProviderImpl::Factory:
  std::unique_ptr<cryptauth::RemoteDeviceProvider> BuildInstance(
      cryptauth::CryptAuthDeviceManager* device_manager,
      const std::string& user_id,
      const std::string& user_private_key) override {
    return std::make_unique<cryptauth::FakeRemoteDeviceProvider>();
  }
};

class FakeTetherHostFetcherFactory
    : public chromeos::tether::TetherHostFetcherImpl::Factory {
 public:
  FakeTetherHostFetcherFactory(
      const cryptauth::RemoteDeviceRefList& initial_devices)
      : initial_devices_(initial_devices) {}
  virtual ~FakeTetherHostFetcherFactory() = default;

  chromeos::tether::FakeTetherHostFetcher* last_created() {
    return last_created_;
  }

  void SetNoInitialDevices() { initial_devices_.clear(); }

  // chromeos::tether::TetherHostFetcherImpl::Factory :
  std::unique_ptr<chromeos::tether::TetherHostFetcher> BuildInstance(
      cryptauth::RemoteDeviceProvider* remote_device_provider,
      chromeos::device_sync::DeviceSyncClient* device_sync_client,
      chromeos::multidevice_setup::MultiDeviceSetupClient*
          multidevice_setup_client) override {
    last_created_ =
        new chromeos::tether::FakeTetherHostFetcher(initial_devices_);
    return base::WrapUnique(last_created_);
  }

 private:
  cryptauth::RemoteDeviceRefList initial_devices_;
  chromeos::tether::FakeTetherHostFetcher* last_created_ = nullptr;
};

class FakeDeviceSyncClientImplFactory
    : public chromeos::device_sync::DeviceSyncClientImpl::Factory {
 public:
  FakeDeviceSyncClientImplFactory() = default;

  ~FakeDeviceSyncClientImplFactory() override = default;

  // chromeos::device_sync::DeviceSyncClientImpl::Factory:
  std::unique_ptr<chromeos::device_sync::DeviceSyncClient> BuildInstance(
      service_manager::Connector* connector) override {
    auto fake_device_sync_client =
        std::make_unique<chromeos::device_sync::FakeDeviceSyncClient>();
    fake_device_sync_client->NotifyReady();
    return fake_device_sync_client;
  }
};

class FakeSecureChannelClientImplFactory
    : public chromeos::secure_channel::SecureChannelClientImpl::Factory {
 public:
  FakeSecureChannelClientImplFactory() = default;

  ~FakeSecureChannelClientImplFactory() override = default;

  // chromeos::secure_channel::SecureChannelClientImpl::Factory:
  std::unique_ptr<chromeos::secure_channel::SecureChannelClient> BuildInstance(
      service_manager::Connector* connector,
      scoped_refptr<base::TaskRunner> task_runner) override {
    return std::make_unique<
        chromeos::secure_channel::FakeSecureChannelClient>();
  }
};

class FakeMultiDeviceSetupClientImplFactory
    : public chromeos::multidevice_setup::MultiDeviceSetupClientImpl::Factory {
 public:
  FakeMultiDeviceSetupClientImplFactory() = default;

  ~FakeMultiDeviceSetupClientImplFactory() override = default;

  // chromeos::multidevice_setup::MultiDeviceSetupClientImpl::Factory:
  std::unique_ptr<chromeos::multidevice_setup::MultiDeviceSetupClient>
  BuildInstance(service_manager::Connector* connector) override {
    auto fake_multidevice_setup_client = std::make_unique<
        chromeos::multidevice_setup::FakeMultiDeviceSetupClient>();
    fake_multidevice_setup_client_ = fake_multidevice_setup_client.get();
    return fake_multidevice_setup_client;
  }

  chromeos::multidevice_setup::FakeMultiDeviceSetupClient*
  fake_multidevice_setup_client() {
    return fake_multidevice_setup_client_;
  }

 private:
  chromeos::multidevice_setup::FakeMultiDeviceSetupClient*
      fake_multidevice_setup_client_;
};

}  // namespace

class TetherServiceTest : public chromeos::NetworkStateTest {
 protected:
  TetherServiceTest()
      : NetworkStateTest(), test_devices_(CreateTestDevices()) {}
  ~TetherServiceTest() override {}

  void SetUp() override {
    fake_notification_presenter_ = nullptr;
    mock_timer_ = nullptr;

    chromeos::DBusThreadManager::Initialize();
    chromeos::NetworkStateTest::SetUp();

    chromeos::NetworkConnect::Initialize(nullptr);
    chromeos::NetworkHandler::Initialize();

    TestingProfile::Builder builder;
    profile_ = builder.Build();

    fake_chrome_user_manager_ = new chromeos::FakeChromeUserManager();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(fake_chrome_user_manager_));

    fake_power_manager_client_ =
        std::make_unique<chromeos::FakePowerManagerClient>();

    fake_device_sync_client_ =
        std::make_unique<chromeos::device_sync::FakeDeviceSyncClient>();
    fake_device_sync_client_->NotifyReady();

    fake_device_sync_client_impl_factory_ =
        std::make_unique<FakeDeviceSyncClientImplFactory>();
    chromeos::device_sync::DeviceSyncClientImpl::Factory::SetInstanceForTesting(
        fake_device_sync_client_impl_factory_.get());

    fake_secure_channel_client_ =
        std::make_unique<chromeos::secure_channel::FakeSecureChannelClient>();
    fake_secure_channel_client_impl_factory_ =
        std::make_unique<FakeSecureChannelClientImplFactory>();
    chromeos::secure_channel::SecureChannelClientImpl::Factory::
        SetInstanceForTesting(fake_secure_channel_client_impl_factory_.get());

    fake_multidevice_setup_client_ = std::make_unique<
        chromeos::multidevice_setup::FakeMultiDeviceSetupClient>();
    fake_multidevice_setup_client_impl_factory_ =
        std::make_unique<FakeMultiDeviceSetupClientImplFactory>();
    chromeos::multidevice_setup::MultiDeviceSetupClientImpl::Factory::
        SetInstanceForTesting(
            fake_multidevice_setup_client_impl_factory_.get());
    initial_feature_state_ =
        chromeos::multidevice_setup::mojom::FeatureState::kEnabledByUser;

    fake_cryptauth_service_ =
        std::make_unique<cryptauth::FakeCryptAuthService>();
    fake_enrollment_manager_ =
        std::make_unique<cryptauth::FakeCryptAuthEnrollmentManager>();
    fake_enrollment_manager_->set_user_private_key(kTestUserPrivateKey);
    fake_cryptauth_service_->set_cryptauth_enrollment_manager(
        fake_enrollment_manager_.get());

    mock_adapter_ =
        base::MakeRefCounted<NiceMock<MockExtendedBluetoothAdapter>>();
    SetIsBluetoothPowered(true);
    is_adapter_present_ = true;
    ON_CALL(*mock_adapter_, IsPresent())
        .WillByDefault(Invoke(this, &TetherServiceTest::IsBluetoothPresent));
    ON_CALL(*mock_adapter_, IsPowered())
        .WillByDefault(Invoke(this, &TetherServiceTest::IsBluetoothPowered));
    device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);

    test_tether_component_factory_ =
        base::WrapUnique(new TestTetherComponentFactory());
    chromeos::tether::TetherComponentImpl::Factory::SetInstanceForTesting(
        test_tether_component_factory_.get());
    shutdown_reason_verified_ = false;

    fake_remote_device_provider_factory_ =
        base::WrapUnique(new FakeRemoteDeviceProviderFactory());
    cryptauth::RemoteDeviceProviderImpl::Factory::SetInstanceForTesting(
        fake_remote_device_provider_factory_.get());

    fake_tether_host_fetcher_factory_ =
        base::WrapUnique(new FakeTetherHostFetcherFactory(test_devices_));
    chromeos::tether::TetherHostFetcherImpl::Factory::SetInstanceForTesting(
        fake_tether_host_fetcher_factory_.get());

    TestingBrowserProcess::GetGlobal()->SetLocalState(&local_pref_service_);
    RegisterLocalState(local_pref_service_.registry());
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);

    chromeos::device_sync::DeviceSyncClientImpl::Factory::SetInstanceForTesting(
        nullptr);
    chromeos::secure_channel::SecureChannelClientImpl::Factory::
        SetInstanceForTesting(nullptr);
    chromeos::multidevice_setup::MultiDeviceSetupClientImpl::Factory::
        SetInstanceForTesting(nullptr);

    ShutdownTetherService();

    if (tether_service_) {
      // As of crbug.com/798605, SHUT_DOWN should not be logged since it does not
      // contribute meaningful data.
      histogram_tester_.ExpectBucketCount(
          "InstantTethering.FeatureState",
          TetherService::TetherFeatureState::SHUT_DOWN, 0 /* count */);
      tether_service_.reset();
    }

    EXPECT_EQ(test_tether_component_factory_->was_tether_component_active(),
              shutdown_reason_verified_);

    chromeos::NetworkConnect::Shutdown();
    chromeos::NetworkHandler::Shutdown();

    ShutdownNetworkState();
    chromeos::NetworkStateTest::TearDown();
    chromeos::DBusThreadManager::Shutdown();
  }

  void SetMultiDeviceApi(bool enabled) {
    static const std::vector<base::Feature> kFeatures{
        chromeos::features::kMultiDeviceApi,
        chromeos::features::kEnableUnifiedMultiDeviceSetup};

    scoped_feature_list_.InitWithFeatures(
        (enabled ? kFeatures
                 : std::vector<base::Feature>() /* enable_features */),
        (enabled ? std::vector<base::Feature>()
                 : kFeatures /* disable_features */));
  }

  void SetPrimaryUserLoggedIn() {
    const AccountId account_id(
        AccountId::FromUserEmail(profile_->GetProfileUserName()));
    const user_manager::User* user =
        fake_chrome_user_manager_->AddPublicAccountUser(account_id);
    fake_chrome_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                            false /* browser_restart */,
                                            false /* is_child */);
  }

  void CreateTetherService(bool enable_multidevice_apis) {
    SetMultiDeviceApi(enable_multidevice_apis);

    fake_multidevice_setup_client_->SetFeatureState(
        chromeos::multidevice_setup::mojom::Feature::kInstantTethering,
        initial_feature_state_);

    tether_service_ = base::WrapUnique(new TestTetherService(
        profile_.get(), fake_power_manager_client_.get(),
        fake_cryptauth_service_.get(), fake_device_sync_client_.get(),
        fake_secure_channel_client_.get(), fake_multidevice_setup_client_.get(),
        network_state_handler(), nullptr /* session_manager */));

    fake_notification_presenter_ =
        new chromeos::tether::FakeNotificationPresenter();
    mock_timer_ = new base::MockOneShotTimer();
    tether_service_->SetTestDoubles(
        base::WrapUnique(fake_notification_presenter_),
        base::WrapUnique(mock_timer_));

    SetPrimaryUserLoggedIn();

    // Ensure that TetherService does not prematurely update its
    // TechnologyState before it fetches the BluetoothAdapter.
    EXPECT_EQ(
        chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
        network_state_handler()->GetTechnologyState(
            chromeos::NetworkTypePattern::Tether()));
    VerifyTetherActiveStatus(false /* expected_active */);

    if (base::FeatureList::IsEnabled(chromeos::features::kMultiDeviceApi) &&
        !fake_device_sync_client_->is_ready()) {
      return;
    }

    // Allow the posted task to fetch the BluetoothAdapter to finish.
    base::RunLoop().RunUntilIdle();
  }

  void ShutdownTetherService() {
    if (tether_service_)
      tether_service_->Shutdown();
  }

  void SetTetherTechnologyStateEnabled(bool enabled) {
    network_state_handler()->SetTetherTechnologyState(
        enabled
            ? chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED
            : chromeos::NetworkStateHandler::TechnologyState::
                  TECHNOLOGY_AVAILABLE);
  }

  void SetCellularTechnologyStateEnabled(bool enabled) {
    network_state_handler()->SetTechnologyEnabled(
        chromeos::NetworkTypePattern::Cellular(), enabled,
        chromeos::network_handler::ErrorCallback());
    base::RunLoop().RunUntilIdle();
  }

  void SetIsBluetoothPowered(bool powered) {
    is_adapter_powered_ = powered;
    for (auto& observer : mock_adapter_->GetObservers())
      observer.AdapterPoweredChanged(mock_adapter_.get(), powered);
  }

  void set_is_adapter_present(bool present) { is_adapter_present_ = present; }

  bool IsBluetoothPresent() { return is_adapter_present_; }
  bool IsBluetoothPowered() { return is_adapter_powered_; }

  void RemoveWifiFromSystem() {
    chromeos::DBusThreadManager::Get()
        ->GetShillManagerClient()
        ->GetTestInterface()
        ->RemoveTechnology(shill::kTypeWifi);
    base::RunLoop().RunUntilIdle();
  }

  void DisconnectDefaultShillNetworks() {
    const chromeos::NetworkState* default_state;
    while ((default_state = network_state_handler()->DefaultNetwork())) {
      SetServiceProperty(default_state->path(), shill::kStateProperty,
                         base::Value(shill::kStateIdle));
    }
  }

  void VerifyTetherFeatureStateRecorded(
      TetherService::TetherFeatureState expected_technology_state_and_reason,
      int expected_count) {
    histogram_tester_.ExpectBucketCount("InstantTethering.FeatureState",
                                        expected_technology_state_and_reason,
                                        expected_count);
  }

  void VerifyTetherActiveStatus(bool expected_active) {
    EXPECT_EQ(
        expected_active,
        test_tether_component_factory_->active_tether_component() != nullptr);
  }

  void VerifyLastShutdownReason(
      const chromeos::tether::TetherComponent::ShutdownReason&
          expected_shutdown_reason) {
    EXPECT_EQ(expected_shutdown_reason,
              test_tether_component_factory_->last_shutdown_reason());
    shutdown_reason_verified_ = true;
  }

  const cryptauth::RemoteDeviceRefList test_devices_;
  const content::TestBrowserThreadBundle thread_bundle_;
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<TestingProfile> profile_;
  chromeos::FakeChromeUserManager* fake_chrome_user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<chromeos::FakePowerManagerClient> fake_power_manager_client_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<TestTetherComponentFactory> test_tether_component_factory_;
  std::unique_ptr<FakeRemoteDeviceProviderFactory>
      fake_remote_device_provider_factory_;
  std::unique_ptr<FakeTetherHostFetcherFactory>
      fake_tether_host_fetcher_factory_;
  chromeos::tether::FakeNotificationPresenter* fake_notification_presenter_;
  base::MockOneShotTimer* mock_timer_;
  std::unique_ptr<chromeos::device_sync::FakeDeviceSyncClient>
      fake_device_sync_client_;
  std::unique_ptr<FakeDeviceSyncClientImplFactory>
      fake_device_sync_client_impl_factory_;
  std::unique_ptr<chromeos::secure_channel::FakeSecureChannelClient>
      fake_secure_channel_client_;
  std::unique_ptr<FakeSecureChannelClientImplFactory>
      fake_secure_channel_client_impl_factory_;
  std::unique_ptr<chromeos::multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;
  std::unique_ptr<FakeMultiDeviceSetupClientImplFactory>
      fake_multidevice_setup_client_impl_factory_;
  std::unique_ptr<cryptauth::FakeCryptAuthService> fake_cryptauth_service_;
  std::unique_ptr<cryptauth::FakeCryptAuthEnrollmentManager>
      fake_enrollment_manager_;

  chromeos::multidevice_setup::mojom::FeatureState initial_feature_state_;

  scoped_refptr<MockExtendedBluetoothAdapter> mock_adapter_;
  bool is_adapter_present_;
  bool is_adapter_powered_;
  bool shutdown_reason_verified_;

  // PrefService which contains the browser process' local storage.
  TestingPrefServiceSimple local_pref_service_;

  std::unique_ptr<TestTetherService> tether_service_;

  base::HistogramTester histogram_tester_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TetherServiceTest);
};

TEST_F(TetherServiceTest, TestShutdown) {
  CreateTetherService(false /* enable_multidevice_apis */);
  VerifyTetherActiveStatus(true /* expected_active */);

  ShutdownTetherService();

  // The TechnologyState should not have changed due to Shutdown() being called.
  // If it had changed, any settings UI that was previously open would have
  // shown visual jank.
  EXPECT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);
  VerifyLastShutdownReason(
      chromeos::tether::TetherComponent::ShutdownReason::USER_LOGGED_OUT);
}

TEST_F(TetherServiceTest, TestAsyncTetherShutdown) {
  CreateTetherService(false /* enable_multidevice_apis */);

  // Tether should be ENABLED, and there should be no AsyncShutdownTask.
  EXPECT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);

  // Use an asynchronous shutdown.
  test_tether_component_factory_->active_tether_component()
      ->set_has_asynchronous_shutdown(true);

  // Disable the Tether preference. This should trigger the asynchrnous
  // shutdown.
  SetTetherTechnologyStateEnabled(false);

  // Tether should be active, but shutting down.
  VerifyTetherActiveStatus(true /* expected_active */);
  EXPECT_EQ(
      chromeos::tether::TetherComponent::Status::SHUTTING_DOWN,
      test_tether_component_factory_->active_tether_component()->status());

  // Tether should be AVAILABLE.
  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));

  // Complete the shutdown process; TetherService should delete its
  // TetherComponent instance.
  test_tether_component_factory_->active_tether_component()
      ->FinishAsynchronousShutdown();
  VerifyTetherActiveStatus(false /* expected_active */);
  VerifyLastShutdownReason(
      chromeos::tether::TetherComponent::ShutdownReason::PREF_DISABLED);
}

TEST_F(TetherServiceTest, TestSuspend) {
  CreateTetherService(false /* enable_multidevice_apis */);
  VerifyTetherActiveStatus(true /* expected_active */);

  fake_power_manager_client_->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);

  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  fake_power_manager_client_->SendSuspendDone();

  EXPECT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);

  fake_power_manager_client_->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);

  VerifyTetherFeatureStateRecorded(TetherService::TetherFeatureState::SUSPENDED,
                                   2 /* expected_count */);
  VerifyLastShutdownReason(
      chromeos::tether::TetherComponent::ShutdownReason::USER_CLOSED_LID);
}

TEST_F(TetherServiceTest, TestDeviceSyncClientNotReady) {
  fake_device_sync_client_ =
      std::make_unique<chromeos::device_sync::FakeDeviceSyncClient>();

  CreateTetherService(true /* enable_multidevice_apis */);

  fake_device_sync_client_->NotifyReady();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);

  ShutdownTetherService();
  VerifyLastShutdownReason(
      chromeos::tether::TetherComponent::ShutdownReason::USER_LOGGED_OUT);
}

TEST_F(TetherServiceTest,
       TestMultiDeviceSetupClientInitiallyHasNoVerifiedHost) {
  fake_tether_host_fetcher_factory_->SetNoInitialDevices();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {chromeos::features::kMultiDeviceApi,
       chromeos::features::
           kEnableUnifiedMultiDeviceSetup} /* enabled_features */,
      {} /* disabled_features */);

  initial_feature_state_ = chromeos::multidevice_setup::mojom::FeatureState::
      kUnavailableNoVerifiedHost;

  CreateTetherService(true /* enable_multidevice_apis */);

  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  fake_tether_host_fetcher_factory_->last_created()->set_tether_hosts(
      test_devices_);
  fake_multidevice_setup_client_->SetFeatureState(
      chromeos::multidevice_setup::mojom::Feature::kInstantTethering,
      chromeos::multidevice_setup::mojom::FeatureState::kEnabledByUser);

  EXPECT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);

  ShutdownTetherService();
  VerifyLastShutdownReason(
      chromeos::tether::TetherComponent::ShutdownReason::USER_LOGGED_OUT);
}

TEST_F(TetherServiceTest, TestMultiDeviceSetupClientLosesVerifiedHost) {
  CreateTetherService(true /* enable_multidevice_apis */);

  EXPECT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);

  fake_tether_host_fetcher_factory_->last_created()->set_tether_hosts({});
  fake_multidevice_setup_client_->SetFeatureState(
      chromeos::multidevice_setup::mojom::Feature::kInstantTethering,
      chromeos::multidevice_setup::mojom::FeatureState::
          kUnavailableNoVerifiedHost);

  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  mock_timer_->Fire();
  ShutdownTetherService();
  VerifyTetherFeatureStateRecorded(
      TetherService::TetherFeatureState::NO_AVAILABLE_HOSTS,
      1 /* expected_count */);
  VerifyLastShutdownReason(chromeos::tether::TetherComponent::ShutdownReason::
                               MULTIDEVICE_HOST_UNVERIFIED);
}

TEST_F(TetherServiceTest, TestBetterTogetherSuiteInitiallyDisabled) {
  initial_feature_state_ = chromeos::multidevice_setup::mojom::FeatureState::
      kUnavailableSuiteDisabled;

  CreateTetherService(true /* enable_multidevice_apis */);

  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  fake_multidevice_setup_client_->SetFeatureState(
      chromeos::multidevice_setup::mojom::Feature::kInstantTethering,
      chromeos::multidevice_setup::mojom::FeatureState::kEnabledByUser);

  EXPECT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);

  ShutdownTetherService();
  VerifyLastShutdownReason(
      chromeos::tether::TetherComponent::ShutdownReason::USER_LOGGED_OUT);
}

TEST_F(TetherServiceTest, TestBetterTogetherSuiteBecomesDisabled) {
  CreateTetherService(true /* enable_multidevice_apis */);

  EXPECT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);

  fake_multidevice_setup_client_->SetFeatureState(
      chromeos::multidevice_setup::mojom::Feature::kInstantTethering,
      chromeos::multidevice_setup::mojom::FeatureState::
          kUnavailableSuiteDisabled);

  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  ShutdownTetherService();
  VerifyTetherFeatureStateRecorded(
      TetherService::TetherFeatureState::BETTER_TOGETHER_SUITE_DISABLED,
      1 /* expected_count */);
  VerifyLastShutdownReason(chromeos::tether::TetherComponent::ShutdownReason::
                               BETTER_TOGETHER_SUITE_DISABLED);
}

TEST_F(TetherServiceTest, TestBleAdvertisingNotSupported) {
  mock_adapter_->set_is_ble_advertising_supported(false);

  CreateTetherService(false /* enable_multidevice_apis */);

  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  VerifyTetherFeatureStateRecorded(
      TetherService::TetherFeatureState::BLE_ADVERTISING_NOT_SUPPORTED,
      1 /* expected_count */);
  VerifyLastShutdownReason(
      chromeos::tether::TetherComponent::ShutdownReason::OTHER);
}

TEST_F(TetherServiceTest,
       TestBleAdvertisingNotSupported_BluetoothIsInitiallyNotPowered) {
  SetIsBluetoothPowered(false);

  mock_adapter_->set_is_ble_advertising_supported(false);

  CreateTetherService(false /* enable_multidevice_apis */);

  // TetherService has not yet been able to find out that BLE advertising is not
  // supported.
  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNINITIALIZED,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      prefs::kInstantTetheringBleAdvertisingSupported));

  SetIsBluetoothPowered(true);

  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(
      prefs::kInstantTetheringBleAdvertisingSupported));

  VerifyTetherFeatureStateRecorded(
      TetherService::TetherFeatureState::BLE_ADVERTISING_NOT_SUPPORTED,
      1 /* expected_count */);
}

TEST_F(
    TetherServiceTest,
    TestBleAdvertisingNotSupportedAndRecorded_BluetoothIsInitiallyNotPowered) {
  SetIsBluetoothPowered(false);

  mock_adapter_->set_is_ble_advertising_supported(false);

  // Simulate a login after we determined that BLE advertising is not supported.
  profile_->GetPrefs()->SetBoolean(
      prefs::kInstantTetheringBleAdvertisingSupported, false);

  CreateTetherService(false /* enable_multidevice_apis */);

  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  SetIsBluetoothPowered(true);

  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  VerifyTetherFeatureStateRecorded(
      TetherService::TetherFeatureState::BLE_ADVERTISING_NOT_SUPPORTED,
      1 /* expected_count */);
}

TEST_F(TetherServiceTest, TestBleAdvertisingSupportedButIncorrectlyRecorded) {
  // Simulate a login after we incorrectly determined that BLE advertising is
  // not supported (this is not an expected case, but may have happened if
  // BluetoothAdapter::SetAdvertisingInterval() failed for a weird, one-off
  // reason).
  profile_->GetPrefs()->SetBoolean(
      prefs::kInstantTetheringBleAdvertisingSupported, false);

  CreateTetherService(false /* enable_multidevice_apis */);

  EXPECT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      prefs::kInstantTetheringBleAdvertisingSupported));

  VerifyTetherFeatureStateRecorded(TetherService::TetherFeatureState::ENABLED,
                                   1 /* expected_count */);

  ShutdownTetherService();
  VerifyLastShutdownReason(
      chromeos::tether::TetherComponent::ShutdownReason::USER_LOGGED_OUT);
}

TEST_F(TetherServiceTest, TestGet_NotPrimaryUser_FeatureFlagDisabled) {
  EXPECT_FALSE(TetherService::Get(profile_.get()));
}

TEST_F(TetherServiceTest, TestGet_PrimaryUser_FeatureFlagDisabled) {
  SetPrimaryUserLoggedIn();
  EXPECT_FALSE(TetherService::Get(profile_.get()));
}

TEST_F(TetherServiceTest, TestGet_NotPrimaryUser_FeatureFlagEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kInstantTethering);

  EXPECT_FALSE(TetherService::Get(profile_.get()));
}

// TODO(https://crbug.com/893878): Fix disabled test.
TEST_F(TetherServiceTest, DISABLED_TestGet_PrimaryUser_FeatureFlagEnabled) {
  SetPrimaryUserLoggedIn();

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {chromeos::features::kInstantTethering} /* enabled_features */,
      {chromeos::features::kMultiDeviceApi} /* disabled_features */);

  TetherService* tether_service = TetherService::Get(profile_.get());
  ASSERT_TRUE(tether_service);

  base::RunLoop().RunUntilIdle();
  tether_service->Shutdown();

  VerifyLastShutdownReason(
      chromeos::tether::TetherComponent::ShutdownReason::USER_LOGGED_OUT);
}

// TODO(https://crbug.com/893878): Fix disabled test.
TEST_F(
    TetherServiceTest,
    DISABLED_TestGet_PrimaryUser_FeatureFlagEnabled_MultiDeviceApiFlagEnabled) {
  SetPrimaryUserLoggedIn();

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {chromeos::features::kInstantTethering,
       chromeos::features::kMultiDeviceApi} /* enabled_features */,
      {} /* disabled_features */);

  TetherService* tether_service = TetherService::Get(profile_.get());
  ASSERT_TRUE(tether_service);

  base::RunLoop().RunUntilIdle();
  tether_service->Shutdown();

  VerifyLastShutdownReason(
      chromeos::tether::TetherComponent::ShutdownReason::USER_LOGGED_OUT);
}

// TODO(https://crbug.com/893878): Fix disabled test.
TEST_F(
    TetherServiceTest,
    DISABLED_TestGet_PrimaryUser_FeatureFlagEnabled_MultiDeviceApiAndMultiDeviceSetupFlagsEnabled) {
  SetPrimaryUserLoggedIn();

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {chromeos::features::kInstantTethering,
       chromeos::features::kMultiDeviceApi,
       chromeos::features::
           kEnableUnifiedMultiDeviceSetup} /* enabled_features */,
      {} /* disabled_features */);

  TetherService* tether_service = TetherService::Get(profile_.get());
  ASSERT_TRUE(tether_service);

  fake_multidevice_setup_client_impl_factory_->fake_multidevice_setup_client()
      ->SetFeatureState(
          chromeos::multidevice_setup::mojom::Feature::kInstantTethering,
          chromeos::multidevice_setup::mojom::FeatureState::kEnabledByUser);

  base::RunLoop().RunUntilIdle();
  tether_service->Shutdown();

  VerifyLastShutdownReason(
      chromeos::tether::TetherComponent::ShutdownReason::USER_LOGGED_OUT);
}

TEST_F(TetherServiceTest, TestNoTetherHosts) {
  fake_tether_host_fetcher_factory_->SetNoInitialDevices();
  CreateTetherService(false /* enable_multidevice_apis */);

  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  // Simulate this being the final state of Tether by passing time.
  mock_timer_->Fire();

  VerifyTetherFeatureStateRecorded(
      TetherService::TetherFeatureState::NO_AVAILABLE_HOSTS,
      1 /* expected_count */);
}

TEST_F(TetherServiceTest, TestProhibitedByPolicy) {
  profile_->GetPrefs()->SetBoolean(
      chromeos::multidevice_setup::kInstantTetheringAllowedPrefName, false);

  CreateTetherService(false /* enable_multidevice_apis */);

  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_PROHIBITED,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  VerifyTetherFeatureStateRecorded(
      TetherService::TetherFeatureState::PROHIBITED, 1 /* expected_count */);
}

TEST_F(TetherServiceTest, TestBluetoothNotPresent) {
  set_is_adapter_present(false);

  CreateTetherService(false /* enable_multidevice_apis */);

  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  // Simulate this being the final state of Tether by passing time.
  mock_timer_->Fire();

  VerifyTetherFeatureStateRecorded(
      TetherService::TetherFeatureState::BLE_NOT_PRESENT,
      1 /* expected_count */);
}

TEST_F(TetherServiceTest, TestMetricsFalsePositives) {
  set_is_adapter_present(false);
  fake_tether_host_fetcher_factory_->SetNoInitialDevices();
  CreateTetherService(false /* enable_multidevice_apis */);

  set_is_adapter_present(true);
  SetIsBluetoothPowered(true);

  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  fake_tether_host_fetcher_factory_->last_created()->set_tether_hosts(
      test_devices_);
  fake_tether_host_fetcher_factory_->last_created()->NotifyTetherHostsUpdated();

  EXPECT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);

  // No metric should have been recorded for BLE_NOT_PRESENT and
  // NO_AVAILABLE_HOSTS, but ENABLED should have been recorded.
  VerifyTetherFeatureStateRecorded(
      TetherService::TetherFeatureState::BLE_NOT_PRESENT,
      0 /* expected_count */);
  VerifyTetherFeatureStateRecorded(
      TetherService::TetherFeatureState::NO_AVAILABLE_HOSTS,
      0 /* expected_count */);
  VerifyTetherFeatureStateRecorded(TetherService::TetherFeatureState::ENABLED,
                                   1 /* expected_count */);

  // Ensure that the pending state recording has been canceled.
  ASSERT_FALSE(mock_timer_->IsRunning());

  ShutdownTetherService();
  VerifyLastShutdownReason(
      chromeos::tether::TetherComponent::ShutdownReason::USER_LOGGED_OUT);
}

TEST_F(TetherServiceTest, TestWifiNotPresent) {
  RemoveWifiFromSystem();

  CreateTetherService(false /* enable_multidevice_apis */);

  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));

  VerifyTetherFeatureStateRecorded(
      TetherService::TetherFeatureState::WIFI_NOT_PRESENT,
      1 /* expected_count */);
}

TEST_F(TetherServiceTest, TestIsBluetoothPowered) {
  SetIsBluetoothPowered(false);

  CreateTetherService(false /* enable_multidevice_apis */);

  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNINITIALIZED,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  SetIsBluetoothPowered(true);

  EXPECT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);

  SetIsBluetoothPowered(false);

  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNINITIALIZED,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  VerifyTetherFeatureStateRecorded(
      TetherService::TetherFeatureState::BLUETOOTH_DISABLED,
      2 /* expected_count */);
  VerifyLastShutdownReason(
      chromeos::tether::TetherComponent::ShutdownReason::BLUETOOTH_DISABLED);
}

TEST_F(TetherServiceTest, TestCellularIsUnavailable) {
  test_manager_client()->RemoveTechnology(shill::kTypeCellular);
  ASSERT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Cellular()));

  CreateTetherService(false /* enable_multidevice_apis */);

  SetTetherTechnologyStateEnabled(false);
  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);
  VerifyLastShutdownReason(
      chromeos::tether::TetherComponent::ShutdownReason::PREF_DISABLED);

  SetTetherTechnologyStateEnabled(true);
  EXPECT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);

  VerifyTetherFeatureStateRecorded(TetherService::TetherFeatureState::ENABLED,
                                   2 /* expected_count */);
}

TEST_F(TetherServiceTest, TestCellularIsAvailable) {
  // TODO (lesliewatkins): Investigate why cellular needs to be removed and
  // re-added for NetworkStateHandler to return the correct TechnologyState.
  test_manager_client()->RemoveTechnology(shill::kTypeCellular);
  test_manager_client()->AddTechnology(shill::kTypeCellular, false);

  CreateTetherService(false /* enable_multidevice_apis */);

  // Cellular disabled
  SetCellularTechnologyStateEnabled(false);
  ASSERT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Cellular()));
  VerifyTetherActiveStatus(false /* expected_active */);

  SetTetherTechnologyStateEnabled(false);
  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  SetTetherTechnologyStateEnabled(true);
  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  // Cellular enabled
  SetCellularTechnologyStateEnabled(true);
  ASSERT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Cellular()));
  VerifyTetherActiveStatus(true /* expected_active */);

  SetTetherTechnologyStateEnabled(false);
  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  SetTetherTechnologyStateEnabled(true);
  EXPECT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);

  SetCellularTechnologyStateEnabled(false);

  VerifyTetherFeatureStateRecorded(
      TetherService::TetherFeatureState::CELLULAR_DISABLED,
      2 /* expected_count */);
  VerifyLastShutdownReason(
      chromeos::tether::TetherComponent::ShutdownReason::CELLULAR_DISABLED);
}

TEST_F(TetherServiceTest, TestDisabled) {
  profile_->GetPrefs()->SetBoolean(
      chromeos::multidevice_setup::kInstantTetheringEnabledPrefName, false);

  CreateTetherService(false /* enable_multidevice_apis */);

  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(
      chromeos::multidevice_setup::kInstantTetheringEnabledPrefName));
  VerifyTetherActiveStatus(false /* expected_active */);

  VerifyTetherFeatureStateRecorded(
      TetherService::TetherFeatureState::USER_PREFERENCE_DISABLED,
      1 /* expected_count */);
}

TEST_F(TetherServiceTest, TestEnabled) {
  CreateTetherService(false /* enable_multidevice_apis */);

  EXPECT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);

  SetTetherTechnologyStateEnabled(false);
  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(
      chromeos::multidevice_setup::kInstantTetheringEnabledPrefName));
  VerifyTetherActiveStatus(false /* expected_active */);
  histogram_tester_.ExpectBucketCount(
      "InstantTethering.UserPreference.OnToggle", false,
      1u /* expected_count */);

  SetTetherTechnologyStateEnabled(true);
  EXPECT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Tether()));
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      chromeos::multidevice_setup::kInstantTetheringEnabledPrefName));
  VerifyTetherActiveStatus(true /* expected_active */);
  histogram_tester_.ExpectBucketCount(
      "InstantTethering.UserPreference.OnToggle", true,
      1u /* expected_count */);

  VerifyTetherFeatureStateRecorded(TetherService::TetherFeatureState::ENABLED,
                                   2 /* expected_count */);
  VerifyLastShutdownReason(
      chromeos::tether::TetherComponent::ShutdownReason::PREF_DISABLED);
}

TEST_F(TetherServiceTest, TestUserPrefChangesViaFeatureStateChange) {
  // Start with the feature disabled.
  initial_feature_state_ =
      chromeos::multidevice_setup::mojom::FeatureState::kDisabledByUser;
  profile_->GetPrefs()->SetBoolean(
      chromeos::multidevice_setup::kInstantTetheringEnabledPrefName, false);
  CreateTetherService(true /* enable_multidevice_apis */);

  // Enable the feature.
  profile_->GetPrefs()->SetBoolean(
      chromeos::multidevice_setup::kInstantTetheringEnabledPrefName, true);
  fake_multidevice_setup_client_->SetFeatureState(
      chromeos::multidevice_setup::mojom::Feature::kInstantTethering,
      chromeos::multidevice_setup::mojom::FeatureState::kEnabledByUser);
  EXPECT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);
  VerifyTetherFeatureStateRecorded(TetherService::TetherFeatureState::ENABLED,
                                   1 /* expected_count */);
  histogram_tester_.ExpectBucketCount(
      "InstantTethering.UserPreference.OnToggle", true,
      1u /* expected_count */);

  // Disable the feature.
  profile_->GetPrefs()->SetBoolean(
      chromeos::multidevice_setup::kInstantTetheringEnabledPrefName, false);
  fake_multidevice_setup_client_->SetFeatureState(
      chromeos::multidevice_setup::mojom::Feature::kInstantTethering,
      chromeos::multidevice_setup::mojom::FeatureState::kDisabledByUser);
  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);
  VerifyTetherFeatureStateRecorded(
      TetherService::TetherFeatureState::USER_PREFERENCE_DISABLED,
      2 /* expected_count */);
  histogram_tester_.ExpectBucketCount(
      "InstantTethering.UserPreference.OnToggle", false,
      1u /* expected_count */);

  // Enable the feature.
  profile_->GetPrefs()->SetBoolean(
      chromeos::multidevice_setup::kInstantTetheringEnabledPrefName, true);
  fake_multidevice_setup_client_->SetFeatureState(
      chromeos::multidevice_setup::mojom::Feature::kInstantTethering,
      chromeos::multidevice_setup::mojom::FeatureState::kEnabledByUser);
  EXPECT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);
  VerifyTetherFeatureStateRecorded(TetherService::TetherFeatureState::ENABLED,
                                   2 /* expected_count */);
  histogram_tester_.ExpectBucketCount(
      "InstantTethering.UserPreference.OnToggle", true,
      2u /* expected_count */);

  VerifyLastShutdownReason(
      chromeos::tether::TetherComponent::ShutdownReason::PREF_DISABLED);
}

TEST_F(TetherServiceTest, TestUserPrefChangesViaTechnologyStateChange) {
  CreateTetherService(true /* enable_multidevice_apis */);

  EXPECT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);

  SetTetherTechnologyStateEnabled(false);
  fake_multidevice_setup_client_->InvokePendingSetFeatureEnabledStateCallback(
      chromeos::multidevice_setup::mojom::Feature::kInstantTethering,
      false /* expected_enabled */, base::nullopt /* expected_auth_token */,
      true /* success */);
  profile_->GetPrefs()->SetBoolean(
      chromeos::multidevice_setup::kInstantTetheringEnabledPrefName, false);
  fake_multidevice_setup_client_->SetFeatureState(
      chromeos::multidevice_setup::mojom::Feature::kInstantTethering,
      chromeos::multidevice_setup::mojom::FeatureState::kDisabledByUser);
  EXPECT_EQ(
      chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE,
      network_state_handler()->GetTechnologyState(
          chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);
  histogram_tester_.ExpectBucketCount(
      "InstantTethering.UserPreference.OnToggle", false,
      1u /* expected_count */);

  SetTetherTechnologyStateEnabled(true);
  fake_multidevice_setup_client_->InvokePendingSetFeatureEnabledStateCallback(
      chromeos::multidevice_setup::mojom::Feature::kInstantTethering,
      true /* expected_enabled */, base::nullopt /* expected_auth_token */,
      false /* success */);
  profile_->GetPrefs()->SetBoolean(
      chromeos::multidevice_setup::kInstantTetheringEnabledPrefName, true);
  fake_multidevice_setup_client_->SetFeatureState(
      chromeos::multidevice_setup::mojom::Feature::kInstantTethering,
      chromeos::multidevice_setup::mojom::FeatureState::kEnabledByUser);
  EXPECT_EQ(chromeos::NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                chromeos::NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);
  histogram_tester_.ExpectBucketCount(
      "InstantTethering.UserPreference.OnToggle", true,
      1u /* expected_count */);

  VerifyTetherFeatureStateRecorded(TetherService::TetherFeatureState::ENABLED,
                                   2 /* expected_count */);
  VerifyLastShutdownReason(
      chromeos::tether::TetherComponent::ShutdownReason::PREF_DISABLED);
}

// Test against a past defect that made TetherService and NetworkStateHandler
// repeatly update technology state after the other did so. TetherService should
// only update technology state if NetworkStateHandler has provided a different
// state than the user preference.
TEST_F(TetherServiceTest, TestEnabledMultipleChanges) {
  CreateTetherService(false /* enable_multidevice_apis */);

  // CreateTetherService calls RunUntilIdle() so UpdateTetherTechnologyState()
  // may be called multiple times in the initialization process.
  int updated_technology_state_count =
      tether_service_->updated_technology_state_count();

  SetTetherTechnologyStateEnabled(false);
  SetTetherTechnologyStateEnabled(false);
  SetTetherTechnologyStateEnabled(false);

  updated_technology_state_count++;
  EXPECT_EQ(updated_technology_state_count,
            tether_service_->updated_technology_state_count());

  SetTetherTechnologyStateEnabled(true);
  SetTetherTechnologyStateEnabled(true);
  SetTetherTechnologyStateEnabled(true);

  updated_technology_state_count++;
  EXPECT_EQ(updated_technology_state_count,
            tether_service_->updated_technology_state_count());

  VerifyLastShutdownReason(
      chromeos::tether::TetherComponent::ShutdownReason::PREF_DISABLED);
}
