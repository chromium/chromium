// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/privacy_screen_controller.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/dbus/privacy_screen_service_provider.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/services/service_provider_test_helper.h"
#include "components/prefs/pref_service.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/display/manager/display_change_observer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/test/action_logger_util.h"
#include "ui/display/manager/test/fake_display_snapshot.h"
#include "ui/display/manager/test/test_native_display_delegate.h"
#include "ui/display/types/display_constants.h"

namespace ash {

namespace {

constexpr char kUser1Email[] = "user1@privacyscreen";
constexpr char kUser2Email[] = "user2@privacyscreen";
constexpr gfx::Size kDisplaySize{1024, 768};

class MockObserver : public PrivacyScreenController::Observer {
 public:
  MockObserver() {}
  ~MockObserver() override = default;

  MOCK_METHOD(void,
              OnPrivacyScreenSettingChanged,
              (bool enabled, bool notify_ui),
              (override));
};

class PrivacyScreenControllerTest : public NoSessionAshTestBase {
 public:
  PrivacyScreenControllerTest()
      : logger_(std::make_unique<display::test::ActionLogger>()) {}
  ~PrivacyScreenControllerTest() override = default;

  PrivacyScreenControllerTest(const PrivacyScreenControllerTest&) = delete;
  PrivacyScreenControllerTest& operator=(const PrivacyScreenControllerTest&) =
      delete;

  PrivacyScreenController* controller() {
    return Shell::Get()->privacy_screen_controller();
  }

  PrefService* user1_pref_service() const {
    return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
        AccountId::FromUserEmail(kUser1Email));
  }

  void SetUp() override {
    NoSessionAshTestBase::SetUp();

    // Create user 1 session and simulate its login.
    SimulateUserLogin(kUser1Email);

    // Create user 2 session.
    GetSessionControllerClient()->AddUserSession(kUser2Email);

    native_display_delegate_ =
        new display::test::TestNativeDisplayDelegate(logger_.get());
    display_manager()->configurator()->SetDelegateForTesting(
        std::unique_ptr<display::NativeDisplayDelegate>(
            native_display_delegate_));
    display_change_observer_ =
        std::make_unique<display::DisplayChangeObserver>(display_manager());
    test_api_ = std::make_unique<display::DisplayConfigurator::TestApi>(
        display_manager()->configurator());

    controller()->AddObserver(observer());
  }

  struct TestSnapshotParams {
    int64_t id;
    bool is_internal_display;
    bool supports_privacy_screen;
  };

  void TearDown() override {
    // DisplayChangeObserver access DeviceDataManager in its destructor, so
    // destroy it first.
    display_change_observer_ = nullptr;
    controller()->RemoveObserver(observer());
    AshTestBase::TearDown();
  }

  void SwitchActiveUser(const std::string& email) {
    GetSessionControllerClient()->SwitchActiveUser(
        AccountId::FromUserEmail(email));
  }

  // Builds display snapshots into |native_display_delegate_| and update the
  // display configurator and display manager with it.
  void BuildAndUpdateDisplaySnapshots(
      const std::vector<TestSnapshotParams>& snapshot_params) {
    std::vector<std::unique_ptr<display::DisplaySnapshot>> outputs;

    for (const auto& param : snapshot_params) {
      outputs.push_back(
          display::FakeDisplaySnapshot::Builder()
              .SetId(param.id)
              .SetNativeMode(kDisplaySize)
              .SetCurrentMode(kDisplaySize)
              .SetType(param.is_internal_display
                           ? display::DISPLAY_CONNECTION_TYPE_INTERNAL
                           : display::DISPLAY_CONNECTION_TYPE_HDMI)
              .SetPrivacyScreen(param.supports_privacy_screen
                                    ? display::kDisabled
                                    : display::kNotSupported)
              .Build());
    }

    native_display_delegate_->SetOutputs(std::move(outputs));
    display_manager()->configurator()->OnConfigurationChanged();
    display_manager()->configurator()->ForceInitialConfigure();
    EXPECT_TRUE(test_api_->TriggerConfigureTimeout());
    display_change_observer_->OnDisplayConfigurationChanged(
        native_display_delegate_->GetOutputs());
  }

  MockObserver* observer() { return &observer_; }

 private:
  std::unique_ptr<display::test::ActionLogger> logger_;
  raw_ptr<display::test::TestNativeDisplayDelegate,
          DanglingUntriaged>
      native_display_delegate_;  // Not owned.
  std::unique_ptr<display::DisplayChangeObserver> display_change_observer_;
  std::unique_ptr<display::DisplayConfigurator::TestApi> test_api_;
  ::testing::NiceMock<MockObserver> observer_;
};

class PrivacyScreenServiceProviderTest : public PrivacyScreenControllerTest {
 public:
  PrivacyScreenServiceProviderTest() = default;
  ~PrivacyScreenServiceProviderTest() override = default;
  PrivacyScreenServiceProviderTest(const PrivacyScreenServiceProviderTest&) =
      delete;
  PrivacyScreenServiceProviderTest& operator=(
      const PrivacyScreenServiceProviderTest&) = delete;

