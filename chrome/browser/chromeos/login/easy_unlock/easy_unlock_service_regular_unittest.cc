// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_notification_controller.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service_factory.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service_regular.h"
#include "chrome/browser/chromeos/login/session/chrome_session_manager.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_dialog.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/components/multidevice/beacon_seed.h"
#include "chromeos/components/multidevice/remote_device_test_util.h"
#include "chromeos/components/proximity_auth/fake_lock_handler.h"
#include "chromeos/components/proximity_auth/screenlock_bridge.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "components/account_id/account_id.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/views/test/test_views_delegate.h"

using device::MockBluetoothAdapter;
using testing::_;
using testing::AnyNumber;
using testing::Return;

namespace chromeos {

namespace {

class MockEasyUnlockNotificationController
    : public EasyUnlockNotificationController {
 public:
  MockEasyUnlockNotificationController()
      : EasyUnlockNotificationController(nullptr) {}
  ~MockEasyUnlockNotificationController() override {}

  // EasyUnlockNotificationController:
  MOCK_METHOD0(ShowChromebookAddedNotification, void());
  MOCK_METHOD0(ShowPairingChangeNotification, void());
  MOCK_METHOD1(ShowPairingChangeAppliedNotification, void(const std::string&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockEasyUnlockNotificationController);
};

// Define a stub MultiDeviceSetupDialog because the base class has a protected
// constructor and destructor (which prevents usage of smart pointers).
class FakeMultiDeviceSetupDialog
    : public multidevice_setup::MultiDeviceSetupDialog {
 public:
  FakeMultiDeviceSetupDialog() = default;
};

}  // namespace

class EasyUnlockServiceRegularTest : public testing::Test {
 protected:
  EasyUnlockServiceRegularTest()
      : test_local_device_(
            multidevice::RemoteDeviceRefBuilder()
                .SetPublicKey("local device")
                .SetSoftwareFeatureState(
                    chromeos::multidevice::SoftwareFeature::kSmartLockClient,
                    chromeos::multidevice::SoftwareFeatureState::kEnabled)
                .SetBeaconSeeds(multidevice::FromCryptAuthSeedList(
                    std::vector<cryptauth::BeaconSeed>(4)))
                .Build()),
        test_remote_device_smart_lock_host_(
            multidevice::RemoteDeviceRefBuilder()
                .SetPublicKey("local device")
                .SetSoftwareFeatureState(
                    chromeos::multidevice::SoftwareFeature::kSmartLockHost,
                    chromeos::multidevice::SoftwareFeatureState::kEnabled)
                .SetBeaconSeeds(multidevice::FromCryptAuthSeedList(
                    std::vector<cryptauth::BeaconSeed>(4)))
                .Build()) {}
  ~EasyUnlockServiceRegularTest() override = default;

  void SetUp() override {
    PowerManagerClient::InitializeFake();

    mock_adapter_ = new testing::NiceMock<MockBluetoothAdapter>();
    device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);
    EXPECT_CALL(*mock_adapter_, IsPresent())
        .WillRepeatedly(testing::Invoke(
            this, &EasyUnlockServiceRegularTest::is_bluetooth_adapter_present));

    TestingBrowserProcess::GetGlobal()->SetLocalState(&local_pref_service_);
    RegisterLocalState(local_pref_service_.registry());

    auto test_other_remote_device =
        multidevice::RemoteDeviceRefBuilder()
            .SetPublicKey("potential, but disabled, host device")
            .SetSoftwareFeatureState(
                chromeos::multidevice::SoftwareFeature::kSmartLockHost,
                chromeos::multidevice::SoftwareFeatureState::kSupported)
            .Build();
    test_remote_devices_.push_back(test_remote_device_smart_lock_host_);
    test_remote_devices_.push_back(test_other_remote_device);

    fake_secure_channel_client_ =
        std::make_unique<chromeos::secure_channel::FakeSecureChannelClient>();
    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_multidevice_setup_client_ = std::make_unique<
        chromeos::multidevice_setup::FakeMultiDeviceSetupClient>();

    TestingProfile::Builder builder;
    profile_ = builder.Build();

    account_id_ = AccountId::FromUserEmail(profile_->GetProfileUserName());

    auto fake_chrome_user_manager =
        std::make_unique<chromeos::FakeChromeUserManager>();
    fake_chrome_user_manager_ = fake_chrome_user_manager.get();
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_chrome_user_manager));
    SetPrimaryUserLoggedIn();

