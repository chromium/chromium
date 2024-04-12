// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_notification_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/public/mojom/input_device_settings.mojom-shared.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/toast/anchored_nudge.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/contains.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/ash/mojom/simulate_right_click_modifier.mojom-shared.h"
#include "ui/events/ash/mojom/six_pack_shortcut_modifier.mojom-shared.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/message_center/fake_message_center.h"

namespace ash {

namespace {

constexpr char kTopRowKeyNoMatchNudgeId[] = "top-row-key-no-match-nudge-id";
constexpr char kSixPackKeyNoMatchNudgeId[] = "six-patch-key-no-match-nudge-id";
constexpr char kCapsLockNoMatchNudgeId[] = "caps-lock-no-match-nudge-id";

const mojom::Mouse kMouse1 = mojom::Mouse(
    /*name=*/"Razer Basilisk V3",
    /*is_external=*/false,
    /*id=*/1,
    /*device_key=*/"fake-device-key1",
    /*customization_restriction=*/
    mojom::CustomizationRestriction::kAllowCustomizations,
    /*mouse_button_config=*/mojom::MouseButtonConfig::kNoConfig,
    mojom::MouseSettings::New());

const mojom::GraphicsTablet kGraphicsTablet2 = mojom::GraphicsTablet(
    /*name=*/"Wacom Intuos S",
    /*id=*/2,
    /*device_key=*/"fake-device-key2",
    /*customization_restriction=*/
    ::ash::mojom::CustomizationRestriction::kAllowCustomizations,
    /*graphics_tablet_button_config=*/
    mojom::GraphicsTabletButtonConfig::kNoConfig,
    mojom::GraphicsTabletSettings::New());

int GetPrefNotificationCount(const char* pref_name) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  return prefs->GetInteger(pref_name);
}

class TestMessageCenter : public message_center::FakeMessageCenter {
 public:
  TestMessageCenter() = default;

  TestMessageCenter(const TestMessageCenter&) = delete;
  TestMessageCenter& operator=(const TestMessageCenter&) = delete;

  ~TestMessageCenter() override = default;

  void ClickOnNotification(const std::string& id) override {
    message_center::Notification* notification =
        FindVisibleNotificationById(id);
    CHECK(notification);
    notification->delegate()->Click(std::nullopt, std::nullopt);
  }

  void ClickOnNotificationButton(const std::string& id,
                                 int button_index) override {
    message_center::Notification* notification =
        FindVisibleNotificationById(id);
    CHECK(notification);
    notification->delegate()->Click(button_index, std::nullopt);
  }
};

class MockNewWindowDelegate : public testing::NiceMock<TestNewWindowDelegate> {
 public:
  // TestNewWindowDelegate:
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, OpenUrlFrom from, Disposition disposition),
              (override));
};

void CancelNudge(const std::string& id) {
  Shell::Get()->anchored_nudge_manager()->Cancel(id);
}

}  // namespace

class InputDeviceSettingsNotificationControllerTest : public AshTestBase {
 public:
  InputDeviceSettingsNotificationControllerTest() = default;

  InputDeviceSettingsNotificationControllerTest(
      const InputDeviceSettingsNotificationControllerTest&) = delete;
  InputDeviceSettingsNotificationControllerTest& operator=(
      const InputDeviceSettingsNotificationControllerTest&) = delete;

  ~InputDeviceSettingsNotificationControllerTest() override = default;

  message_center::FakeMessageCenter* message_center() {
    return message_center_.get();
  }
  InputDeviceSettingsNotificationController* controller() {
    return controller_.get();
  }

  // AshTestBase:
  void SetUp() override {
    auto delegate = std::make_unique<MockNewWindowDelegate>();
    new_window_delegate_ = delegate.get();
    delegate_provider_ =
        std::make_unique<TestNewWindowDelegateProvider>(std::move(delegate));
    AshTestBase::SetUp();
    message_center_ = std::make_unique<TestMessageCenter>();
    controller_ = std::make_unique<InputDeviceSettingsNotificationController>(
        message_center_.get());
  }

  void TearDown() override {
    controller_.reset();
    message_center_.reset();
    AshTestBase::TearDown();
  }

