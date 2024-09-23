// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/smart_lock/smart_lock_service.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "chrome/browser/ash/login/session/chrome_session_manager.h"
#include "chrome/browser/ash/login/smart_lock/smart_lock_feature_usage_metrics.h"
#include "chrome/browser/ash/login/smart_lock/smart_lock_notification_controller.h"
#include "chrome/browser/ash/login/smart_lock/smart_lock_service_factory.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/webui/ash/multidevice_setup/multidevice_setup_dialog.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/multidevice/beacon_seed.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "chromeos/ash/components/proximity_auth/fake_lock_handler.h"
#include "chromeos/ash/components/proximity_auth/screenlock_bridge.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/ash/services/device_sync/public/cpp/fake_device_sync_client.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_secure_channel_client.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/display/display.h"
#include "ui/display/test/test_screen.h"
#include "ui/display/util/display_util.h"
#include "ui/views/test/test_views_delegate.h"

namespace ash {
namespace {

using ::device::MockBluetoothAdapter;
using ::testing::Return;

struct SmartLockStateTestCase {
  SmartLockState smart_lock_state;
  SmartLockAuthEvent smart_lock_auth_event;
  SmartLockMetricsRecorder::SmartLockAuthEventPasswordState
      smart_lock_auth_event_password_state;
  bool should_be_valid_on_remote_auth_failure;
};

constexpr SmartLockStateTestCase kSmartLockStateTestCases[] = {
    {SmartLockState::kInactive,
     SmartLockAuthEvent::PASSWORD_ENTRY_SERVICE_NOT_ACTIVE,
     SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
         kServiceNotActive,
     true},
    {SmartLockState::kDisabled,
     SmartLockAuthEvent::PASSWORD_ENTRY_SERVICE_NOT_ACTIVE,
     SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
         kServiceNotActive,
     true},
    {SmartLockState::kBluetoothDisabled,
     SmartLockAuthEvent::PASSWORD_ENTRY_NO_BLUETOOTH,
     SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::kNoBluetooth,
     true},
    {SmartLockState::kConnectingToPhone,
     SmartLockAuthEvent::PASSWORD_ENTRY_BLUETOOTH_CONNECTING,
     SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
         kBluetoothConnecting,
     false},
    {SmartLockState::kPhoneNotFound,
     SmartLockAuthEvent::PASSWORD_ENTRY_NO_PHONE,
     SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
         kCouldNotConnectToPhone,
     false},
    {SmartLockState::kPhoneNotAuthenticated,
     SmartLockAuthEvent::PASSWORD_ENTRY_PHONE_NOT_AUTHENTICATED,
     SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
         kNotAuthenticated,
     false},
    {SmartLockState::kPhoneFoundLockedAndProximate,
     SmartLockAuthEvent::PASSWORD_ENTRY_PHONE_LOCKED,
     SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::kPhoneLocked,
     true},
    {SmartLockState::kPhoneFoundUnlockedAndDistant,
     SmartLockAuthEvent::PASSWORD_ENTRY_RSSI_TOO_LOW,
     SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::kRssiTooLow,
     false},
    {SmartLockState::kPhoneFoundLockedAndDistant,
     SmartLockAuthEvent::PASSWORD_ENTRY_PHONE_LOCKED_AND_RSSI_TOO_LOW,
     SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
         kPhoneLockedAndRssiTooLow,
     false},
    {SmartLockState::kPhoneAuthenticated,
     SmartLockAuthEvent::PASSWORD_ENTRY_WITH_AUTHENTICATED_PHONE,
     SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
         kAuthenticatedPhone,
     false},
    {SmartLockState::kPhoneNotLockable,
     SmartLockAuthEvent::PASSWORD_ENTRY_PHONE_NOT_LOCKABLE,
     SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
         kPhoneNotLockable,
     false},
    {SmartLockState::kPrimaryUserAbsent,
     SmartLockAuthEvent::PASSWORD_ENTRY_PRIMARY_USER_ABSENT,
     SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::
         kPrimaryUserAbsent,
     false}};

class MockSmartLockNotificationController
    : public SmartLockNotificationController {
 public:
  MockSmartLockNotificationController()
      : SmartLockNotificationController(nullptr) {}

  MockSmartLockNotificationController(
      const MockSmartLockNotificationController&) = delete;
  MockSmartLockNotificationController& operator=(
      const MockSmartLockNotificationController&) = delete;

  ~MockSmartLockNotificationController() override {}

  // SmartLockNotificationController:
  MOCK_METHOD0(ShowChromebookAddedNotification, void());
  MOCK_METHOD0(ShowPairingChangeNotification, void());
  MOCK_METHOD1(ShowPairingChangeAppliedNotification, void(const std::string&));
};

// Define a stub MultiDeviceSetupDialog because the base class has a protected
// constructor and destructor (which prevents usage of smart pointers).
class FakeMultiDeviceSetupDialog
    : public multidevice_setup::MultiDeviceSetupDialog {
 public:
  FakeMultiDeviceSetupDialog() = default;
};

}  // namespace

class SmartLockServiceTest : public testing::Test {
 public:
  SmartLockServiceTest(const SmartLockServiceTest&) = delete;
  SmartLockServiceTest& operator=(const SmartLockServiceTest&) = delete;

