// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/tether/tether_service.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/ash/network/tether_notification_presenter.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/multidevice/software_feature.h"
#include "chromeos/ash/components/network/network_connect.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/ash/components/network/technology_state_controller.h"
#include "chromeos/ash/components/tether/fake_notification_presenter.h"
#include "chromeos/ash/components/tether/fake_tether_component.h"
#include "chromeos/ash/components/tether/fake_tether_host_fetcher.h"
#include "chromeos/ash/components/tether/tether_component_impl.h"
#include "chromeos/ash/components/tether/tether_host_fetcher_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_device_manager.h"
#include "chromeos/ash/services/device_sync/fake_cryptauth_enrollment_manager.h"
#include "chromeos/ash/services/device_sync/fake_remote_device_provider.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client_impl.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/ash/services/device_sync/remote_device_provider_impl.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client_impl.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/prefs.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client_impl.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace ash {
namespace tether {

namespace {

using ::testing::Invoke;
using ::testing::NiceMock;

const char kTestUserPrivateKey[] = "kTestUserPrivateKey";

class TestTetherService : public TetherService {
 public:
  TestTetherService(
      Profile* profile,
      chromeos::PowerManagerClient* power_manager_client,
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client,
      session_manager::SessionManager* session_manager)
      : TetherService(profile,
                      power_manager_client,
                      device_sync_client,
                      secure_channel_client,
                      multidevice_setup_client,
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

class FakeTetherComponentWithDestructorCallback : public FakeTetherComponent {
 public:
  FakeTetherComponentWithDestructorCallback(
      base::OnceClosure destructor_callback)
      : FakeTetherComponent(/*has_asynchronous_shutdown=*/false),
        destructor_callback_(std::move(destructor_callback)) {}

  ~FakeTetherComponentWithDestructorCallback() override {
    std::move(destructor_callback_).Run();
  }

 private:
  base::OnceClosure destructor_callback_;
};

class TestTetherComponentFactory final : public TetherComponentImpl::Factory {
 public:
  TestTetherComponentFactory() {}

  // Returns nullptr if no TetherComponent has been created or if the last one
  // that was created has already been deleted.
  FakeTetherComponentWithDestructorCallback* active_tether_component() {
    return active_tether_component_;
  }

  // TetherComponentImpl::Factory:
  std::unique_ptr<TetherComponent> CreateInstance(
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client,
      TetherHostFetcher* tether_host_fetcher,
      NotificationPresenter* notification_presenter,
      GmsCoreNotificationsStateTrackerImpl*
          gms_core_notifications_state_tracker,
      PrefService* pref_service,
      NetworkHandler* network_handler,
      NetworkConnect* network_connect,
      scoped_refptr<device::BluetoothAdapter> adapter,
      session_manager::SessionManager* session_manager) override {
    active_tether_component_ =
        new FakeTetherComponentWithDestructorCallback(base::BindOnce(
            &TestTetherComponentFactory::OnActiveTetherComponentDeleted,
            base::Unretained(this)));
    was_tether_component_active_ = true;
    return base::WrapUnique(active_tether_component_.get());
  }

  bool was_tether_component_active() { return was_tether_component_active_; }

  const TetherComponent::ShutdownReason& last_shutdown_reason() {
    return last_shutdown_reason_;
  }

 private:
  void OnActiveTetherComponentDeleted() {
    last_shutdown_reason_ = *active_tether_component_->last_shutdown_reason();
    active_tether_component_ = nullptr;
  }

  raw_ptr<FakeTetherComponentWithDestructorCallback> active_tether_component_ =
      nullptr;
  bool was_tether_component_active_ = false;
  TetherComponent::ShutdownReason last_shutdown_reason_;
};

class FakeRemoteDeviceProviderFactory
    : public device_sync::RemoteDeviceProviderImpl::Factory {
 public:
  FakeRemoteDeviceProviderFactory() = default;
  ~FakeRemoteDeviceProviderFactory() override = default;

  // device_sync::RemoteDeviceProviderImpl::Factory:
  std::unique_ptr<device_sync::RemoteDeviceProvider> CreateInstance(
      device_sync::CryptAuthV2DeviceManager* v2_device_manager,
      const std::string& user_email,
      const std::string& user_private_key) override {
    return std::make_unique<device_sync::FakeRemoteDeviceProvider>();
  }
};

class FakeTetherHostFetcherFactory : public TetherHostFetcherImpl::Factory {
 public:
  FakeTetherHostFetcherFactory(
      const multidevice::RemoteDeviceRef& initial_device)
      : initial_device_(initial_device) {}
  virtual ~FakeTetherHostFetcherFactory() = default;

  FakeTetherHostFetcher* last_created() { return last_created_; }

  void SetNoInitialDevices() { initial_device_ = std::nullopt; }

  // TetherHostFetcherImpl::Factory :
  std::unique_ptr<TetherHostFetcher> CreateInstance(
      device_sync::DeviceSyncClient* device_sync_client,
      multidevice_setup::MultiDeviceSetupClient* multidevice_setup_client)
      override {
    last_created_ = new FakeTetherHostFetcher(initial_device_);
    return base::WrapUnique(last_created_.get());
  }

 private:
  std::optional<multidevice::RemoteDeviceRef> initial_device_;
  raw_ptr<FakeTetherHostFetcher, DanglingUntriaged> last_created_ = nullptr;
};

class FakeDeviceSyncClientImplFactory
    : public device_sync::DeviceSyncClientImpl::Factory {
 public:
  FakeDeviceSyncClientImplFactory() = default;

  ~FakeDeviceSyncClientImplFactory() override = default;

  // device_sync::DeviceSyncClientImpl::Factory:
  std::unique_ptr<device_sync::DeviceSyncClient> CreateInstance() override {
    auto fake_device_sync_client =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_device_sync_client->NotifyReady();
    return fake_device_sync_client;
  }
};

class FakeSecureChannelClientImplFactory
    : public secure_channel::SecureChannelClientImpl::Factory {
 public:
  FakeSecureChannelClientImplFactory() = default;

  ~FakeSecureChannelClientImplFactory() override = default;

  // secure_channel::SecureChannelClientImpl::Factory:
  std::unique_ptr<secure_channel::SecureChannelClient> CreateInstance(
      mojo::PendingRemote<secure_channel::mojom::SecureChannel> channel,
      scoped_refptr<base::TaskRunner> task_runner) override {
    return std::make_unique<secure_channel::FakeSecureChannelClient>();
  }
};

class FakeMultiDeviceSetupClientImplFactory
    : public multidevice_setup::MultiDeviceSetupClientImpl::Factory {
 public:
  FakeMultiDeviceSetupClientImplFactory() = default;

  ~FakeMultiDeviceSetupClientImplFactory() override = default;

  // multidevice_setup::MultiDeviceSetupClientImpl::Factory:
  std::unique_ptr<multidevice_setup::MultiDeviceSetupClient> CreateInstance(
      mojo::PendingRemote<multidevice_setup::mojom::MultiDeviceSetup>)
      override {
    auto fake_multidevice_setup_client =
        std::make_unique<multidevice_setup::FakeMultiDeviceSetupClient>();
    fake_multidevice_setup_client_ = fake_multidevice_setup_client.get();
    return fake_multidevice_setup_client;
  }

  multidevice_setup::FakeMultiDeviceSetupClient*
  fake_multidevice_setup_client() {
    return fake_multidevice_setup_client_;
  }

 private:
  raw_ptr<multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;
};

}  // namespace

class TetherServiceTest : public testing::Test {
 public:
  TetherServiceTest(const TetherServiceTest&) = delete;
  TetherServiceTest& operator=(const TetherServiceTest&) = delete;