    fake_lock_handler_ = std::make_unique<proximity_auth::FakeLockHandler>();
    SetScreenLockState(false /* is_locked */);
  }

  void TearDown() override {
    SetScreenLockState(false /* is_locked */);
    easy_unlock_service_regular_->Shutdown();
    PowerManagerClient::Shutdown();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }

  // Most tests will want to pass |should_initialize_all_dependencies| == true,
  // but may pass false if they wish to tweak the dependencies' state themselves
  // before initializing the service.
  void InitializeService(bool should_initialize_all_dependencies) {
    if (should_initialize_all_dependencies) {
      fake_device_sync_client_->NotifyReady();
      SetLocalDevice(test_local_device_);
      SetSyncedDevices(test_remote_devices_);
      SetIsEnabled(true /* is_enabled */);
    }

    auto mock_notification_controller = std::make_unique<
        testing::StrictMock<MockEasyUnlockNotificationController>>();
    mock_notification_controller_ = mock_notification_controller.get();

    easy_unlock_service_regular_ = std::make_unique<EasyUnlockServiceRegular>(
        profile_.get(), fake_secure_channel_client_.get(),
        std::move(mock_notification_controller), fake_device_sync_client_.get(),
        fake_multidevice_setup_client_.get());
    easy_unlock_service_regular_->Initialize();
  }

  void SetLocalDevice(
      const base::Optional<multidevice::RemoteDeviceRef>& local_device) {
    fake_device_sync_client_->set_local_device_metadata(test_local_device_);
    fake_device_sync_client_->NotifyEnrollmentFinished();
  }

  void SetSyncedDevices(multidevice::RemoteDeviceRefList synced_devices) {
    fake_device_sync_client_->set_synced_devices(synced_devices);
    fake_device_sync_client_->NotifyNewDevicesSynced();
  }

  void SetIsEnabled(bool is_enabled) {
    fake_multidevice_setup_client_->SetFeatureState(
        chromeos::multidevice_setup::mojom::Feature::kSmartLock,
        is_enabled
            ? chromeos::multidevice_setup::mojom::FeatureState::kEnabledByUser
            : chromeos::multidevice_setup::mojom::FeatureState::
                  kDisabledByUser);
  }

  void SetEasyUnlockAllowedPolicy(bool allowed) {
    profile_->GetTestingPrefService()->SetManagedPref(
        prefs::kEasyUnlockAllowed, std::make_unique<base::Value>(allowed));
  }

  void set_is_bluetooth_adapter_present(bool is_present) {
    is_bluetooth_adapter_present_ = is_present;
  }

  bool is_bluetooth_adapter_present() const {
    return is_bluetooth_adapter_present_;
  }

  void SetScreenLockState(bool is_locked) {
    if (is_locked)
      proximity_auth::ScreenlockBridge::Get()->SetFocusedUser(account_id_);

    proximity_auth::ScreenlockBridge::Get()->SetLockHandler(
        is_locked ? fake_lock_handler_.get() : nullptr);
  }

  void VerifyGetRemoteDevices(bool are_local_and_remote_devices_expected) {
    const base::ListValue* remote_devices =
        static_cast<EasyUnlockService*>(easy_unlock_service_regular_.get())
            ->GetRemoteDevices();
    if (are_local_and_remote_devices_expected)
      // 2 devices are expected: the local device and the remote device.
      EXPECT_EQ(2u, remote_devices->GetSize());
    else
      EXPECT_FALSE(remote_devices);
  }

  // Must outlive TestingProfiles.
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;
  AccountId account_id_;
  chromeos::FakeChromeUserManager* fake_chrome_user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;

  const multidevice::RemoteDeviceRef test_local_device_;
  const multidevice::RemoteDeviceRef test_remote_device_smart_lock_host_;
  multidevice::RemoteDeviceRefList test_remote_devices_;

  std::unique_ptr<secure_channel::FakeSecureChannelClient>
      fake_secure_channel_client_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;
  std::unique_ptr<proximity_auth::FakeLockHandler> fake_lock_handler_;
  std::unique_ptr<EasyUnlockServiceRegular> easy_unlock_service_regular_;

  std::string profile_gaia_id_;

  bool is_bluetooth_adapter_present_ = true;
  scoped_refptr<testing::NiceMock<MockBluetoothAdapter>> mock_adapter_;

  testing::StrictMock<MockEasyUnlockNotificationController>*
      mock_notification_controller_;

  // PrefService which contains the browser process' local storage.
  TestingPrefServiceSimple local_pref_service_;

  views::TestViewsDelegate view_delegate_;
  base::HistogramTester histogram_tester_;

 private:
  void SetPrimaryUserLoggedIn() {
    const user_manager::User* user =
        fake_chrome_user_manager_->AddPublicAccountUser(account_id_);
    fake_chrome_user_manager_->UserLoggedIn(account_id_, user->username_hash(),
                                            false /* browser_restart */,
                                            false /* is_child */);
  }

  DISALLOW_COPY_AND_ASSIGN(EasyUnlockServiceRegularTest);
};