 protected:
  MockNewWindowDelegate& new_window_delegate() { return *new_window_delegate_; }
  raw_ptr<MockNewWindowDelegate, DanglingUntriaged> new_window_delegate_;
  std::unique_ptr<TestMessageCenter> message_center_;
  std::unique_ptr<InputDeviceSettingsNotificationController> controller_;
  std::unique_ptr<TestNewWindowDelegateProvider> delegate_provider_;
};

TEST_F(InputDeviceSettingsNotificationControllerTest,
       NotifyRightClickRewriteBlockedBySetting) {
  size_t expected_notification_count = 1;
  controller()->NotifyRightClickRewriteBlockedBySetting(
      ui::mojom::SimulateRightClickModifier::kAlt,
      ui::mojom::SimulateRightClickModifier::kSearch);
  EXPECT_EQ(expected_notification_count++,
            message_center()->NotificationCount());
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(
      "alt_right_click_rewrite_blocked_by_setting"));

  controller()->NotifyRightClickRewriteBlockedBySetting(
      ui::mojom::SimulateRightClickModifier::kSearch,
      ui::mojom::SimulateRightClickModifier::kAlt);
  EXPECT_EQ(expected_notification_count++,
            message_center()->NotificationCount());
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(
      "search_right_click_rewrite_blocked_by_setting"));

  controller()->NotifyRightClickRewriteBlockedBySetting(
      ui::mojom::SimulateRightClickModifier::kAlt,
      ui::mojom::SimulateRightClickModifier::kNone);
  EXPECT_EQ(expected_notification_count++,
            message_center()->NotificationCount());
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(
      "right_click_rewrite_disabled_by_setting"));
}

TEST_F(InputDeviceSettingsNotificationControllerTest,
       RemapToRightClickNotificationOnlyShownForActiveUserSessions) {
  GetSessionControllerClient()->LockScreen();

  controller()->NotifyRightClickRewriteBlockedBySetting(
      ui::mojom::SimulateRightClickModifier::kAlt,
      ui::mojom::SimulateRightClickModifier::kSearch);
  EXPECT_EQ(message_center()->NotificationCount(), 0u);
}

TEST_F(InputDeviceSettingsNotificationControllerTest,
       SixPackKeyNotificationShownAtMostThreeTimes) {
  EXPECT_EQ(3, GetPrefNotificationCount(
                   prefs::kSixPackKeyDeleteNotificationsRemaining));

  controller()->NotifySixPackRewriteBlockedBySetting(
      ui::VKEY_DELETE, ui::mojom::SixPackShortcutModifier::kAlt,
      ui::mojom::SixPackShortcutModifier::kSearch,
      /*device_id=*/1);
  EXPECT_EQ(1u, message_center()->NotificationCount());
  EXPECT_EQ(2, GetPrefNotificationCount(
                   prefs::kSixPackKeyDeleteNotificationsRemaining));

  controller()->NotifySixPackRewriteBlockedBySetting(
      ui::VKEY_DELETE, ui::mojom::SixPackShortcutModifier::kAlt,
      ui::mojom::SixPackShortcutModifier::kSearch,
      /*device_id=*/1);
  EXPECT_EQ(1, GetPrefNotificationCount(
                   prefs::kSixPackKeyDeleteNotificationsRemaining));

  controller()->NotifySixPackRewriteBlockedBySetting(
      ui::VKEY_DELETE, ui::mojom::SixPackShortcutModifier::kAlt,
      ui::mojom::SixPackShortcutModifier::kSearch,
      /*device_id=*/1);
  EXPECT_EQ(0, GetPrefNotificationCount(
                   prefs::kSixPackKeyDeleteNotificationsRemaining));

  message_center()->RemoveAllNotifications(
      false, message_center::MessageCenter::RemoveType::ALL);
  controller()->NotifySixPackRewriteBlockedBySetting(
      ui::VKEY_DELETE, ui::mojom::SixPackShortcutModifier::kAlt,
      ui::mojom::SixPackShortcutModifier::kSearch,
      /*device_id=*/1);
  EXPECT_EQ(0u, message_center()->NotificationCount());

  // Only the delete notification pref should have changed.
  EXPECT_EQ(
      3, GetPrefNotificationCount(prefs::kSixPackKeyEndNotificationsRemaining));
  EXPECT_EQ(3, GetPrefNotificationCount(
                   prefs::kSixPackKeyHomeNotificationsRemaining));
  EXPECT_EQ(3, GetPrefNotificationCount(
                   prefs::kSixPackKeyInsertNotificationsRemaining));
  EXPECT_EQ(3, GetPrefNotificationCount(
                   prefs::kSixPackKeyPageUpNotificationsRemaining));
  EXPECT_EQ(3, GetPrefNotificationCount(
                   prefs::kSixPackKeyPageDownNotificationsRemaining));
}