 protected:
  SmartLockServiceTest()
      : test_local_device_(
            multidevice::RemoteDeviceRefBuilder()
                .SetPublicKey("local device")
                .SetSoftwareFeatureState(
                    multidevice::SoftwareFeature::kSmartLockClient,
                    multidevice::SoftwareFeatureState::kEnabled)
                .SetBeaconSeeds(multidevice::FromCryptAuthSeedList(
                    std::vector<cryptauth::BeaconSeed>(4)))
                .Build()),
        test_remote_device_smart_lock_host_(
            multidevice::RemoteDeviceRefBuilder()
                .SetPublicKey("local device")
                .SetSoftwareFeatureState(
                    multidevice::SoftwareFeature::kSmartLockHost,
                    multidevice::SoftwareFeatureState::kEnabled)
                .SetBeaconSeeds(multidevice::FromCryptAuthSeedList(
                    std::vector<cryptauth::BeaconSeed>(4)))
                .Build()) {}
  ~SmartLockServiceTest() override = default;

  void SetUp() override {
    display::Screen::SetScreenInstance(&test_screen_);
    display::SetInternalDisplayIds({test_screen_.GetPrimaryDisplay().id()});

    chromeos::PowerManagerClient::InitializeFake();

    // Note: this is necessary because objects owned by SmartLockService
    // depend on the BluetoothAdapter -- fetching the real one causes tests
    // to fail.
    mock_adapter_ = new testing::NiceMock<MockBluetoothAdapter>();
    device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);

    TestingBrowserProcess::GetGlobal()->SetLocalState(&local_pref_service_);
    RegisterLocalState(local_pref_service_.registry());
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());

    auto test_other_remote_device =
        multidevice::RemoteDeviceRefBuilder()
            .SetPublicKey("potential, but disabled, host device")
            .SetSoftwareFeatureState(
                multidevice::SoftwareFeature::kSmartLockHost,
                multidevice::SoftwareFeatureState::kSupported)
            .Build();
    test_remote_devices_.push_back(test_remote_device_smart_lock_host_);
    test_remote_devices_.push_back(test_other_remote_device);

    fake_secure_channel_client_ =
        std::make_unique<secure_channel::FakeSecureChannelClient>();
    fake_device_sync_client_ =
        std::make_unique<device_sync::FakeDeviceSyncClient>();
    fake_multidevice_setup_client_ =
        std::make_unique<multidevice_setup::FakeMultiDeviceSetupClient>();

    TestingProfile::Builder builder;
    profile_ = builder.Build();

    account_id_ = AccountId::FromUserEmail(profile_->GetProfileUserName());

    SetPrimaryUserLoggedIn();