  // PrivacyScreenControllerTest:
  void SetUp() override {
    PrivacyScreenControllerTest::SetUp();
    service_provider_ = std::make_unique<PrivacyScreenServiceProvider>();
    test_helper_.SetUp(
        privacy_screen::kPrivacyScreenServiceName,
        dbus::ObjectPath(privacy_screen::kPrivacyScreenServicePath),
        privacy_screen::kPrivacyScreenServiceInterface,
        privacy_screen::kPrivacyScreenServiceGetPrivacyScreenSettingMethod,
        service_provider_.get());
  }

  void TearDown() override {
    test_helper_.TearDown();
    service_provider_.reset();
    PrivacyScreenControllerTest::TearDown();
  }

  privacy_screen::PrivacyScreenSetting_PrivacyScreenState
  GetPrivacyScreenSettingStateFromDBus() {
    dbus::MethodCall method_call(
        privacy_screen::kPrivacyScreenServiceInterface,
        privacy_screen::kPrivacyScreenServiceGetPrivacyScreenSettingMethod);
    std::unique_ptr<dbus::Response> response =
        test_helper_.CallMethod(&method_call);

    dbus::MessageReader reader(response.get());
    privacy_screen::PrivacyScreenSetting setting;
    EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&setting));
    EXPECT_FALSE(reader.HasMoreData());
    return setting.state();
  }

  void ConnectToPrivacyScreenSettingChangedDBusSignal() {
    test_helper_.SetUpReturnSignal(
        privacy_screen::kPrivacyScreenServiceInterface,
        privacy_screen::kPrivacyScreenServicePrivacyScreenSettingChangedSignal,
        base::BindRepeating(&PrivacyScreenServiceProviderTest::
                                OnPrivacyScreenSettingChangedDBusSignal,
                            base::Unretained(this)),
        base::DoNothing());
  }

  void OnPrivacyScreenSettingChangedDBusSignal(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    privacy_screen::PrivacyScreenSetting setting;
    EXPECT_TRUE(reader.PopArrayOfBytesAsProto(&setting));
    last_signal_state_ = setting.state();
    EXPECT_FALSE(reader.HasMoreData());
  }

 protected:
  privacy_screen::PrivacyScreenSetting_PrivacyScreenState last_signal_state_ =
      privacy_screen::PrivacyScreenSetting_PrivacyScreenState_NOT_SUPPORTED;
  std::unique_ptr<PrivacyScreenServiceProvider> service_provider_;
  ServiceProviderTestHelper test_helper_;
};

// Test that user prefs do not get mixed up between user changes on a device
// with a single supporting display.
TEST_F(PrivacyScreenControllerTest, TestEnableAndDisable) {
  // Create a single internal display that supports privacy screen.
  BuildAndUpdateDisplaySnapshots({{
      /*id=*/123u,
      /*is_internal_display=*/true,
      /*supports_privacy_screen=*/true,
  }});
  EXPECT_EQ(1u, display_manager()->GetNumDisplays());
  ASSERT_TRUE(controller()->IsSupported());

  // Enable for user 1, and switch to user 2. User 2 should have it disabled.
  EXPECT_CALL(*observer(), OnPrivacyScreenSettingChanged(true, true));
  controller()->SetEnabled(true);
  EXPECT_TRUE(controller()->GetEnabled());

  // Switching accounts should trigger observers but should not notify ui.
  ::testing::Mock::VerifyAndClear(observer());
  EXPECT_CALL(*observer(), OnPrivacyScreenSettingChanged(false, false));
  SwitchActiveUser(kUser2Email);
  EXPECT_FALSE(controller()->GetEnabled());

  // Switch back to user 1, expect it to be enabled.
  ::testing::Mock::VerifyAndClear(observer());
  EXPECT_CALL(*observer(), OnPrivacyScreenSettingChanged(true, false));
  SwitchActiveUser(kUser1Email);
  EXPECT_TRUE(controller()->GetEnabled());
}