TEST_F(InputDeviceSettingsNotificationControllerTest,
       RightClickNotificationShownAtMostThreeTimes) {
  EXPECT_EQ(3, GetPrefNotificationCount(
                   prefs::kRemapToRightClickNotificationsRemaining));

  controller()->NotifyRightClickRewriteBlockedBySetting(
      ui::mojom::SimulateRightClickModifier::kAlt,
      ui::mojom::SimulateRightClickModifier::kSearch);
  EXPECT_EQ(1u, message_center()->NotificationCount());
  EXPECT_EQ(2, GetPrefNotificationCount(
                   prefs::kRemapToRightClickNotificationsRemaining));

  controller()->NotifyRightClickRewriteBlockedBySetting(
      ui::mojom::SimulateRightClickModifier::kSearch,
      ui::mojom::SimulateRightClickModifier::kAlt);
  EXPECT_EQ(2u, message_center()->NotificationCount());
  EXPECT_EQ(1, GetPrefNotificationCount(
                   prefs::kRemapToRightClickNotificationsRemaining));

  controller()->NotifyRightClickRewriteBlockedBySetting(
      ui::mojom::SimulateRightClickModifier::kAlt,
      ui::mojom::SimulateRightClickModifier::kNone);
  EXPECT_EQ(3u, message_center()->NotificationCount());
  EXPECT_EQ(0, GetPrefNotificationCount(
                   prefs::kRemapToRightClickNotificationsRemaining));

  controller()->NotifyRightClickRewriteBlockedBySetting(
      ui::mojom::SimulateRightClickModifier::kAlt,
      ui::mojom::SimulateRightClickModifier::kSearch);
  EXPECT_EQ(3u, message_center()->NotificationCount());
  EXPECT_EQ(0, GetPrefNotificationCount(
                   prefs::kRemapToRightClickNotificationsRemaining));
}

TEST_F(InputDeviceSettingsNotificationControllerTest,
       StopShowingNotificationIfUserClicksOnIt) {
  EXPECT_EQ(3, GetPrefNotificationCount(
                   prefs::kRemapToRightClickNotificationsRemaining));

  controller()->NotifyRightClickRewriteBlockedBySetting(
      ui::mojom::SimulateRightClickModifier::kAlt,
      ui::mojom::SimulateRightClickModifier::kSearch);
  message_center()->ClickOnNotification(
      "alt_right_click_rewrite_blocked_by_setting");
  EXPECT_EQ(0, GetPrefNotificationCount(
                   prefs::kRemapToRightClickNotificationsRemaining));
}

TEST_F(InputDeviceSettingsNotificationControllerTest,
       ShowPeripheralSettingsOnCustomizationNotificationClick) {
  controller()->NotifyMouseIsCustomizable(kMouse1);
  message_center()->ClickOnNotification("peripheral_customization_mouse_1");
  EXPECT_EQ(GetSystemTrayClient()->show_mouse_settings_count(), 1);

  controller()->NotifyGraphicsTabletIsCustomizable(kGraphicsTablet2);
  message_center()->ClickOnNotification(
      "peripheral_customization_graphics_tablet_2");
  EXPECT_EQ(GetSystemTrayClient()->show_graphics_tablet_settings_count(), 1);
}

TEST_F(InputDeviceSettingsNotificationControllerTest,
       ShowPeripheralSettingsOnCustomizationNotificationButtonClick) {
  controller()->NotifyMouseIsCustomizable(kMouse1);
  message_center()->ClickOnNotificationButton(
      "peripheral_customization_mouse_1", /*button_index=*/0);
  EXPECT_EQ(GetSystemTrayClient()->show_mouse_settings_count(), 1);

  controller()->NotifyGraphicsTabletIsCustomizable(kGraphicsTablet2);
  message_center()->ClickOnNotificationButton(
      "peripheral_customization_graphics_tablet_2", /*button_index=*/0);
  EXPECT_EQ(GetSystemTrayClient()->show_graphics_tablet_settings_count(), 1);
}