    fake_lock_handler_ = std::make_unique<proximity_auth::FakeLockHandler>();
    SetScreenLockState(false /* is_locked */);
  }

  void TearDown() override {
    SetScreenLockState(false /* is_locked */);
    smart_lock_service_->Shutdown();
    chromeos::PowerManagerClient::Shutdown();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
    display::Screen::SetScreenInstance(nullptr);
  }

  // Most tests will want to pass `should_initialize_all_dependencies` == true,
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
        testing::StrictMock<MockSmartLockNotificationController>>();
    mock_notification_controller_ = mock_notification_controller.get();

    smart_lock_service_ = std::make_unique<SmartLockService>(
        profile_.get(), fake_secure_channel_client_.get(),
        std::move(mock_notification_controller), fake_device_sync_client_.get(),
        fake_multidevice_setup_client_.get());
    smart_lock_service_->Initialize();
  }

  void SetLocalDevice(
      const std::optional<multidevice::RemoteDeviceRef>& local_device) {
    fake_device_sync_client_->set_local_device_metadata(test_local_device_);
    fake_device_sync_client_->NotifyEnrollmentFinished();
  }

  void SetSyncedDevices(multidevice::RemoteDeviceRefList synced_devices) {
    fake_device_sync_client_->set_synced_devices(synced_devices);
    fake_device_sync_client_->NotifyNewDevicesSynced();
  }

  void SetIsEnabled(bool is_enabled) {
    fake_multidevice_setup_client_->SetFeatureState(
        multidevice_setup::mojom::Feature::kSmartLock,
        is_enabled ? multidevice_setup::mojom::FeatureState::kEnabledByUser
                   : multidevice_setup::mojom::FeatureState::kDisabledByUser);
  }

  void SetSmartLockAllowedPolicy(bool allowed) {
    profile_->GetTestingPrefService()->SetManagedPref(
        prefs::kEasyUnlockAllowed, std::make_unique<base::Value>(allowed));
  }

  void SetScreenLockState(bool is_locked) {
    if (is_locked) {
      proximity_auth::ScreenlockBridge::Get()->SetFocusedUser(account_id_);
    }

    fake_lock_handler_->ClearSmartLockState();
    fake_lock_handler_->ClearSmartLockAuthResult();

    proximity_auth::ScreenlockBridge::Get()->SetLockHandler(
        is_locked ? fake_lock_handler_.get() : nullptr);
  }

  void VerifyGetRemoteDevices(bool are_remote_devices_expected) {
    const multidevice::RemoteDeviceRefList remote_devices =
        smart_lock_service_->GetRemoteDevicesForTesting();
    if (are_remote_devices_expected) {
      EXPECT_FALSE(remote_devices.empty());
    } else {
      EXPECT_TRUE(remote_devices.empty());
    }
  }

  void SetDisplaySize(const gfx::Size& size) {
    display::Display display = test_screen_.GetPrimaryDisplay();
    display.SetSize(size);
    test_screen_.display_list().RemoveDisplay(display.id());
    test_screen_.display_list().AddDisplay(display,
                                           display::DisplayList::Type::PRIMARY);
  }

  SmartLockAuthEvent GetPasswordAuthEvent() {
    return smart_lock_service_->GetPasswordAuthEvent();
  }

  SmartLockMetricsRecorder::SmartLockAuthEventPasswordState
  GetSmartUnlockPasswordAuthEvent() {
    return smart_lock_service_->GetSmartUnlockPasswordAuthEvent();
  }

  void ResetSmartLockState() { smart_lock_service_->ResetSmartLockState(); }

  // Must outlive TestingProfiles.
  content::BrowserTaskEnvironment task_environment_;

  // PrefService which contains the browser process' local storage. It should be
  // destructed after TestingProfile.
  TestingPrefServiceSimple local_pref_service_;

  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  std::unique_ptr<TestingProfile> profile_;
  AccountId account_id_;

  const multidevice::RemoteDeviceRef test_local_device_;
  const multidevice::RemoteDeviceRef test_remote_device_smart_lock_host_;
  multidevice::RemoteDeviceRefList test_remote_devices_;

  std::unique_ptr<secure_channel::FakeSecureChannelClient>
      fake_secure_channel_client_;
  std::unique_ptr<device_sync::FakeDeviceSyncClient> fake_device_sync_client_;
  std::unique_ptr<multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;
  std::unique_ptr<proximity_auth::FakeLockHandler> fake_lock_handler_;
  std::unique_ptr<SmartLockService> smart_lock_service_;

  std::string profile_gaia_id_;

  scoped_refptr<testing::NiceMock<MockBluetoothAdapter>> mock_adapter_;

  raw_ptr<testing::StrictMock<MockSmartLockNotificationController>,
          DanglingUntriaged>
      mock_notification_controller_;

  views::TestViewsDelegate view_delegate_;
  base::HistogramTester histogram_tester_;

  display::test::TestScreen test_screen_;

 private:
  void SetPrimaryUserLoggedIn() {
    const user_manager::User* user =
        fake_user_manager_->AddPublicAccountUser(account_id_);
    fake_user_manager_->UserLoggedIn(account_id_, user->username_hash(),
                                     false /* browser_restart */,
                                     false /* is_child */);
  }
};