// Checks that when the privacy screen is enforced by Data Leak Prevention
// feature, it's turned on regardless of the user pref state.
TEST_F(PrivacyScreenControllerTest, TestDlpEnforced) {
  // Create a single internal display that supports privacy screen.
  BuildAndUpdateDisplaySnapshots({{
      /*id=*/123u,
      /*is_internal_display=*/true,
      /*supports_privacy_screen=*/true,
  }});
  EXPECT_EQ(1u, display_manager()->GetNumDisplays());
  ASSERT_TRUE(controller()->IsSupported());
  EXPECT_FALSE(controller()->GetEnabled());

  // Enforce privacy screen and check notification.
  EXPECT_CALL(*observer(), OnPrivacyScreenSettingChanged(true, true));
  controller()->SetEnforced(true);
  EXPECT_TRUE(controller()->GetEnabled());

  // Additionally enable it via pref, no change.
  ::testing::Mock::VerifyAndClear(observer());
  EXPECT_CALL(*observer(), OnPrivacyScreenSettingChanged(true, true));
  controller()->SetEnabled(true);
  EXPECT_TRUE(controller()->GetEnabled());

  // Shouldn't be turned off when pref is disabled, because already enforced.
  ::testing::Mock::VerifyAndClear(observer());
  EXPECT_CALL(*observer(), OnPrivacyScreenSettingChanged(true, true));
  controller()->SetEnabled(false);
  EXPECT_TRUE(controller()->GetEnabled());

  // Privacy screen enforced again by DLP, no notification should be shown.
  ::testing::Mock::VerifyAndClear(observer());
  EXPECT_CALL(*observer(),
              OnPrivacyScreenSettingChanged(::testing::_, ::testing::_))
      .Times(0);
  controller()->SetEnforced(true);
  EXPECT_TRUE(controller()->GetEnabled());

  // Remove enforcement, turned off as pref was not changed.
  ::testing::Mock::VerifyAndClear(observer());
  EXPECT_CALL(*observer(), OnPrivacyScreenSettingChanged(false, true));
  controller()->SetEnforced(false);
  EXPECT_FALSE(controller()->GetEnabled());

  // Add pref back.
  ::testing::Mock::VerifyAndClear(observer());
  EXPECT_CALL(*observer(), OnPrivacyScreenSettingChanged(true, true));
  controller()->SetEnabled(true);
  EXPECT_TRUE(controller()->GetEnabled());

  // Privacy screen enforced again by DLP, no notification should be shown as
  // privacy screen already turned on by the user.
  ::testing::Mock::VerifyAndClear(observer());
  EXPECT_CALL(*observer(),
              OnPrivacyScreenSettingChanged(::testing::_, ::testing::_))
      .Times(0);
  controller()->SetEnforced(true);
  EXPECT_TRUE(controller()->GetEnabled());

  // Remove enforcement, privacy screen should still be on due to pref and no
  // notification.
  ::testing::Mock::VerifyAndClear(observer());
  EXPECT_CALL(*observer(),
              OnPrivacyScreenSettingChanged(::testing::_, ::testing::_))
      .Times(0);
  controller()->SetEnforced(false);
  EXPECT_TRUE(controller()->GetEnabled());

  // Disable via pref, privacy screen is turned off with a notification.
  ::testing::Mock::VerifyAndClear(observer());
  EXPECT_CALL(*observer(), OnPrivacyScreenSettingChanged(false, true));
  controller()->SetEnabled(false);
  EXPECT_FALSE(controller()->GetEnabled());
}

// Tests that updates of the Privacy Screen user prefs from outside the
// PrivacyScreenController (such as Settings UI) are observed and applied.
TEST_F(PrivacyScreenControllerTest, TestOutsidePrefsUpdates) {
  BuildAndUpdateDisplaySnapshots({{
      /*id=*/123u,
      /*is_internal_display=*/true,
      /*supports_privacy_screen=*/true,
  }});
  EXPECT_EQ(1u, display_manager()->GetNumDisplays());
  ASSERT_TRUE(controller()->IsSupported());

  EXPECT_CALL(*observer(), OnPrivacyScreenSettingChanged(true, true));
  EXPECT_FALSE(controller()->GetEnabled());
  user1_pref_service()->SetBoolean(prefs::kDisplayPrivacyScreenEnabled, true);
  EXPECT_TRUE(controller()->GetEnabled());

  ::testing::Mock::VerifyAndClear(observer());
  EXPECT_CALL(*observer(), OnPrivacyScreenSettingChanged(false, true));
  user1_pref_service()->SetBoolean(prefs::kDisplayPrivacyScreenEnabled, false);
  EXPECT_FALSE(controller()->GetEnabled());
}

TEST_F(PrivacyScreenControllerTest, SupportedOnSingleInternalDisplay) {
  BuildAndUpdateDisplaySnapshots({{
      /*id=*/123u,
      /*is_internal_display=*/true,
      /*supports_privacy_screen=*/true,
  }});
  EXPECT_EQ(1u, display_manager()->GetNumDisplays());
  ASSERT_TRUE(controller()->IsSupported());

  EXPECT_CALL(*observer(), OnPrivacyScreenSettingChanged(true, true));
  controller()->SetEnabled(true);
  EXPECT_TRUE(controller()->GetEnabled());
}