// TODO(b/279503977): Add test that verifies behavior of clicking on the
// "Learn more" button.
TEST_F(InputDeviceSettingsNotificationControllerTest,
       ShowTouchpadSettingsOnRightClickNotificationClick) {
  controller()->NotifyRightClickRewriteBlockedBySetting(
      ui::mojom::SimulateRightClickModifier::kAlt,
      ui::mojom::SimulateRightClickModifier::kSearch);
  message_center()->ClickOnNotificationButton(
      "alt_right_click_rewrite_blocked_by_setting",
      NotificationButtonIndex::BUTTON_EDIT_SHORTCUT);
  EXPECT_EQ(GetSystemTrayClient()->show_touchpad_settings_count(), 1);
}

TEST_F(InputDeviceSettingsNotificationControllerTest,
       ShowRemapKeysSettingsOnSixPackNotificationClick) {
  controller()->NotifySixPackRewriteBlockedBySetting(
      ui::VKEY_DELETE, ui::mojom::SixPackShortcutModifier::kAlt,
      ui::mojom::SixPackShortcutModifier::kSearch,
      /*device_id=*/1);
  message_center()->ClickOnNotificationButton(
      "delete_six_pack_rewrite_blocked_by_setting_1",
      NotificationButtonIndex::BUTTON_EDIT_SHORTCUT);
  EXPECT_EQ(GetSystemTrayClient()->show_remap_keys_subpage_count(), 1);
}

TEST_F(InputDeviceSettingsNotificationControllerTest,
       NotifySixPackRewriteBlockedBySetting) {
  size_t expected_notification_count = 1;
  controller()->NotifySixPackRewriteBlockedBySetting(
      ui::VKEY_DELETE, ui::mojom::SixPackShortcutModifier::kAlt,
      ui::mojom::SixPackShortcutModifier::kSearch,
      /*device_id=*/1);
  EXPECT_EQ(expected_notification_count++,
            message_center()->NotificationCount());
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(
      "delete_six_pack_rewrite_blocked_by_setting_1"));

  controller()->NotifySixPackRewriteBlockedBySetting(
      ui::VKEY_INSERT, ui::mojom::SixPackShortcutModifier::kSearch,
      ui::mojom::SixPackShortcutModifier::kNone,
      /*device_id=*/1);
  EXPECT_EQ(expected_notification_count++,
            message_center()->NotificationCount());
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(
      "insert_six_pack_rewrite_blocked_by_setting_1"));

  controller()->NotifySixPackRewriteBlockedBySetting(
      ui::VKEY_HOME, ui::mojom::SixPackShortcutModifier::kAlt,
      ui::mojom::SixPackShortcutModifier::kSearch,
      /*device_id=*/1);
  EXPECT_EQ(expected_notification_count++,
            message_center()->NotificationCount());
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(
      "home_six_pack_rewrite_blocked_by_setting_1"));

  controller()->NotifySixPackRewriteBlockedBySetting(
      ui::VKEY_END, ui::mojom::SixPackShortcutModifier::kAlt,
      ui::mojom::SixPackShortcutModifier::kSearch,
      /*device_id=*/1);
  EXPECT_EQ(expected_notification_count++,
            message_center()->NotificationCount());
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(
      "end_six_pack_rewrite_blocked_by_setting_1"));

  controller()->NotifySixPackRewriteBlockedBySetting(
      ui::VKEY_PRIOR, ui::mojom::SixPackShortcutModifier::kAlt,
      ui::mojom::SixPackShortcutModifier::kSearch,
      /*device_id=*/1);
  EXPECT_EQ(expected_notification_count++,
            message_center()->NotificationCount());
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(
      "page_up_six_pack_rewrite_blocked_by_setting_1"));

  controller()->NotifySixPackRewriteBlockedBySetting(
      ui::VKEY_NEXT, ui::mojom::SixPackShortcutModifier::kAlt,
      ui::mojom::SixPackShortcutModifier::kSearch,
      /*device_id=*/1);
  EXPECT_EQ(expected_notification_count++,
            message_center()->NotificationCount());
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(
      "page_down_six_pack_rewrite_blocked_by_setting_1"));
}