TEST_F(SmartLockServiceTest, NotAllowedWhenProhibited) {
  InitializeService(true /* should_initialize_all_dependencies */);
  fake_multidevice_setup_client_->SetFeatureState(
      multidevice_setup::mojom::Feature::kSmartLock,
      multidevice_setup::mojom::FeatureState::kProhibitedByPolicy);

  EXPECT_FALSE(smart_lock_service_->IsAllowed());
}

TEST_F(SmartLockServiceTest, NotAllowedForEphemeralAccounts) {
  InitializeService(true /* should_initialize_all_dependencies */);
  fake_user_manager_->set_current_user_ephemeral(true);
  EXPECT_FALSE(smart_lock_service_->IsAllowed());
}

TEST_F(SmartLockServiceTest, GetRemoteDevices) {
  InitializeService(true /* should_initialize_all_dependencies */);
  VerifyGetRemoteDevices(true /* are_remote_devices_expected */);
}

TEST_F(SmartLockServiceTest, GetRemoteDevices_InitiallyNotReady) {
  SetIsEnabled(true /* is_enabled */);
  InitializeService(false /* should_initialize_all_dependencies */);
  VerifyGetRemoteDevices(false /* are_remote_devices_expected */);

  EXPECT_CALL(*mock_notification_controller_,
              ShowChromebookAddedNotification());

  fake_device_sync_client_->NotifyReady();
  SetLocalDevice(test_local_device_);
  SetSyncedDevices(test_remote_devices_);
  VerifyGetRemoteDevices(true /* are_remote_devices_expected */);
}

TEST_F(SmartLockServiceTest, GetRemoteDevices_InitiallyNoSyncedDevices) {
  fake_device_sync_client_->NotifyReady();
  SetLocalDevice(test_local_device_);
  SetSyncedDevices(multidevice::RemoteDeviceRefList() /* synced_devices */);
  SetIsEnabled(true /* is_enabled */);
  InitializeService(false /* should_initialize_all_dependencies */);
  VerifyGetRemoteDevices(false /* are_remote_devices_expected */);

  EXPECT_CALL(*mock_notification_controller_,
              ShowChromebookAddedNotification());

  SetSyncedDevices(test_remote_devices_);
  VerifyGetRemoteDevices(true /* are_remote_devices_expected */);
}