 protected:
  TetherServiceTest()
      : test_device_(multidevice::CreateRemoteDeviceRefForTest()) {}
  ~TetherServiceTest() override {}

  void SetUp() override {
    fake_notification_presenter_ = nullptr;
    mock_timer_ = nullptr;

    NetworkConnect::Initialize(nullptr);

    TestingProfile::Builder builder;
    profile_ = builder.Build();

    // TestingProfile creates FakeChromeUserManager, so it could be obtained
    // from UserManager::Get().
    fake_chrome_user_manager_ = static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());

    chromeos::PowerManagerClient::InitializeFake();

    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_device_sync_client_->NotifyReady();

    fake_device_sync_client_impl_factory_ =
        std::make_unique<FakeDeviceSyncClientImplFactory>();
    device_sync::DeviceSyncClientImpl::Factory::SetFactoryForTesting(
        fake_device_sync_client_impl_factory_.get());

    fake_secure_channel_client_ =
        std::make_unique<secure_channel::FakeSecureChannelClient>();
    fake_secure_channel_client_impl_factory_ =
        std::make_unique<FakeSecureChannelClientImplFactory>();
    secure_channel::SecureChannelClientImpl::Factory::SetFactoryForTesting(
        fake_secure_channel_client_impl_factory_.get());

    fake_multidevice_setup_client_ =
        std::make_unique<multidevice_setup::FakeMultiDeviceSetupClient>();
    fake_multidevice_setup_client_impl_factory_ =
        std::make_unique<FakeMultiDeviceSetupClientImplFactory>();
    multidevice_setup::MultiDeviceSetupClientImpl::Factory::
        SetFactoryForTesting(fake_multidevice_setup_client_impl_factory_.get());
    initial_feature_state_ =
        multidevice_setup::mojom::FeatureState::kEnabledByUser;

    fake_enrollment_manager_ =
        std::make_unique<device_sync::FakeCryptAuthEnrollmentManager>();
    fake_enrollment_manager_->set_user_private_key(kTestUserPrivateKey);

    mock_adapter_ =
        base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();
    SetIsBluetoothPowered(true);
    is_adapter_present_ = true;
    ON_CALL(*mock_adapter_, IsPresent())
        .WillByDefault(Invoke(this, &TetherServiceTest::IsBluetoothPresent));
    ON_CALL(*mock_adapter_, IsPowered())
        .WillByDefault(Invoke(this, &TetherServiceTest::IsBluetoothPowered));
    device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);