TEST_F(InputDeviceSettingsNotificationControllerTest,
       NotifyPeripheralCustomization) {
  size_t expected_notification_count = 1;
  controller()->NotifyMouseIsCustomizable(kMouse1);
  EXPECT_EQ(expected_notification_count++,
            message_center()->NotificationCount());
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(
      "peripheral_customization_mouse_1"));

  controller()->NotifyGraphicsTabletIsCustomizable(kGraphicsTablet2);
  EXPECT_EQ(expected_notification_count++,
            message_center()->NotificationCount());
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(
      "peripheral_customization_graphics_tablet_2"));
}

TEST_F(InputDeviceSettingsNotificationControllerTest,
       SixPackRewriteNotificationOnlyShownForActiveUserSessions) {
  GetSessionControllerClient()->LockScreen();
  controller()->NotifySixPackRewriteBlockedBySetting(
      ui::VKEY_PRIOR, ui::mojom::SixPackShortcutModifier::kAlt,
      ui::mojom::SixPackShortcutModifier::kSearch,
      /*device_id=*/1);
  EXPECT_EQ(message_center()->NotificationCount(), 0u);
}

TEST_F(InputDeviceSettingsNotificationControllerTest, LearnMoreButtonClicked) {
  controller()->NotifySixPackRewriteBlockedBySetting(
      ui::VKEY_DELETE, ui::mojom::SixPackShortcutModifier::kAlt,
      ui::mojom::SixPackShortcutModifier::kSearch,
      /*device_id=*/1);
  EXPECT_CALL(
      new_window_delegate(),
      OpenUrl(GURL("https://support.google.com/chromebook?p=keyboard_settings"),
              NewWindowDelegate::OpenUrlFrom::kUserInteraction,
              NewWindowDelegate::Disposition::kNewForegroundTab));
  message_center()->ClickOnNotificationButton(
      "delete_six_pack_rewrite_blocked_by_setting_1",
      NotificationButtonIndex::BUTTON_LEARN_MORE);
}

TEST_F(InputDeviceSettingsNotificationControllerTest,
       NotifyMouseFirstTimeConnected) {
  size_t expected_notification_count = 1;
  mojom::MousePtr mojom_mouse = mojom::Mouse::New();
  mojom_mouse->device_key = "0001:0001";
  mojom_mouse->id = 1;
  mojom_mouse->settings = mojom::MouseSettings::New();

  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();

  EXPECT_TRUE(prefs->GetList(prefs::kPeripheralNotificationMiceSeen).empty());
  controller()->NotifyMouseFirstTimeConnected(*mojom_mouse);
  EXPECT_EQ(prefs->GetList(prefs::kPeripheralNotificationMiceSeen).size(), 1u);
  EXPECT_TRUE(
      base::Contains(prefs->GetList(prefs::kPeripheralNotificationMiceSeen),
                     base::Value("0001:0001")));
  controller()->NotifyMouseFirstTimeConnected(*mojom_mouse);
  EXPECT_EQ(prefs->GetList(prefs::kPeripheralNotificationMiceSeen).size(), 1u);
  EXPECT_EQ(expected_notification_count++,
            message_center()->NotificationCount());
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(
      "peripheral_customization_mouse_1"));

  mojom_mouse->id = 2;
  mojom_mouse->device_key = "0001:0002";

  controller()->NotifyMouseFirstTimeConnected(*mojom_mouse);
  EXPECT_EQ(prefs->GetList(prefs::kPeripheralNotificationMiceSeen).size(), 2u);
  EXPECT_TRUE(
      base::Contains(prefs->GetList(prefs::kPeripheralNotificationMiceSeen),
                     base::Value("0001:0002")));
  EXPECT_EQ(expected_notification_count, message_center()->NotificationCount());
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(
      "peripheral_customization_mouse_2"));

  mojom_mouse->id = 3;
  mojom_mouse->device_key = "0001:0003";
  mojom_mouse->settings->button_remappings.push_back(
      mojom::ButtonRemapping::New(
          /*name=*/"Button 1",
          mojom::Button::NewCustomizableButton(
              mojom::CustomizableButton::kBack),
          /*remapping_action=*/nullptr));
  controller()->NotifyMouseFirstTimeConnected(*mojom_mouse);
  EXPECT_EQ(prefs->GetList(prefs::kPeripheralNotificationMiceSeen).size(), 3u);
  EXPECT_TRUE(
      base::Contains(prefs->GetList(prefs::kPeripheralNotificationMiceSeen),
                     base::Value("0001:0003")));
  EXPECT_EQ(expected_notification_count, message_center()->NotificationCount());
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(
      "peripheral_customization_mouse_3"));
}