// Test that the "Chromebook added" notification does not show while the
// MultiDeviceSetupDialog is active, and only shows once it is closed.
TEST_F(
    SmartLockServiceTest,
    GetRemoteDevices_InitiallyNoSyncedDevices_MultiDeviceSetupDialogVisible) {
  SetDisplaySize(gfx::Size(1920, 1200));

  ChromeSessionManager manager;
  manager.OnUserManagerCreated(fake_user_manager_.Get());

  auto dialog = std::make_unique<FakeMultiDeviceSetupDialog>();
  multidevice_setup::MultiDeviceSetupDialog::SetInstanceForTesting(
      dialog.get());

  fake_device_sync_client_->NotifyReady();
  SetLocalDevice(test_local_device_);
  SetSyncedDevices(multidevice::RemoteDeviceRefList() /* synced_devices */);
  SetIsEnabled(true /* is_enabled */);
  InitializeService(false /* should_initialize_all_dependencies */);
  VerifyGetRemoteDevices(false /* are_remote_devices_expected */);

  // Calling SetSyncedDevices() below would usually cause the "Chromebook added"
  // notification to appear, but it shouldn't because MultiDeviceSetupDialog is
  // active.
  EXPECT_CALL(*mock_notification_controller_, ShowChromebookAddedNotification())
      .Times(0);
  SetSyncedDevices(test_remote_devices_);
  VerifyGetRemoteDevices(true /* are_remote_devices_expected */);

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

TEST_F(SmartLockServiceTest, GetRemoteDevices_InitiallyNotEnabled) {
  fake_device_sync_client_->NotifyReady();
  SetLocalDevice(test_local_device_);
  SetSyncedDevices(test_remote_devices_);
  SetIsEnabled(false /* is_enabled */);
  InitializeService(false /* should_initialize_all_dependencies */);
  VerifyGetRemoteDevices(false /* are_remote_devices_expected */);

  SetIsEnabled(true /* is_enabled */);
  VerifyGetRemoteDevices(true /* are_remote_devices_expected */);
}

TEST_F(SmartLockServiceTest,
       GetRemoteDevices_DeferDeviceLoadUntilScreenIsUnlocked) {
  SetScreenLockState(true /* is_locked */);
  InitializeService(true /* should_initialize_all_dependencies */);
  VerifyGetRemoteDevices(false /* are_remote_devices_expected */);

  SetScreenLockState(false /* is_locked */);
  VerifyGetRemoteDevices(true /* are_remote_devices_expected */);
}

TEST_F(SmartLockServiceTest, GetRemoteDevices_SmartLockHostChanged) {
  InitializeService(true /* should_initialize_all_dependencies */);
  SetScreenLockState(true /* is_locked */);
  VerifyGetRemoteDevices(true /* are_remote_devices_expected */);

  auto new_remote_device =
      multidevice::RemoteDeviceRefBuilder()
          .SetPublicKey("new smartlock host")
          .SetSoftwareFeatureState(multidevice::SoftwareFeature::kSmartLockHost,
                                   multidevice::SoftwareFeatureState::kEnabled)
          .Build();
  multidevice::RemoteDeviceRefList new_remote_devices;
  new_remote_devices.push_back(new_remote_device);

  EXPECT_CALL(*mock_notification_controller_, ShowPairingChangeNotification());
  SetSyncedDevices(new_remote_devices /* synced_devices */);
  VerifyGetRemoteDevices(true /* are_remote_devices_expected */);

  EXPECT_CALL(*mock_notification_controller_,
              ShowPairingChangeAppliedNotification(testing::_));
  SetScreenLockState(false /* is_locked */);
}

// Test through the core flow of unlocking the screen with Smart Lock.
// Unfortunately, the only observable side effect we have available is verifying
// that the success metric is emitted.
// The "SmartLock.AuthResult" Failure bucket is incorrectly emitted to during
// this test. See crbug.com/1255964 for more info.
TEST_F(SmartLockServiceTest, AuthenticateWithSmartLock) {
  InitializeService(true /* should_initialize_all_dependencies */);
  SetScreenLockState(true /* is_locked */);

  // smart_lock_service_->AttemptAuth() will fail if the SmartLockState is not
  // kPhoneAuthenticated.
  smart_lock_service_->UpdateSmartLockState(
      SmartLockState::kPhoneAuthenticated);

  EXPECT_TRUE(smart_lock_service_->AttemptAuth(account_id_));
  smart_lock_service_->FinalizeUnlock(true);

  histogram_tester_.ExpectBucketCount("SmartLock.AuthResult.Unlock", 1, 0);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.FeatureUsage.SmartLock",
      feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess, 0);

  SetScreenLockState(false /* is_locked */);

  histogram_tester_.ExpectBucketCount("SmartLock.AuthResult.Unlock", 1, 1);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.FeatureUsage.SmartLock",
      feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess, 1);
}