    test_tether_component_factory_ =
        base::WrapUnique(new TestTetherComponentFactory());
    TetherComponentImpl::Factory::SetFactoryForTesting(
        test_tether_component_factory_.get());
    shutdown_reason_verified_ = false;

    fake_remote_device_provider_factory_ =
        base::WrapUnique(new FakeRemoteDeviceProviderFactory());
    device_sync::RemoteDeviceProviderImpl::Factory::SetFactoryForTesting(
        fake_remote_device_provider_factory_.get());

    fake_tether_host_fetcher_factory_ =
        base::WrapUnique(new FakeTetherHostFetcherFactory(test_device_));
    TetherHostFetcherImpl::Factory::SetFactoryForTesting(
        fake_tether_host_fetcher_factory_.get());

    TestingBrowserProcess::GetGlobal()->SetLocalState(&local_pref_service_);
    RegisterLocalState(local_pref_service_.registry());
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);

    device_sync::DeviceSyncClientImpl::Factory::SetFactoryForTesting(nullptr);
    secure_channel::SecureChannelClientImpl::Factory::SetFactoryForTesting(
        nullptr);
    multidevice_setup::MultiDeviceSetupClientImpl::Factory::
        SetFactoryForTesting(nullptr);

    ShutdownTetherService();

    if (tether_service_) {
      // As of crbug.com/798605, SHUT_DOWN should not be logged since it does
      // not contribute meaningful data.
      histogram_tester_.ExpectBucketCount(
          "InstantTethering.FeatureState",
          TetherService::TetherFeatureState::SHUT_DOWN, 0 /* count */);
      tether_service_.reset();
    }

    EXPECT_EQ(test_tether_component_factory_->was_tether_component_active(),
              shutdown_reason_verified_);

    chromeos::PowerManagerClient::Shutdown();
    NetworkConnect::Shutdown();
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

  void CreateTetherService() {
    fake_multidevice_setup_client_->SetFeatureState(
        multidevice_setup::mojom::Feature::kInstantTethering,
        initial_feature_state_);

    tether_service_ = base::WrapUnique(new TestTetherService(
        profile_.get(), chromeos::FakePowerManagerClient::Get(),
        fake_device_sync_client_.get(), fake_secure_channel_client_.get(),
        fake_multidevice_setup_client_.get(), nullptr /* session_manager */));

    fake_notification_presenter_ = new FakeNotificationPresenter();
    mock_timer_ = new base::MockOneShotTimer();
    tether_service_->SetTestDoubles(
        base::WrapUnique(fake_notification_presenter_.get()),
        base::WrapUnique(mock_timer_.get()));

    SetPrimaryUserLoggedIn();

    // Ensure that TetherService does not prematurely update its
    // TechnologyState before it fetches the BluetoothAdapter.
    EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
              network_state_handler()->GetTechnologyState(
                  NetworkTypePattern::Tether()));
    VerifyTetherActiveStatus(false /* expected_active */);

    if (!fake_device_sync_client_->is_ready()) {
      return;
    }

    // Allow the posted task to fetch the BluetoothAdapter to finish.
    base::RunLoop().RunUntilIdle();
  }

  void ShutdownTetherService() {
    if (tether_service_) {
      tether_service_->Shutdown();
    }
  }

  void SetTetherTechnologyStateEnabled(bool enabled) {
    network_state_handler()->SetTetherTechnologyState(
        enabled ? NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED
                : NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE);
  }

  void SetCellularTechnologyStateEnabled(bool enabled) {
    NetworkHandler::Get()
        ->technology_state_controller()
        ->SetTechnologiesEnabled(NetworkTypePattern::Cellular(), enabled,
                                 network_handler::ErrorCallback());
    base::RunLoop().RunUntilIdle();
  }

  void SetTetherUserPrefState(bool enabled) {
    fake_multidevice_setup_client_->InvokePendingSetFeatureEnabledStateCallback(
        multidevice_setup::mojom::Feature::kInstantTethering,
        enabled /* expected_enabled */, std::nullopt /* expected_auth_token */,
        !enabled /* success */);
    profile_->GetPrefs()->SetBoolean(
        multidevice_setup::kInstantTetheringEnabledPrefName, enabled);
    if (enabled) {
      fake_multidevice_setup_client_->SetFeatureState(
          multidevice_setup::mojom::Feature::kInstantTethering,
          multidevice_setup::mojom::FeatureState::kEnabledByUser);
    } else {
      fake_multidevice_setup_client_->SetFeatureState(
          multidevice_setup::mojom::Feature::kInstantTethering,
          multidevice_setup::mojom::FeatureState::kDisabledByUser);
    }
  }

  void SetIsBluetoothPowered(bool powered) {
    is_adapter_powered_ = powered;
    for (auto& observer : mock_adapter_->GetObservers()) {
      observer.AdapterPoweredChanged(mock_adapter_.get(), powered);
    }
  }

  void set_is_adapter_present(bool present) { is_adapter_present_ = present; }

  bool IsBluetoothPresent() { return is_adapter_present_; }
  bool IsBluetoothPowered() { return is_adapter_powered_; }

  void RemoveWifiFromSystem() {
    manager_test()->RemoveTechnology(shill::kTypeWifi);
    base::RunLoop().RunUntilIdle();
  }