TEST_F(PrivacyScreenControllerTest, NotSupportedOnSingleInternalDisplay) {
  BuildAndUpdateDisplaySnapshots({{
      /*id=*/123u,
      /*is_internal_display=*/true,
      /*supports_privacy_screen=*/false,
  }});
  EXPECT_EQ(1u, display_manager()->GetNumDisplays());
  ASSERT_FALSE(controller()->IsSupported());

  EXPECT_FALSE(controller()->GetEnabled());
}

// Test that the privacy screen is not supported when the device is connected
// to an external display and the lid is closed (a.k.a. docked mode).
TEST_F(PrivacyScreenControllerTest, NotSupportedOnInternalDisplayWhenDocked) {
  BuildAndUpdateDisplaySnapshots({{
                                      /*id=*/123u,
                                      /*is_internal_display=*/true,
                                      /*supports_privacy_screen=*/true,
                                  },
                                  {
                                      /*id=*/234u,
                                      /*is_internal_display=*/false,
                                      /*supports_privacy_screen=*/false,
                                  }});
  EXPECT_EQ(2u, display_manager()->GetNumDisplays());

  // Turn off the internal display
  display_manager()->configurator()->SetDisplayPower(
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
      display::DisplayConfigurator::kSetDisplayPowerNoFlags, base::DoNothing());

  ASSERT_FALSE(controller()->IsSupported());
  EXPECT_FALSE(controller()->GetEnabled());
}

TEST_F(PrivacyScreenControllerTest,
       SupportedOnInternalDisplayWithMultipleExternalDisplays) {
  BuildAndUpdateDisplaySnapshots({{
                                      /*id=*/1234u,
                                      /*is_internal_display=*/true,
                                      /*supports_privacy_screen=*/true,
                                  },
                                  {
                                      /*id=*/2341u,
                                      /*is_internal_display=*/false,
                                      /*supports_privacy_screen=*/false,
                                  },
                                  {
                                      /*id=*/3412u,
                                      /*is_internal_display=*/false,
                                      /*supports_privacy_screen=*/false,
                                  }});
  EXPECT_EQ(3u, display_manager()->GetNumDisplays());
  ASSERT_TRUE(controller()->IsSupported());

  controller()->SetEnabled(true);
  EXPECT_TRUE(controller()->GetEnabled());
}

TEST_F(PrivacyScreenControllerTest,
       NotSupportedOnInternalDisplayWithMultipleExternalDisplays) {
  BuildAndUpdateDisplaySnapshots({{
                                      /*id=*/1234u,
                                      /*is_internal_display=*/true,
                                      /*supports_privacy_screen=*/false,
                                  },
                                  {
                                      /*id=*/2341u,
                                      /*is_internal_display=*/false,
                                      /*supports_privacy_screen=*/false,
                                  },
                                  {
                                      /*id=*/3412u,
                                      /*is_internal_display=*/false,
                                      /*supports_privacy_screen=*/false,
                                  }});
  EXPECT_EQ(3u, display_manager()->GetNumDisplays());
  ASSERT_FALSE(controller()->IsSupported());

  EXPECT_FALSE(controller()->GetEnabled());
}

TEST_F(PrivacyScreenServiceProviderTest, PrivacyScreenNotSupported) {
  BuildAndUpdateDisplaySnapshots({{
      /*id=*/123u,
      /*is_internal_display=*/true,
      /*supports_privacy_screen=*/false,
  }});

  ASSERT_EQ(
      GetPrivacyScreenSettingStateFromDBus(),
      privacy_screen::PrivacyScreenSetting_PrivacyScreenState_NOT_SUPPORTED);
}

TEST_F(PrivacyScreenServiceProviderTest, PrivacyScreenDisabled) {
  BuildAndUpdateDisplaySnapshots({{
      /*id=*/123u,
      /*is_internal_display=*/true,
      /*supports_privacy_screen=*/true,
  }});

  ASSERT_EQ(GetPrivacyScreenSettingStateFromDBus(),
            privacy_screen::PrivacyScreenSetting_PrivacyScreenState_DISABLED);
}

TEST_F(PrivacyScreenServiceProviderTest, PrivacyScreenEnabled) {
  ConnectToPrivacyScreenSettingChangedDBusSignal();

  BuildAndUpdateDisplaySnapshots({{
      /*id=*/123u,
      /*is_internal_display=*/true,
      /*supports_privacy_screen=*/true,
  }});

  controller()->SetEnabled(true);

  // Expects PrivacyScreenSettingChanged D-Bus signal to be called once.
  ASSERT_EQ(last_signal_state_,
            privacy_screen::PrivacyScreenSetting_PrivacyScreenState_ENABLED);

  ASSERT_EQ(GetPrivacyScreenSettingStateFromDBus(),
            privacy_screen::PrivacyScreenSetting_PrivacyScreenState_ENABLED);
}

}  // namespace

}  // namespace ash