// Regression test for crbug.com/974410.
// The "SmartLock.AuthResult" Failure bucket is incorrectly emitted to during
// this test. See crbug.com/1255964 for more info.
TEST_F(SmartLockServiceTest, AuthenticateWithSmartLockMultipleTimes) {
  InitializeService(true /* should_initialize_all_dependencies */);
  SetScreenLockState(true /* is_locked */);

  // smart_lock_service_->AttemptAuth() will fail if the SmartLockState is not
  // kPhoneAuthenticated.
  smart_lock_service_->UpdateSmartLockState(
      SmartLockState::kPhoneAuthenticated);

  EXPECT_TRUE(smart_lock_service_->AttemptAuth(account_id_));
  smart_lock_service_->FinalizeUnlock(true);

  // The first auth attempt is still ongoing. A second auth attempt request
  // should be rejected.
  EXPECT_FALSE(smart_lock_service_->AttemptAuth(account_id_));

  histogram_tester_.ExpectBucketCount("SmartLock.AuthResult.Unlock", 1, 0);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.FeatureUsage.SmartLock",
      feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess, 0);

  SetScreenLockState(false /* is_locked */);

  histogram_tester_.ExpectBucketCount("SmartLock.AuthResult.Unlock", 1, 1);
  histogram_tester_.ExpectBucketCount(
      "ChromeOS.FeatureUsage.SmartLock",
      feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess, 1);
}

// Regression test for b/213941298.
// Authenticate with Smart Lock twice, calling UpdateSmartLockState in between.
// Ordinarily, this call to UpdateSmartLockState would be made by
// ProximityAuthClient during the OnScreenDidUnlock event. Check that the second
// call to AttemptAuth succeeds since UpdateSmartLockState clears out the last
// auth attempt, allowing the next auth attempt to proceed.
TEST_F(SmartLockServiceTest, UpdateSmartLockStateClearsLastAuthAttempt) {
  InitializeService(true /* should_initialize_all_dependencies */);
  SetScreenLockState(true /* is_locked */);

  smart_lock_service_->UpdateSmartLockState(
      SmartLockState::kPhoneAuthenticated);

  EXPECT_TRUE(smart_lock_service_->AttemptAuth(account_id_));
  smart_lock_service_->FinalizeUnlock(true);

  ASSERT_TRUE(fake_lock_handler_->smart_lock_auth_result().has_value());
  EXPECT_TRUE(fake_lock_handler_->smart_lock_auth_result().value());

  SetScreenLockState(false /* is_locked */);
  smart_lock_service_->UpdateSmartLockState(SmartLockState::kInactive);

  EXPECT_FALSE(fake_lock_handler_->smart_lock_auth_result().has_value());
  EXPECT_FALSE(fake_lock_handler_->smart_lock_state().has_value());

  SetScreenLockState(true /* is_locked */);
  smart_lock_service_->UpdateSmartLockState(
      SmartLockState::kPhoneAuthenticated);
  EXPECT_TRUE(smart_lock_service_->AttemptAuth(account_id_));
  smart_lock_service_->FinalizeUnlock(true);

  ASSERT_TRUE(fake_lock_handler_->smart_lock_auth_result().has_value());
  EXPECT_TRUE(fake_lock_handler_->smart_lock_auth_result().value());

  SetScreenLockState(false /* is_locked */);
}