  void DisconnectDefaultShillNetworks() {
    const NetworkState* default_state;
    while ((default_state = network_state_handler()->DefaultNetwork())) {
      network_handler_test_helper_.service_test()->SetServiceProperty(
          default_state->path(), shill::kStateProperty,
          base::Value(shill::kStateIdle));
      base::RunLoop().RunUntilIdle();
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
      const TetherComponent::ShutdownReason& expected_shutdown_reason) {
    EXPECT_EQ(expected_shutdown_reason,
              test_tether_component_factory_->last_shutdown_reason());
    shutdown_reason_verified_ = true;
  }

  ShillManagerClient::TestInterface* manager_test() {
    return network_handler_test_helper_.manager_test();
  }

  NetworkStateHandler* network_state_handler() {
    return NetworkHandler::Get()->network_state_handler();
  }

  const multidevice::RemoteDeviceRef test_device_;
  const content::BrowserTaskEnvironment task_environment_;

  NetworkHandlerTestHelper network_handler_test_helper_;
  raw_ptr<FakeChromeUserManager, DanglingUntriaged> fake_chrome_user_manager_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
      test_pref_service_;
  std::unique_ptr<TestTetherComponentFactory> test_tether_component_factory_;
  std::unique_ptr<FakeRemoteDeviceProviderFactory>
      fake_remote_device_provider_factory_;
  std::unique_ptr<FakeTetherHostFetcherFactory>
      fake_tether_host_fetcher_factory_;
  raw_ptr<FakeNotificationPresenter, DanglingUntriaged>
      fake_notification_presenter_;
  raw_ptr<base::MockOneShotTimer, DanglingUntriaged> mock_timer_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<FakeDeviceSyncClientImplFactory>
      fake_device_sync_client_impl_factory_;
  std::unique_ptr<secure_channel::FakeSecureChannelClient>
      fake_secure_channel_client_;
  std::unique_ptr<FakeSecureChannelClientImplFactory>
      fake_secure_channel_client_impl_factory_;
  std::unique_ptr<multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;
  std::unique_ptr<FakeMultiDeviceSetupClientImplFactory>
      fake_multidevice_setup_client_impl_factory_;
  std::unique_ptr<device_sync::FakeCryptAuthEnrollmentManager>
      fake_enrollment_manager_;

  multidevice_setup::mojom::FeatureState initial_feature_state_;

  scoped_refptr<device::MockBluetoothAdapter> mock_adapter_;
  bool is_adapter_present_;
  bool is_adapter_powered_;
  bool shutdown_reason_verified_;

  // PrefService which contains the browser process' local storage.
  TestingPrefServiceSimple local_pref_service_;

  std::unique_ptr<TestTetherService> tether_service_;
  std::unique_ptr<TestingProfile> profile_;

  base::HistogramTester histogram_tester_;
};

TEST_F(TetherServiceTest, TestShutdown) {
  CreateTetherService();
  VerifyTetherActiveStatus(true /* expected_active */);

  ShutdownTetherService();

  // The TechnologyState should not have changed due to Shutdown() being called.
  // If it had changed, any settings UI that was previously open would have
  // shown visual jank.
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);
  VerifyLastShutdownReason(TetherComponent::ShutdownReason::USER_LOGGED_OUT);
}

TEST_F(TetherServiceTest, TestAsyncTetherShutdown) {
  CreateTetherService();

  // Tether should be ENABLED, and there should be no AsyncShutdownTask.
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);

  // Use an asynchronous shutdown.
  test_tether_component_factory_->active_tether_component()
      ->set_has_asynchronous_shutdown(true);

  // Disable the Tether preference. This should trigger the asynchrnous
  // shutdown.
  SetTetherTechnologyStateEnabled(false);
  SetTetherUserPrefState(false);

  // Tether should be active, but shutting down.
  VerifyTetherActiveStatus(true /* expected_active */);
  EXPECT_EQ(
      TetherComponent::Status::SHUTTING_DOWN,
      test_tether_component_factory_->active_tether_component()->status());

  // Tether should be AVAILABLE.
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));

  // Complete the shutdown process; TetherService should delete its
  // TetherComponent instance.
  test_tether_component_factory_->active_tether_component()
      ->FinishAsynchronousShutdown();
  VerifyTetherActiveStatus(false /* expected_active */);
  VerifyLastShutdownReason(TetherComponent::ShutdownReason::PREF_DISABLED);
}

TEST_F(TetherServiceTest, TestSuspend) {
  CreateTetherService();
  VerifyTetherActiveStatus(true /* expected_active */);

  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);

  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();

  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);

  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);

  VerifyTetherFeatureStateRecorded(TetherService::TetherFeatureState::SUSPENDED,
                                   2 /* expected_count */);
  VerifyLastShutdownReason(TetherComponent::ShutdownReason::USER_CLOSED_LID);
}

TEST_F(TetherServiceTest, TestDeviceSyncClientNotReady) {
  fake_device_sync_client_ =
      std::make_unique<device_sync::FakeDeviceSyncClient>();

  CreateTetherService();

  fake_device_sync_client_->NotifyReady();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);

  ShutdownTetherService();
  VerifyLastShutdownReason(TetherComponent::ShutdownReason::USER_LOGGED_OUT);
}