TEST_F(InputDeviceSettingsNotificationControllerTest,
       NotifyGraphicsTabletFirstTimeConnected) {
  size_t expected_notification_count = 1;
  mojom::GraphicsTabletPtr mojom_graphics_tablet = mojom::GraphicsTablet::New();
  mojom_graphics_tablet->id = 1;
  mojom_graphics_tablet->device_key = "0002:0001";
  mojom_graphics_tablet->settings = mojom::GraphicsTabletSettings::New();
  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();

  EXPECT_TRUE(prefs->GetList(prefs::kPeripheralNotificationGraphicsTabletsSeen)
                  .empty());
  controller()->NotifyGraphicsTabletFirstTimeConnected(*mojom_graphics_tablet);
  EXPECT_EQ(
      prefs->GetList(prefs::kPeripheralNotificationGraphicsTabletsSeen).size(),
      1u);
  EXPECT_EQ(expected_notification_count++,
            message_center()->NotificationCount());
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(
      "peripheral_customization_graphics_tablet_1"));

  EXPECT_TRUE(base::Contains(
      prefs->GetList(prefs::kPeripheralNotificationGraphicsTabletsSeen),
      base::Value("0002:0001")));
  controller()->NotifyGraphicsTabletFirstTimeConnected(*mojom_graphics_tablet);
  EXPECT_EQ(
      prefs->GetList(prefs::kPeripheralNotificationGraphicsTabletsSeen).size(),
      1u);

  mojom_graphics_tablet->id = 2;
  mojom_graphics_tablet->device_key = "0002:0002";

  controller()->NotifyGraphicsTabletFirstTimeConnected(*mojom_graphics_tablet);
  EXPECT_EQ(
      prefs->GetList(prefs::kPeripheralNotificationGraphicsTabletsSeen).size(),
      2u);
  EXPECT_TRUE(base::Contains(
      prefs->GetList(prefs::kPeripheralNotificationGraphicsTabletsSeen),
      base::Value("0002:0002")));
  EXPECT_EQ(expected_notification_count, message_center()->NotificationCount());
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(
      "peripheral_customization_graphics_tablet_2"));

  mojom_graphics_tablet->id = 3;
  mojom_graphics_tablet->device_key = "0002:0003";
  mojom_graphics_tablet->settings->pen_button_remappings.push_back(
      mojom::ButtonRemapping::New(
          /*name=*/"Button 1",
          mojom::Button::NewCustomizableButton(
              mojom::CustomizableButton::kBack),
          /*remapping_action=*/nullptr));

  controller()->NotifyGraphicsTabletFirstTimeConnected(*mojom_graphics_tablet);
  EXPECT_EQ(
      prefs->GetList(prefs::kPeripheralNotificationGraphicsTabletsSeen).size(),
      3u);
  EXPECT_TRUE(base::Contains(
      prefs->GetList(prefs::kPeripheralNotificationGraphicsTabletsSeen),
      base::Value("0002:0003")));
  EXPECT_EQ(expected_notification_count, message_center()->NotificationCount());
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(
      "peripheral_customization_graphics_tablet_3"));

  mojom_graphics_tablet->id = 4;
  mojom_graphics_tablet->device_key = "0002:0004";
  mojom_graphics_tablet->settings->pen_button_remappings.clear();
  mojom_graphics_tablet->settings->tablet_button_remappings.push_back(
      mojom::ButtonRemapping::New(
          /*name=*/"Button 1",
          mojom::Button::NewCustomizableButton(
              mojom::CustomizableButton::kBack),
          /*remapping_action=*/nullptr));

  controller()->NotifyGraphicsTabletFirstTimeConnected(*mojom_graphics_tablet);
  EXPECT_EQ(
      prefs->GetList(prefs::kPeripheralNotificationGraphicsTabletsSeen).size(),
      4u);
  EXPECT_TRUE(base::Contains(
      prefs->GetList(prefs::kPeripheralNotificationGraphicsTabletsSeen),
      base::Value("0002:0004")));
  EXPECT_EQ(expected_notification_count, message_center()->NotificationCount());
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(
      "peripheral_customization_graphics_tablet_4"));
}