TEST_F(SmartLockServiceTest, GetInitialSmartLockState_FeatureEnabled) {
  InitializeService(true /* should_initialize_all_dependencies */);
  SetScreenLockState(true /* is_locked */);

  EXPECT_EQ(SmartLockState::kConnectingToPhone,
            smart_lock_service_->GetInitialSmartLockState());
}

TEST_F(SmartLockServiceTest, GetInitialSmartLockState_FeatureDisabled) {
  InitializeService(true /* should_initialize_all_dependencies */);
  SetIsEnabled(false);
  SetScreenLockState(true /* is_locked */);

  EXPECT_EQ(SmartLockState::kDisabled,
            smart_lock_service_->GetInitialSmartLockState());
}

TEST_F(SmartLockServiceTest, ShowInitialSmartLockState_FeatureEnabled) {
  InitializeService(true /* should_initialize_all_dependencies */);

  EXPECT_FALSE(fake_lock_handler_->smart_lock_state());

  SetScreenLockState(true /* is_locked */);

  // Before SmartLockService::UpdateSmartLockState() is called externally,
  // ensure that internal state is updated to an "initial" state to prevent
  // UI jank.
  EXPECT_EQ(SmartLockState::kConnectingToPhone,
            fake_lock_handler_->smart_lock_state().value());

  smart_lock_service_->UpdateSmartLockState(SmartLockState::kConnectingToPhone);
  EXPECT_EQ(SmartLockState::kConnectingToPhone,
            fake_lock_handler_->smart_lock_state().value());
}

TEST_F(SmartLockServiceTest, ShowInitialSmartLockState_FeatureDisabled) {
  InitializeService(true /* should_initialize_all_dependencies */);
  SetIsEnabled(false);

  EXPECT_FALSE(fake_lock_handler_->smart_lock_state());

  SetScreenLockState(true /* is_locked */);

  // Before SmartLockService::UpdateSmartLockState() is called externally,
  // ensure that internal state is updated to an "initial" state to prevent
  // UI jank.
  EXPECT_EQ(SmartLockState::kDisabled,
            fake_lock_handler_->smart_lock_state().value());
}

TEST_F(SmartLockServiceTest, PrepareForSuspend) {
  InitializeService(/*should_initialize_all_dependencies=*/true);
  SetScreenLockState(/*is_locked=*/true);
  smart_lock_service_->UpdateSmartLockState(
      SmartLockState::kPhoneAuthenticated);
  EXPECT_EQ(SmartLockState::kPhoneAuthenticated,
            fake_lock_handler_->smart_lock_state().value());
  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent::LID_CLOSED);
  EXPECT_EQ(SmartLockState::kConnectingToPhone,
            fake_lock_handler_->smart_lock_state().value());
}

TEST_F(SmartLockServiceTest, HandleAuthFailureInUpdateSmartLockState) {
  InitializeService(/*should_initialize_all_dependencies=*/true);
  SetScreenLockState(/*is_locked=*/true);
  smart_lock_service_->UpdateSmartLockState(
      SmartLockState::kPhoneAuthenticated);
  EXPECT_EQ(proximity_auth::mojom::AuthType::USER_CLICK,
            fake_lock_handler_->GetAuthType(account_id_));
  smart_lock_service_->AttemptAuth(account_id_);
  smart_lock_service_->FinalizeUnlock(true);
  EXPECT_TRUE(fake_lock_handler_->smart_lock_auth_result().has_value());
  EXPECT_TRUE(fake_lock_handler_->smart_lock_auth_result().value());
  smart_lock_service_->UpdateSmartLockState(
      SmartLockState::kPhoneNotAuthenticated);
  EXPECT_EQ(proximity_auth::mojom::AuthType::OFFLINE_PASSWORD,
            fake_lock_handler_->GetAuthType(account_id_));
  EXPECT_TRUE(fake_lock_handler_->smart_lock_auth_result().has_value());
  EXPECT_FALSE(fake_lock_handler_->smart_lock_auth_result().value());
}