TEST_F(TetherServiceTest,
       TestMultiDeviceSetupClientInitiallyHasNoVerifiedHost) {
  fake_tether_host_fetcher_factory_->SetNoInitialDevices();
  initial_feature_state_ = multidevice_setup::mojom::FeatureState::
      kUnavailableNoVerifiedHost_NoEligibleHosts;

  CreateTetherService();

  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  fake_tether_host_fetcher_factory_->last_created()->SetTetherHost(
      test_device_);
  fake_multidevice_setup_client_->SetFeatureState(
      multidevice_setup::mojom::Feature::kInstantTethering,
      multidevice_setup::mojom::FeatureState::kEnabledByUser);

  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);

  ShutdownTetherService();
  VerifyLastShutdownReason(TetherComponent::ShutdownReason::USER_LOGGED_OUT);
}

TEST_F(TetherServiceTest, TestMultiDeviceSetupClientLosesVerifiedHost) {
  CreateTetherService();

  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);

  fake_tether_host_fetcher_factory_->last_created()->SetTetherHost(
      std::nullopt);
  fake_multidevice_setup_client_->SetFeatureState(
      multidevice_setup::mojom::Feature::kInstantTethering,
      multidevice_setup::mojom::FeatureState::
          kUnavailableNoVerifiedHost_HostExistsButNotSetAndVerified);

  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  mock_timer_->Fire();
  ShutdownTetherService();
  VerifyTetherFeatureStateRecorded(
      TetherService::TetherFeatureState::NO_AVAILABLE_HOSTS,
      1 /* expected_count */);
  VerifyLastShutdownReason(
      TetherComponent::ShutdownReason::MULTIDEVICE_HOST_UNVERIFIED);
}

TEST_F(TetherServiceTest, TestBetterTogetherSuiteInitiallyDisabled) {
  initial_feature_state_ =
      multidevice_setup::mojom::FeatureState::kUnavailableSuiteDisabled;

  CreateTetherService();

  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  fake_multidevice_setup_client_->SetFeatureState(
      multidevice_setup::mojom::Feature::kInstantTethering,
      multidevice_setup::mojom::FeatureState::kEnabledByUser);

  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);

  ShutdownTetherService();
  VerifyLastShutdownReason(TetherComponent::ShutdownReason::USER_LOGGED_OUT);
}

TEST_F(TetherServiceTest, TestBetterTogetherSuiteBecomesDisabled) {
  CreateTetherService();

  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);

  fake_multidevice_setup_client_->SetFeatureState(
      multidevice_setup::mojom::Feature::kInstantTethering,
      multidevice_setup::mojom::FeatureState::kUnavailableSuiteDisabled);

  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  ShutdownTetherService();
  VerifyTetherFeatureStateRecorded(
      TetherService::TetherFeatureState::BETTER_TOGETHER_SUITE_DISABLED,
      1 /* expected_count */);
  VerifyLastShutdownReason(
      TetherComponent::ShutdownReason::BETTER_TOGETHER_SUITE_DISABLED);
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
  feature_list.InitAndEnableFeature(features::kInstantTethering);

  EXPECT_FALSE(TetherService::Get(profile_.get()));
}

// Regression test for b/242870461.
TEST_F(TetherServiceTest, TestRegression_TetherDisabledWhileBluetoothDisabled) {
  initial_feature_state_ =
      multidevice_setup::mojom::FeatureState::kDisabledByUser;
  profile_->GetPrefs()->SetBoolean(
      multidevice_setup::kInstantTetheringEnabledPrefName, false);
  SetIsBluetoothPowered(false);

  CreateTetherService();

  // Even though Bluetooth can be initalized, Tether should be UNAVAILABLE as
  // it is disabled by user preference.
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));

  fake_multidevice_setup_client_->SetFeatureState(
      multidevice_setup::mojom::Feature::kInstantTethering,
      multidevice_setup::mojom::FeatureState::kEnabledByUser);

  // Technology should be UNINITIALIZED, since now only Bluetooth is disabled.
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_UNINITIALIZED,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
}

// Regression test for b/242870461.
TEST_F(TetherServiceTest,
       TestRegression_BetterTogetherDisabledWhileBluetoothDisabled) {
  initial_feature_state_ =
      multidevice_setup::mojom::FeatureState::kUnavailableSuiteDisabled;
  SetIsBluetoothPowered(false);

  CreateTetherService();

  // Even though Bluetooth can be initalized, Better Together being disabled
  // should make the Tether state UNAVAILABLE, rather than UNINITIALIZED.
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));

  fake_multidevice_setup_client_->SetFeatureState(
      multidevice_setup::mojom::Feature::kInstantTethering,
      multidevice_setup::mojom::FeatureState::kEnabledByUser);

  // Technology should be UNINITIALIZED, since now only Bluetooth is disabled.
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_UNINITIALIZED,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
}