TEST_F(EasyUnlockServiceRegularTest, NoBluetoothNoService) {
  InitializeService(true /* should_initialize_all_dependencies */);

  set_is_bluetooth_adapter_present(false);
  EXPECT_FALSE(easy_unlock_service_regular_->IsAllowed());
}

TEST_F(EasyUnlockServiceRegularTest, NotAllowedWhenProhibited) {
  InitializeService(true /* should_initialize_all_dependencies */);
  fake_multidevice_setup_client_->SetFeatureState(
      chromeos::multidevice_setup::mojom::Feature::kSmartLock,
      chromeos::multidevice_setup::mojom::FeatureState::kProhibitedByPolicy);

  EXPECT_FALSE(easy_unlock_service_regular_->IsAllowed());
}

TEST_F(EasyUnlockServiceRegularTest, NotAllowedForEphemeralAccounts) {
  InitializeService(true /* should_initialize_all_dependencies */);

  // Only MockUserManager allows for stubbing
  // IsCurrentUserNonCryptohomeDataEphemeral() to return false so we use one
  // here in place of |fake_chrome_user_manager_|. Injecting it into a local
  // ScopedUserManager sets it up as the global UserManager instance.
  auto mock_user_manager =
      std::make_unique<testing::NiceMock<MockUserManager>>();
  ON_CALL(*mock_user_manager, IsCurrentUserNonCryptohomeDataEphemeral())
      .WillByDefault(Return(false));
  auto scoped_user_manager = std::make_unique<user_manager::ScopedUserManager>(
      std::move(mock_user_manager));

  EXPECT_FALSE(easy_unlock_service_regular_->IsAllowed());
}

TEST_F(EasyUnlockServiceRegularTest, GetProximityAuthPrefManager) {
  InitializeService(true /* should_initialize_all_dependencies */);

  EXPECT_TRUE(
      static_cast<EasyUnlockService*>(easy_unlock_service_regular_.get())
          ->GetProximityAuthPrefManager());
}

TEST_F(EasyUnlockServiceRegularTest, GetRemoteDevices) {
  InitializeService(true /* should_initialize_all_dependencies */);
  VerifyGetRemoteDevices(true /* are_local_and_remote_devices_expected */);
}

TEST_F(EasyUnlockServiceRegularTest, GetRemoteDevices_InitiallyNotReady) {
  SetIsEnabled(true /* is_enabled */);
  InitializeService(false /* should_initialize_all_dependencies */);
  VerifyGetRemoteDevices(false /* are_local_and_remote_devices_expected */);

  EXPECT_CALL(*mock_notification_controller_,
              ShowChromebookAddedNotification());

  fake_device_sync_client_->NotifyReady();
  SetLocalDevice(test_local_device_);
  SetSyncedDevices(test_remote_devices_);
  VerifyGetRemoteDevices(true /* are_local_and_remote_devices_expected */);
}

TEST_F(EasyUnlockServiceRegularTest,
       GetRemoteDevices_InitiallyNoSyncedDevices) {
  fake_device_sync_client_->NotifyReady();
  SetLocalDevice(test_local_device_);
  SetSyncedDevices(multidevice::RemoteDeviceRefList() /* synced_devices */);
  SetIsEnabled(true /* is_enabled */);
  InitializeService(false /* should_initialize_all_dependencies */);
  VerifyGetRemoteDevices(false /* are_local_and_remote_devices_expected */);

  EXPECT_CALL(*mock_notification_controller_,
              ShowChromebookAddedNotification());

  SetSyncedDevices(test_remote_devices_);
  VerifyGetRemoteDevices(true /* are_local_and_remote_devices_expected */);
}