TEST_F(SmartLockServiceTest, IsSmartLockStateValidOnRemoteAuthFailure) {
  InitializeService(/*should_initialize_all_dependencies=*/true);
  SetScreenLockState(/*is_locked=*/true);
  for (const SmartLockStateTestCase& testcase : kSmartLockStateTestCases) {
    fake_lock_handler_->ClearSmartLockState();
    fake_lock_handler_->ClearSmartLockAuthResult();
    smart_lock_service_->UpdateSmartLockState(
        SmartLockState::kPhoneAuthenticated);
    smart_lock_service_->AttemptAuth(account_id_);
    smart_lock_service_->UpdateSmartLockState(testcase.smart_lock_state);
    if (testcase.smart_lock_state != SmartLockState::kPhoneAuthenticated) {
      EXPECT_EQ(proximity_auth::mojom::AuthType::OFFLINE_PASSWORD,
                fake_lock_handler_->GetAuthType(account_id_));
      if (testcase.should_be_valid_on_remote_auth_failure) {
        EXPECT_FALSE(fake_lock_handler_->smart_lock_auth_result().has_value());
      } else {
        EXPECT_TRUE(fake_lock_handler_->smart_lock_auth_result().has_value());
        EXPECT_FALSE(fake_lock_handler_->smart_lock_auth_result().value());
      }
    }
  }
}

TEST_F(SmartLockServiceTest, FinalizeUnlock) {
  InitializeService(/*should_initialize_all_dependencies=*/true);
  SetScreenLockState(/*is_locked=*/true);
  smart_lock_service_->FinalizeUnlock(true);
  EXPECT_EQ(0, fake_lock_handler_->unlock_called());
  smart_lock_service_->UpdateSmartLockState(
      SmartLockState::kPhoneAuthenticated);
  smart_lock_service_->AttemptAuth(account_id_);
  smart_lock_service_->FinalizeUnlock(true);
  EXPECT_EQ(1, fake_lock_handler_->unlock_called());
  smart_lock_service_->FinalizeUnlock(false);
  EXPECT_TRUE(fake_lock_handler_->smart_lock_auth_result().has_value());
  EXPECT_FALSE(fake_lock_handler_->smart_lock_auth_result().value());
}

TEST_F(SmartLockServiceTest, GetPasswordAuthEvent) {
  InitializeService(/*should_initialize_all_dependencies=*/true);
  SetScreenLockState(/*is_locked=*/true);
  ResetSmartLockState();
  EXPECT_EQ(SmartLockAuthEvent::PASSWORD_ENTRY_NO_SMARTLOCK_STATE_HANDLER,
            GetPasswordAuthEvent());
  for (const SmartLockStateTestCase& testcase : kSmartLockStateTestCases) {
    fake_lock_handler_->ClearSmartLockState();
    fake_lock_handler_->ClearSmartLockAuthResult();
    smart_lock_service_->UpdateSmartLockState(testcase.smart_lock_state);
    EXPECT_EQ(testcase.smart_lock_auth_event, GetPasswordAuthEvent());
  }
}

TEST_F(SmartLockServiceTest, GetSmartUnlockPasswordAuthEvent) {
  InitializeService(/*should_initialize_all_dependencies=*/true);
  SetScreenLockState(/*is_locked=*/true);
  ResetSmartLockState();
  EXPECT_EQ(
      SmartLockMetricsRecorder::SmartLockAuthEventPasswordState::kUnknownState,
      GetSmartUnlockPasswordAuthEvent());
  for (const SmartLockStateTestCase& testcase : kSmartLockStateTestCases) {
    fake_lock_handler_->ClearSmartLockState();
    fake_lock_handler_->ClearSmartLockAuthResult();
    smart_lock_service_->UpdateSmartLockState(testcase.smart_lock_state);
    EXPECT_EQ(testcase.smart_lock_auth_event_password_state,
              GetSmartUnlockPasswordAuthEvent());
  }
}

}  // namespace ash