// Regression test for b/242870461.
// TODO(https://crbug.com/893878): Fix disabled test.
TEST_F(TetherServiceTest,
       DISABLED_TestRegression_ProhibitedByPolicyWhileBluetoothDisabled) {
  profile_->GetPrefs()->SetBoolean(
      multidevice_setup::kInstantTetheringAllowedPrefName, false);
  SetIsBluetoothPowered(false);

  CreateTetherService();

  // Even though Bluetooth can be initalized, Tether should be UNAVAILABLE as
  // it is prohibited by policy.
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));

  profile_->GetPrefs()->SetBoolean(
      multidevice_setup::kInstantTetheringAllowedPrefName, true);

  // Technology should be UNINITIALIZED, since now only Bluetooth is disabled.
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_UNINITIALIZED,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
}

// TODO(https://crbug.com/893878): Fix disabled test.
TEST_F(TetherServiceTest, DISABLED_TestGet_PrimaryUser_FeatureFlagEnabled) {
  SetPrimaryUserLoggedIn();

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kInstantTethering} /* enabled_features */,
      {} /* disabled_features */);

  TetherService* tether_service = TetherService::Get(profile_.get());
  ASSERT_TRUE(tether_service);

  base::RunLoop().RunUntilIdle();
  tether_service->Shutdown();

  VerifyLastShutdownReason(TetherComponent::ShutdownReason::USER_LOGGED_OUT);
}

// TODO(https://crbug.com/893878): Fix disabled test.
TEST_F(
    TetherServiceTest,
    DISABLED_TestGet_PrimaryUser_FeatureFlagEnabled_MultiDeviceApiFlagEnabled) {
  SetPrimaryUserLoggedIn();

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kInstantTethering} /* enabled_features */,
      {} /* disabled_features */);

  TetherService* tether_service = TetherService::Get(profile_.get());
  ASSERT_TRUE(tether_service);

  base::RunLoop().RunUntilIdle();
  tether_service->Shutdown();

  VerifyLastShutdownReason(TetherComponent::ShutdownReason::USER_LOGGED_OUT);
}

// TODO(https://crbug.com/893878): Fix disabled test.
TEST_F(
    TetherServiceTest,
    DISABLED_TestGet_PrimaryUser_FeatureFlagEnabled_MultiDeviceApiAndMultiDeviceSetupFlagsEnabled) {
  SetPrimaryUserLoggedIn();

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kInstantTethering} /* enabled_features */,
      {} /* disabled_features */);

  TetherService* tether_service = TetherService::Get(profile_.get());
  ASSERT_TRUE(tether_service);

  fake_multidevice_setup_client_impl_factory_->fake_multidevice_setup_client()
      ->SetFeatureState(multidevice_setup::mojom::Feature::kInstantTethering,
                        multidevice_setup::mojom::FeatureState::kEnabledByUser);

  base::RunLoop().RunUntilIdle();
  tether_service->Shutdown();

  VerifyLastShutdownReason(TetherComponent::ShutdownReason::USER_LOGGED_OUT);
}

TEST_F(TetherServiceTest, TestNoTetherHosts) {
  fake_tether_host_fetcher_factory_->SetNoInitialDevices();
  CreateTetherService();

  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  // Simulate this being the final state of Tether by passing time.
  mock_timer_->Fire();

  VerifyTetherFeatureStateRecorded(
      TetherService::TetherFeatureState::NO_AVAILABLE_HOSTS,
      1 /* expected_count */);
}

// TODO(https://crbug.com/893878): Fix disabled test.
TEST_F(TetherServiceTest, DISABLED_TestProhibitedByPolicy) {
  profile_->GetPrefs()->SetBoolean(
      multidevice_setup::kInstantTetheringAllowedPrefName, false);

  CreateTetherService();

  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_PROHIBITED,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  VerifyTetherFeatureStateRecorded(
      TetherService::TetherFeatureState::PROHIBITED, 1 /* expected_count */);
}

TEST_F(TetherServiceTest, TestBluetoothNotPresent) {
  set_is_adapter_present(false);

  CreateTetherService();

  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
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
  CreateTetherService();

  set_is_adapter_present(true);
  SetIsBluetoothPowered(true);

  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  fake_tether_host_fetcher_factory_->last_created()->SetTetherHost(
      test_device_);

  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
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
  VerifyLastShutdownReason(TetherComponent::ShutdownReason::USER_LOGGED_OUT);
}

TEST_F(TetherServiceTest, TestWifiNotPresent) {
  RemoveWifiFromSystem();

  CreateTetherService();

  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));

  VerifyTetherFeatureStateRecorded(
      TetherService::TetherFeatureState::WIFI_NOT_PRESENT,
      1 /* expected_count */);
}

TEST_F(TetherServiceTest, TestIsBluetoothPowered) {
  SetIsBluetoothPowered(false);

  CreateTetherService();

  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_UNINITIALIZED,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  SetIsBluetoothPowered(true);

  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);

  SetIsBluetoothPowered(false);

  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_UNINITIALIZED,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  VerifyTetherFeatureStateRecorded(
      TetherService::TetherFeatureState::BLUETOOTH_DISABLED,
      2 /* expected_count */);
  VerifyLastShutdownReason(TetherComponent::ShutdownReason::BLUETOOTH_DISABLED);
}