// Test that the "Chromebook added" notification does not show while the
// MultiDeviceSetupDialog is active, and only shows once it is closed.
TEST_F(
    EasyUnlockServiceRegularTest,
    GetRemoteDevices_InitiallyNoSyncedDevices_MultiDeviceSetupDialogVisible) {
  ChromeSessionManager manager;

  auto dialog = std::make_unique<FakeMultiDeviceSetupDialog>();
  multidevice_setup::MultiDeviceSetupDialog::SetInstanceForTesting(
      dialog.get());

  fake_device_sync_client_->NotifyReady();
  SetLocalDevice(test_local_device_);
  SetSyncedDevices(multidevice::RemoteDeviceRefList() /* synced_devices */);
  SetIsEnabled(true /* is_enabled */);
  InitializeService(false /* should_initialize_all_dependencies */);
  VerifyGetRemoteDevices(false /* are_local_and_remote_devices_expected */);

  // Calling SetSyncedDevices() below would usually cause the "Chromebook added"
  // notification to appear, but it shouldn't because MultiDeviceSetupDialog is
  // active.
  EXPECT_CALL(*mock_notification_controller_, ShowChromebookAddedNotification())
      .Times(0);
  SetSyncedDevices(test_remote_devices_);
  VerifyGetRemoteDevices(true /* are_local_and_remote_devices_expected */);

  // Now expect the "Chromebook added" notification to appear, and close the
  // dialog by deleting it (this indirectly calls the dialog close callbacks).
  // Using the real dialog Close() method invokes Ash and Widget code that is
  // too cumbersome to stub out.
  testing::Mock::VerifyAndClearExpectations(mock_notification_controller_);
  EXPECT_CALL(*mock_notification_controller_,
              ShowChromebookAddedNotification());
  dialog.reset();
  multidevice_setup::MultiDeviceSetupDialog::SetInstanceForTesting(nullptr);
}

TEST_F(EasyUnlockServiceRegularTest, GetRemoteDevices_InitiallyNotEnabled) {
  fake_device_sync_client_->NotifyReady();
  SetLocalDevice(test_local_device_);
  SetSyncedDevices(test_remote_devices_);
  SetIsEnabled(false /* is_enabled */);
  InitializeService(false /* should_initialize_all_dependencies */);
  VerifyGetRemoteDevices(false /* are_local_and_remote_devices_expected */);

  SetIsEnabled(true /* is_enabled */);
  VerifyGetRemoteDevices(true /* are_local_and_remote_devices_expected */);
}

TEST_F(EasyUnlockServiceRegularTest,
       GetRemoteDevices_DeferDeviceLoadUntilScreenIsUnlocked) {
  SetScreenLockState(true /* is_locked */);
  InitializeService(true /* should_initialize_all_dependencies */);
  VerifyGetRemoteDevices(false /* are_local_and_remote_devices_expected */);

  SetScreenLockState(false /* is_locked */);
  VerifyGetRemoteDevices(true /* are_local_and_remote_devices_expected */);
}

TEST_F(EasyUnlockServiceRegularTest, GetRemoteDevices_SmartLockHostChanged) {
  InitializeService(true /* should_initialize_all_dependencies */);
  SetScreenLockState(true /* is_locked */);
  VerifyGetRemoteDevices(true /* are_local_and_remote_devices_expected */);

  auto new_remote_device =
      multidevice::RemoteDeviceRefBuilder()
          .SetPublicKey("new smartlock host")
          .SetSoftwareFeatureState(
              chromeos::multidevice::SoftwareFeature::kSmartLockHost,
              chromeos::multidevice::SoftwareFeatureState::kEnabled)
          .Build();
  multidevice::RemoteDeviceRefList new_remote_devices;
  new_remote_devices.push_back(new_remote_device);

  EXPECT_CALL(*mock_notification_controller_, ShowPairingChangeNotification());
  SetSyncedDevices(new_remote_devices /* synced_devices */);
  VerifyGetRemoteDevices(true /* are_local_and_remote_devices_expected */);

  EXPECT_CALL(*mock_notification_controller_,
              ShowPairingChangeAppliedNotification(testing::_));
  SetScreenLockState(false /* is_locked */);
}

// Test through the core flow of unlocking the screen with Smart Lock.
// Unfortunately, the only observable side effect we have available is verifying
// that the success metric is emitted.
TEST_F(EasyUnlockServiceRegularTest, AuthenticateWithEasyUnlock) {
  InitializeService(true /* should_initialize_all_dependencies */);
  SetScreenLockState(true /* is_locked */);

  static_cast<EasyUnlockService*>(easy_unlock_service_regular_.get())
      ->AttemptAuth(account_id_);
  static_cast<EasyUnlockService*>(easy_unlock_service_regular_.get())
      ->FinalizeUnlock(true);

  histogram_tester_.ExpectBucketCount("SmartLock.AuthResult.Unlock", 1, 0);

  SetScreenLockState(false /* is_locked */);

  histogram_tester_.ExpectBucketCount("SmartLock.AuthResult.Unlock", 1, 1);
}

}  // namespace chromeos