TEST_F(InputDeviceSettingsNotificationControllerTest,
       ShowTopRowRewritingNudge) {
  const AnchoredNudge* nudge =
      Shell::Get()->anchored_nudge_manager()->GetNudgeIfShown(
          kTopRowKeyNoMatchNudgeId);
  ASSERT_FALSE(nudge);

  controller()->ShowTopRowRewritingNudge();
  const AnchoredNudge* nudge_shown =
      Shell::Get()->anchored_nudge_manager()->GetNudgeIfShown(
          kTopRowKeyNoMatchNudgeId);
  ASSERT_TRUE(nudge_shown);
  EXPECT_TRUE(nudge_shown->GetVisible());
}

TEST_F(InputDeviceSettingsNotificationControllerTest,
       ShowSixPackKeyRewritingNudge) {
  const AnchoredNudge* nudge =
      Shell::Get()->anchored_nudge_manager()->GetNudgeIfShown(
          kSixPackKeyNoMatchNudgeId);
  ASSERT_FALSE(nudge);
  base::Value::Dict overrides;
  overrides.Set(prefs::kSixPackKeyInsert, /*kSearch*/ 2);
  overrides.Set(prefs::kSixPackKeyDelete, /*kAlt*/ 1);
  overrides.Set(prefs::kSixPackKeyHome, /*kSearch*/ 2);
  overrides.Set(prefs::kSixPackKeyPageUp, /*kAlt*/ 1);
  overrides.Set(prefs::kSixPackKeyEnd, /*kSearch*/ 2);
  base::Value::Dict remappings;
  remappings.Set(prefs::kKeyboardSettingSixPackKeyRemappings,
                 std::move(overrides));
  Shell::Get()->session_controller()->GetActivePrefService()->SetDict(
      prefs::kKeyboardDefaultChromeOSSettings, std::move(remappings));
  AnchoredNudgeManagerImpl* nudge_manager =
      Shell::Get()->anchored_nudge_manager();
  EXPECT_TRUE(nudge_manager);

  // Display nudge for VKEY_INSERT.
  controller()->ShowSixPackKeyRewritingNudge(
      ui::VKEY_INSERT, ui::mojom::SixPackShortcutModifier::kSearch);
  const AnchoredNudge* nudge_shown =
      nudge_manager->GetNudgeIfShown(kSixPackKeyNoMatchNudgeId);
  ASSERT_TRUE(nudge_shown);
  EXPECT_TRUE(nudge_shown->GetVisible());
  EXPECT_EQ(
      nudge_manager->GetNudgeBodyTextForTest(kSixPackKeyNoMatchNudgeId),
      l10n_util::GetStringUTF16(
          IDS_ASH_SETTINGS_KEYBOARD_USE_FN_KEY_FOR_SEARCH_PLUS_SHIFT_BACKSPACE_NUDGE_DESCRIPTION));
  CancelNudge(kSixPackKeyNoMatchNudgeId);
  EXPECT_FALSE(nudge_shown->GetVisible());

  // Display nudge for VKEY_DELETE.
  controller()->ShowSixPackKeyRewritingNudge(
      ui::VKEY_DELETE, ui::mojom::SixPackShortcutModifier::kSearch);
  // Modifier not match, nudge should not show.
  EXPECT_FALSE(nudge_shown->GetVisible());
  controller()->ShowSixPackKeyRewritingNudge(
      ui::VKEY_DELETE, ui::mojom::SixPackShortcutModifier::kAlt);
  EXPECT_TRUE(nudge_shown);
  EXPECT_TRUE(nudge_shown->GetVisible());
  EXPECT_EQ(
      nudge_manager->GetNudgeBodyTextForTest(kSixPackKeyNoMatchNudgeId),
      l10n_util::GetStringUTF16(
          IDS_ASH_SETTINGS_KEYBOARD_USE_FN_KEY_FOR_ALT_PLUS_BACKSPACE_NUDGE_DESCRIPTION));
  CancelNudge(kSixPackKeyNoMatchNudgeId);
  EXPECT_FALSE(nudge_shown->GetVisible());

  // Display nudge for VKEY_HOME.
  EXPECT_FALSE(nudge_shown->GetVisible());
  controller()->ShowSixPackKeyRewritingNudge(
      ui::VKEY_HOME, ui::mojom::SixPackShortcutModifier::kSearch);
  EXPECT_TRUE(nudge_shown);
  EXPECT_TRUE(nudge_shown->GetVisible());
  EXPECT_EQ(
      nudge_manager->GetNudgeBodyTextForTest(kSixPackKeyNoMatchNudgeId),
      l10n_util::GetStringUTF16(
          IDS_ASH_SETTINGS_KEYBOARD_USE_FN_KEY_FOR_SEARCH_PLUS_LEFT_NUDGE_DESCRIPTION));
  CancelNudge(kSixPackKeyNoMatchNudgeId);
  EXPECT_FALSE(nudge_shown->GetVisible());

  // Display nudge for VKEY_END.
  EXPECT_FALSE(nudge_shown->GetVisible());
  controller()->ShowSixPackKeyRewritingNudge(
      ui::VKEY_END, ui::mojom::SixPackShortcutModifier::kSearch);
  EXPECT_TRUE(nudge_shown);
  EXPECT_TRUE(nudge_shown->GetVisible());
  EXPECT_EQ(
      nudge_manager->GetNudgeBodyTextForTest(kSixPackKeyNoMatchNudgeId),
      l10n_util::GetStringUTF16(
          IDS_ASH_SETTINGS_KEYBOARD_USE_FN_KEY_FOR_SEARCH_PLUS_RIGHT_NUDGE_DESCRIPTION));
  CancelNudge(kSixPackKeyNoMatchNudgeId);
  EXPECT_FALSE(nudge_shown->GetVisible());

  // Display nudge for VKEY_PRIOR.
  EXPECT_FALSE(nudge_shown->GetVisible());
  controller()->ShowSixPackKeyRewritingNudge(
      ui::VKEY_PRIOR, ui::mojom::SixPackShortcutModifier::kAlt);
  EXPECT_TRUE(nudge_shown);
  EXPECT_TRUE(nudge_shown->GetVisible());
  EXPECT_EQ(
      nudge_manager->GetNudgeBodyTextForTest(kSixPackKeyNoMatchNudgeId),
      l10n_util::GetStringUTF16(
          IDS_ASH_SETTINGS_KEYBOARD_USE_FN_KEY_FOR_ALT_PLUS_UP_NUDGE_DESCRIPTION));
  CancelNudge(kSixPackKeyNoMatchNudgeId);
  EXPECT_FALSE(nudge_shown->GetVisible());

  // Six pack key VKEY_NEXT is not in the prefDict, should not show anything.
  EXPECT_FALSE(nudge_shown->GetVisible());
  controller()->ShowSixPackKeyRewritingNudge(
      ui::VKEY_NEXT, ui::mojom::SixPackShortcutModifier::kSearch);
  EXPECT_TRUE(nudge_shown);
  EXPECT_FALSE(nudge_shown->GetVisible());

  // Call the method with a non six pack key, should not show anything.
  // Six pack key VKEY_NEXT is not in the prefDict, should not show anything.
  EXPECT_FALSE(nudge_shown->GetVisible());
  controller()->ShowSixPackKeyRewritingNudge(
      ui::VKEY_BRIGHTNESS_UP, ui::mojom::SixPackShortcutModifier::kSearch);
  EXPECT_TRUE(nudge_shown);
  EXPECT_FALSE(nudge_shown->GetVisible());
}

TEST_F(InputDeviceSettingsNotificationControllerTest,
       ShowCapsLockRewritingNudge) {
  const AnchoredNudge* nudge =
      Shell::Get()->anchored_nudge_manager()->GetNudgeIfShown(
          kCapsLockNoMatchNudgeId);
  ASSERT_FALSE(nudge);

  controller()->ShowCapsLockRewritingNudge();
  const AnchoredNudge* nudge_shown =
      Shell::Get()->anchored_nudge_manager()->GetNudgeIfShown(
          kCapsLockNoMatchNudgeId);
  ASSERT_TRUE(nudge_shown);
  EXPECT_TRUE(nudge_shown->GetVisible());
}

}  // namespace ash