TEST_F(TetherServiceTest, TestCellularIsUnavailable) {
  manager_test()->RemoveTechnology(shill::kTypeCellular);
  ASSERT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Cellular()));

  CreateTetherService();

  SetTetherTechnologyStateEnabled(false);
  SetTetherUserPrefState(false);
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);
  VerifyLastShutdownReason(TetherComponent::ShutdownReason::PREF_DISABLED);

  SetTetherTechnologyStateEnabled(true);
  SetTetherUserPrefState(true);
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);

  VerifyTetherFeatureStateRecorded(TetherService::TetherFeatureState::ENABLED,
                                   2 /* expected_count */);
}

TEST_F(TetherServiceTest,
       TestCellularIsAvailable_InstantHotspotRebrandDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {} /* enabled_features */,
      {features::kInstantHotspotRebrand} /* disabled_features */);

  CreateTetherService();

  // Cellular disabled
  SetCellularTechnologyStateEnabled(false);
  ASSERT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Cellular()));
  VerifyTetherActiveStatus(false /* expected_active */);

  // Tether disabled
  SetTetherTechnologyStateEnabled(false);
  SetTetherUserPrefState(false);
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  // Tether enabled
  SetTetherTechnologyStateEnabled(true);
  SetTetherUserPrefState(true);
  // If the Instant Hotspot Rebrand feature flag is disabled, enabling tether
  // while cellular is disabled should NOT affect tether.
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_UNAVAILABLE,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  // Cellular enabled
  SetCellularTechnologyStateEnabled(true);
  ASSERT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Cellular()));
  VerifyTetherActiveStatus(true /* expected_active */);

  // Tether enabled
  SetTetherTechnologyStateEnabled(false);
  SetTetherUserPrefState(false);
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  SetTetherTechnologyStateEnabled(true);
  SetTetherUserPrefState(true);
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);

  SetCellularTechnologyStateEnabled(false);

  VerifyTetherFeatureStateRecorded(
      TetherService::TetherFeatureState::CELLULAR_DISABLED,
      2 /* expected_count */);
  VerifyLastShutdownReason(TetherComponent::ShutdownReason::CELLULAR_DISABLED);
}

TEST_F(TetherServiceTest,
       TestCellularIsAvailable_InstantHotspotRebrandEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kInstantHotspotRebrand} /* enabled_features */,
      {} /* disabled_features */);

  CreateTetherService();

  // Cellular disabled
  SetCellularTechnologyStateEnabled(false);
  ASSERT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Cellular()));
  VerifyTetherActiveStatus(true /* expected_active */);

  // Tether disabled
  SetTetherTechnologyStateEnabled(false);
  SetTetherUserPrefState(false);
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  // Tether enabled
  SetTetherTechnologyStateEnabled(true);
  SetTetherUserPrefState(true);
  // If the Instant Hotspot Rebrand feature flag is enabled, enabling tether
  // while cellular is disabled should affect tether.
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);

  // Cellular enabled
  SetCellularTechnologyStateEnabled(true);
  ASSERT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Cellular()));
  VerifyTetherActiveStatus(true /* expected_active */);

  // Tether enabled
  SetTetherTechnologyStateEnabled(false);
  SetTetherUserPrefState(false);
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);

  SetTetherTechnologyStateEnabled(true);
  SetTetherUserPrefState(true);
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);

  SetCellularTechnologyStateEnabled(false);

  VerifyTetherFeatureStateRecorded(
      TetherService::TetherFeatureState::CELLULAR_DISABLED,
      0 /* expected_count */);

  VerifyLastShutdownReason(TetherComponent::ShutdownReason::PREF_DISABLED);
}

// TODO(https://crbug.com/893878): Fix disabled test.
TEST_F(TetherServiceTest, DISABLED_TestDisabled) {
  profile_->GetPrefs()->SetBoolean(
      multidevice_setup::kInstantTetheringEnabledPrefName, false);

  CreateTetherService();

  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(
      multidevice_setup::kInstantTetheringEnabledPrefName));
  VerifyTetherActiveStatus(false /* expected_active */);

  VerifyTetherFeatureStateRecorded(
      TetherService::TetherFeatureState::USER_PREFERENCE_DISABLED,
      1 /* expected_count */);
}

TEST_F(TetherServiceTest, TestEnabled) {
  CreateTetherService();

  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);

  SetTetherTechnologyStateEnabled(false);
  SetTetherUserPrefState(false);
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(
      multidevice_setup::kInstantTetheringEnabledPrefName));
  VerifyTetherActiveStatus(false /* expected_active */);
  histogram_tester_.ExpectBucketCount(
      "InstantTethering.UserPreference.OnToggle", false,
      1u /* expected_count */);

  SetTetherTechnologyStateEnabled(true);
  SetTetherUserPrefState(true);
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      multidevice_setup::kInstantTetheringEnabledPrefName));
  VerifyTetherActiveStatus(true /* expected_active */);
  histogram_tester_.ExpectBucketCount(
      "InstantTethering.UserPreference.OnToggle", true,
      1u /* expected_count */);

  VerifyTetherFeatureStateRecorded(TetherService::TetherFeatureState::ENABLED,
                                   2 /* expected_count */);
  VerifyLastShutdownReason(TetherComponent::ShutdownReason::PREF_DISABLED);
}

TEST_F(TetherServiceTest, TestUserPrefChangesViaFeatureStateChange) {
  // Start with the feature disabled.
  initial_feature_state_ =
      multidevice_setup::mojom::FeatureState::kDisabledByUser;
  profile_->GetPrefs()->SetBoolean(
      multidevice_setup::kInstantTetheringEnabledPrefName, false);
  CreateTetherService();

  // Enable the feature.
  profile_->GetPrefs()->SetBoolean(
      multidevice_setup::kInstantTetheringEnabledPrefName, true);
  fake_multidevice_setup_client_->SetFeatureState(
      multidevice_setup::mojom::Feature::kInstantTethering,
      multidevice_setup::mojom::FeatureState::kEnabledByUser);
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);
  VerifyTetherFeatureStateRecorded(TetherService::TetherFeatureState::ENABLED,
                                   1 /* expected_count */);
  histogram_tester_.ExpectBucketCount(
      "InstantTethering.UserPreference.OnToggle", true,
      1u /* expected_count */);

  // Disable the feature.
  profile_->GetPrefs()->SetBoolean(
      multidevice_setup::kInstantTetheringEnabledPrefName, false);
  fake_multidevice_setup_client_->SetFeatureState(
      multidevice_setup::mojom::Feature::kInstantTethering,
      multidevice_setup::mojom::FeatureState::kDisabledByUser);
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);
  VerifyTetherFeatureStateRecorded(
      TetherService::TetherFeatureState::USER_PREFERENCE_DISABLED,
      2 /* expected_count */);
  histogram_tester_.ExpectBucketCount(
      "InstantTethering.UserPreference.OnToggle", false,
      1u /* expected_count */);

  // Enable the feature.
  profile_->GetPrefs()->SetBoolean(
      multidevice_setup::kInstantTetheringEnabledPrefName, true);
  fake_multidevice_setup_client_->SetFeatureState(
      multidevice_setup::mojom::Feature::kInstantTethering,
      multidevice_setup::mojom::FeatureState::kEnabledByUser);
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);
  VerifyTetherFeatureStateRecorded(TetherService::TetherFeatureState::ENABLED,
                                   2 /* expected_count */);
  histogram_tester_.ExpectBucketCount(
      "InstantTethering.UserPreference.OnToggle", true,
      2u /* expected_count */);

  VerifyLastShutdownReason(TetherComponent::ShutdownReason::PREF_DISABLED);
}

TEST_F(TetherServiceTest, TestUserPrefChangesViaTechnologyStateChange) {
  CreateTetherService();

  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);

  SetTetherTechnologyStateEnabled(false);
  SetTetherUserPrefState(false);
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_AVAILABLE,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(false /* expected_active */);
  histogram_tester_.ExpectBucketCount(
      "InstantTethering.UserPreference.OnToggle", false,
      1u /* expected_count */);

  SetTetherTechnologyStateEnabled(true);
  SetTetherUserPrefState(true);
  EXPECT_EQ(NetworkStateHandler::TechnologyState::TECHNOLOGY_ENABLED,
            network_state_handler()->GetTechnologyState(
                NetworkTypePattern::Tether()));
  VerifyTetherActiveStatus(true /* expected_active */);
  histogram_tester_.ExpectBucketCount(
      "InstantTethering.UserPreference.OnToggle", true,
      1u /* expected_count */);

  VerifyTetherFeatureStateRecorded(TetherService::TetherFeatureState::ENABLED,
                                   2 /* expected_count */);
  VerifyLastShutdownReason(TetherComponent::ShutdownReason::PREF_DISABLED);
}

// Test against a past defect that made TetherService and NetworkStateHandler
// repeatly update technology state after the other did so. TetherService should
// only update technology state if NetworkStateHandler has provided a different
// state than the user preference.
TEST_F(TetherServiceTest, TestEnabledMultipleChanges) {
  CreateTetherService();

  // CreateTetherService calls RunUntilIdle() so UpdateTetherTechnologyState()
  // may be called multiple times in the initialization process.
  int updated_technology_state_count =
      tether_service_->updated_technology_state_count();

  SetTetherTechnologyStateEnabled(false);
  SetTetherTechnologyStateEnabled(false);
  SetTetherTechnologyStateEnabled(false);
  SetTetherUserPrefState(false);

  updated_technology_state_count++;
  EXPECT_EQ(updated_technology_state_count,
            tether_service_->updated_technology_state_count());

  SetTetherTechnologyStateEnabled(true);
  SetTetherTechnologyStateEnabled(true);
  SetTetherTechnologyStateEnabled(true);
  SetTetherUserPrefState(true);

  updated_technology_state_count++;
  EXPECT_EQ(updated_technology_state_count,
            tether_service_->updated_technology_state_count());

  VerifyLastShutdownReason(TetherComponent::ShutdownReason::PREF_DISABLED);
}

}  // namespace tether
}  // namespace ash
