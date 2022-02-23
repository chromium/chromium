// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/screen_layout_observer.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/display/display.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/display_manager_utilities.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/views/controls/label.h"

namespace ash {

class ScreenLayoutObserverTest : public AshTestBase {
 public:
  ScreenLayoutObserverTest();

  ScreenLayoutObserverTest(const ScreenLayoutObserverTest&) = delete;
  ScreenLayoutObserverTest& operator=(const ScreenLayoutObserverTest&) = delete;

  ~ScreenLayoutObserverTest() override;

  // AshTestBase:
  void SetUp() override;

 protected:
  ScreenLayoutObserver* GetScreenLayoutObserver();
  void CheckUpdate();

  void CloseNotification();
  void ClickNotification();
  std::u16string GetDisplayNotificationText() const;
  std::u16string GetDisplayNotificationAdditionalText() const;

  std::u16string GetFirstDisplayName();

  std::u16string GetSecondDisplayName();

  std::u16string GetMirroringDisplayNames();

  std::u16string GetUnifiedDisplayName();

  bool IsNotificationShown() const;

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  const message_center::Notification* GetDisplayNotification() const;
};

ScreenLayoutObserverTest::ScreenLayoutObserverTest() {
  scoped_feature_list_.InitAndDisableFeature(
      features::kReduceDisplayNotifications);
}

ScreenLayoutObserverTest::~ScreenLayoutObserverTest() = default;

void ScreenLayoutObserverTest::SetUp() {
  AshTestBase::SetUp();
  GetScreenLayoutObserver()->set_show_notifications_for_testing(true);
}

ScreenLayoutObserver* ScreenLayoutObserverTest::GetScreenLayoutObserver() {
  return Shell::Get()->screen_layout_observer();
}

void ScreenLayoutObserverTest::CloseNotification() {
  message_center::MessageCenter::Get()->RemoveNotification(
      ScreenLayoutObserver::kNotificationId, false);
  base::RunLoop().RunUntilIdle();
}

void ScreenLayoutObserverTest::ClickNotification() {
  const message_center::Notification* notification = GetDisplayNotification();
  notification->delegate()->Click(absl::nullopt, absl::nullopt);
}

std::u16string ScreenLayoutObserverTest::GetDisplayNotificationText() const {
  const message_center::Notification* notification = GetDisplayNotification();
  return notification ? notification->title() : std::u16string();
}

std::u16string ScreenLayoutObserverTest::GetDisplayNotificationAdditionalText()
    const {
  const message_center::Notification* notification = GetDisplayNotification();
  return notification ? notification->message() : std::u16string();
}

std::u16string ScreenLayoutObserverTest::GetFirstDisplayName() {
  return base::UTF8ToUTF16(display_manager()->GetDisplayNameForId(
      display_manager()->first_display_id()));
}

std::u16string ScreenLayoutObserverTest::GetSecondDisplayName() {
  return base::UTF8ToUTF16(display_manager()->GetDisplayNameForId(
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id()));
}

std::u16string ScreenLayoutObserverTest::GetMirroringDisplayNames() {
  DCHECK(display_manager()->IsInMirrorMode());
  std::u16string display_names;
  for (auto& id : display_manager()->GetMirroringDestinationDisplayIdList()) {
    if (!display_names.empty())
      display_names.append(u",");
    display_names.append(
        base::UTF8ToUTF16(display_manager()->GetDisplayNameForId(id)));
  }
  return display_names;
}

std::u16string ScreenLayoutObserverTest::GetUnifiedDisplayName() {
  return base::UTF8ToUTF16(
      display_manager()->GetDisplayNameForId(display::kUnifiedDisplayId));
}

bool ScreenLayoutObserverTest::IsNotificationShown() const {
  return !(GetDisplayNotificationText().empty() &&
           GetDisplayNotificationAdditionalText().empty());
}

const message_center::Notification*
ScreenLayoutObserverTest::GetDisplayNotification() const {
  const message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  for (const auto* notification : notifications) {
    if (notification->id() == ScreenLayoutObserver::kNotificationId)
      return notification;
  }

  return nullptr;
}

// This test is flaky. crbug.com/1222612
TEST_F(ScreenLayoutObserverTest, DISABLED_DisplayNotifications) {
  UpdateDisplay("500x400");
  display::Display::SetInternalDisplayId(display_manager()->first_display_id());
  EXPECT_TRUE(GetDisplayNotificationText().empty());

  // No-update
  CloseNotification();
  UpdateDisplay("500x400");
  EXPECT_FALSE(IsNotificationShown());

  // Extended.
  CloseNotification();
  UpdateDisplay("500x400,300x200");
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_EXTENDED,
                                       GetSecondDisplayName()),
            GetDisplayNotificationText());
  EXPECT_TRUE(GetDisplayNotificationAdditionalText().empty());

  const int64_t first_display_id =
      display::Screen::GetScreen()->GetPrimaryDisplay().id();
  const int64_t second_display_id =
      display::GetNextSynthesizedDisplayId(first_display_id);
  display::ManagedDisplayInfo first_display_info =
      display::CreateDisplayInfo(first_display_id, gfx::Rect(1, 1, 500, 500));
  display::ManagedDisplayInfo second_display_info =
      display::CreateDisplayInfo(second_display_id, gfx::Rect(2, 2, 500, 500));
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(first_display_info);
  display_info_list.push_back(second_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .set_maximum_display(2u);
  UpdateDisplay("500x400,300x200,200x100");
  EXPECT_TRUE(GetDisplayNotificationText().empty());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_DISPLAY_REMOVED_EXCEEDED_MAXIMUM),
            GetDisplayNotificationAdditionalText());
  EXPECT_TRUE(GetDisplayNotificationText().empty());
  UpdateDisplay("500x400,300x200");
  CloseNotification();

  // Start tablet mode and wait until display mode is updated.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  base::RunLoop().RunUntilIdle();

  // Exit mirror mode manually. Now display mode should be extending mode.
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, absl::nullopt);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_MIRROR_EXIT),
            GetDisplayNotificationText());
  CloseNotification();

  // Simulate that device can support at most two displays and user connects
  // it with three displays. Because device is in tablet mode, display mode
  // becomes mirror mode from extending mode. Under this circumstance, user is
  // still notified of connecting more displays than maximum. See issue 827406
  // (https://crbug.com/827406).
  UpdateDisplay("500x400,300x200,200x100");
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_DISPLAY_REMOVED_EXCEEDED_MAXIMUM),
            GetDisplayNotificationAdditionalText());
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_MIRRORING,
                                       GetMirroringDisplayNames()),
            GetDisplayNotificationText());

  // Reset the parameter. Close tablet mode and wait until display mode is
  // updated.
  display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .ResetMaximumDisplay();
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  base::RunLoop().RunUntilIdle();

  // Turn on mirror mode.
  CloseNotification();
  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, absl::nullopt);
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_MIRRORING,
                                       GetMirroringDisplayNames()),
            GetDisplayNotificationText());
  EXPECT_TRUE(GetDisplayNotificationAdditionalText().empty());

  // Disconnect a display to end mirror mode.
  CloseNotification();
  display_info_list.erase(display_info_list.end() - 1);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_MIRROR_EXIT),
            GetDisplayNotificationText());
  EXPECT_TRUE(GetDisplayNotificationAdditionalText().empty());

  // Restore mirror mode.
  CloseNotification();
  display_info_list.push_back(second_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_MIRRORING,
                                       GetMirroringDisplayNames()),
            GetDisplayNotificationText());
  EXPECT_TRUE(GetDisplayNotificationAdditionalText().empty());

  // Turn off mirror mode.
  CloseNotification();
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, absl::nullopt);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_MIRROR_EXIT),
            GetDisplayNotificationText());
  EXPECT_TRUE(GetDisplayNotificationAdditionalText().empty());

  // Enters closed lid mode.
  UpdateDisplay("500x400@1.5,300x200");
  display::Display::SetInternalDisplayId(
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id());
  UpdateDisplay("500x400@1.5");
  EXPECT_TRUE(GetDisplayNotificationText().empty());
}

TEST_F(ScreenLayoutObserverTest, DisplayNotificationsDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndEnableFeature(
      features::kReduceDisplayNotifications);

  UpdateDisplay("500x400");
  display::Display::SetInternalDisplayId(display_manager()->first_display_id());
  EXPECT_TRUE(GetDisplayNotificationText().empty());

  // Adding a display.
  UpdateDisplay("500x400,300x200");
  EXPECT_FALSE(IsNotificationShown());

  const int64_t first_display_id =
      display::Screen::GetScreen()->GetPrimaryDisplay().id();
  const int64_t second_display_id =
      display::GetNextSynthesizedDisplayId(first_display_id);
  display::ManagedDisplayInfo first_display_info =
      display::CreateDisplayInfo(first_display_id, gfx::Rect(1, 1, 500, 400));
  display::ManagedDisplayInfo second_display_info =
      display::CreateDisplayInfo(second_display_id, gfx::Rect(2, 2, 500, 400));
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(first_display_info);
  display_info_list.push_back(second_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  // Simulate that device can support at most two displays and user
  // connects it with three displays. Notification should still be created to
  // warn user of it. See issue 827406 (https://crbug.com/827406).
  display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .set_maximum_display(2u);
  UpdateDisplay("500x400,300x200,200x100");
  EXPECT_TRUE(GetDisplayNotificationText().empty());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_DISPLAY_REMOVED_EXCEEDED_MAXIMUM),
            GetDisplayNotificationAdditionalText());
  EXPECT_TRUE(GetDisplayNotificationText().empty());
  UpdateDisplay("500x400,300x200");
  CloseNotification();

  // Start tablet mode and wait until display mode is updated.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  base::RunLoop().RunUntilIdle();

  // Exit mirror mode manually. Now display mode should be extending mode.
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, absl::nullopt);
  EXPECT_FALSE(IsNotificationShown());

  // Simulate that device can support at most two displays and user connects
  // it with three displays. Because device is in tablet mode, display mode
  // becomes mirror mode from extending mode. Under this circumstance, user is
  // still notified of connecting more displays than maximum. See issue 827406
  // (https://crbug.com/827406). Notification should still be shown.
  UpdateDisplay("500x400,300x200,200x100");
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_DISPLAY_REMOVED_EXCEEDED_MAXIMUM),
            GetDisplayNotificationAdditionalText());
  // The tablet should no longer be in mirror mode.
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_TRUE(GetDisplayNotificationText().empty());
  CloseNotification();
}

// Zooming in Unified Mode results in display size changes rather than changes
// in the UI scales, in which case, we still want to show a notification when
// the source of change is not the settings ui.
TEST_F(ScreenLayoutObserverTest, ZoomingInUnifiedModeNotification) {
  UpdateDisplay("500x400,300x200");

  // Enter unified mode.
  display_manager()->SetUnifiedDesktopEnabled(true);

  // Using keyboard shortcuts to change the zoom should result in a
  // notification.
  CloseNotification();
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  EXPECT_TRUE(display_manager()->ZoomDisplay(display_id, false /* up */));
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_RESOLUTION_CHANGED,
                                 GetUnifiedDisplayName(), u"550x200"),
      GetDisplayNotificationAdditionalText());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_DISPLAY_RESOLUTION_CHANGED_TITLE),
            GetDisplayNotificationText());

  CloseNotification();
  EXPECT_TRUE(display_manager()->ZoomDisplay(display_id, true /* up */));
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_RESOLUTION_CHANGED,
                                 GetUnifiedDisplayName(), u"1100x400"),
      GetDisplayNotificationAdditionalText());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_DISPLAY_RESOLUTION_CHANGED_TITLE),
            GetDisplayNotificationText());

  // However, when the source is the settings UI, the ScreenLayoutObserver does
  // not produce a notification for resolution changes in Unified Mode. These
  // are handled by the ResolutionNotificationController instead.
  CloseNotification();
  Shell::Get()->screen_layout_observer()->SetDisplayChangedFromSettingsUI(
      display::kUnifiedDisplayId);
  EXPECT_TRUE(display_manager()->ZoomDisplay(display_id, false /* up */));
  EXPECT_TRUE(GetDisplayNotificationAdditionalText().empty());
  EXPECT_TRUE(GetDisplayNotificationText().empty());
}

// Verify that notification shows up when display is switched from dock mode to
// extend mode.
TEST_F(ScreenLayoutObserverTest, DisplayConfigurationChangedTwice) {
  UpdateDisplay("500x400,300x200");
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_DISPLAY_EXTENDED_NO_INTERNAL),
            GetDisplayNotificationText());

  // OnDisplayConfigurationChanged() may be called more than once for a single
  // update display in case of primary is swapped or recovered from dock mode.
  // Should not remove the notification in such case.
  GetScreenLayoutObserver()->OnDisplayConfigurationChanged();
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_DISPLAY_EXTENDED_NO_INTERNAL),
            GetDisplayNotificationText());

  // Back to the single display. It should show that a display was removed.
  UpdateDisplay("500x400");
  EXPECT_TRUE(base::StartsWith(
      GetDisplayNotificationText(),
      l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_REMOVED, u""),
      base::CompareCase::SENSITIVE));
}

// Verify that no notification is shown when overscan of a screen is changed.
TEST_F(ScreenLayoutObserverTest, OverscanDisplay) {
  UpdateDisplay("500x400, 400x300");
  // Close the notification that is shown from initially adding a monitor.
  CloseNotification();
  display::Display::SetInternalDisplayId(display_manager()->first_display_id());

  // /o creates the default overscan.
  UpdateDisplay("500x400, 400x300/o");
  EXPECT_FALSE(IsNotificationShown());

  // Reset the overscan.
  Shell::Get()->display_manager()->SetOverscanInsets(
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id(),
      gfx::Insets());
  EXPECT_FALSE(IsNotificationShown());
}

// Tests that exiting mirror mode by closing the lid shows the correct "exiting
// mirror mode" message.
TEST_F(ScreenLayoutObserverTest, ExitMirrorModeBecauseOfDockedModeMessage) {
  UpdateDisplay("500x400,300x200");
  display::Display::SetInternalDisplayId(
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id());

  // Mirroring.
  UpdateDisplay("500x400,300x200");
  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, absl::nullopt);
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_MIRRORING,
                                       GetMirroringDisplayNames()),
            GetDisplayNotificationText());

  // Docked.
  CloseNotification();
  display::Display::SetInternalDisplayId(display_manager()->first_display_id());
  UpdateDisplay("300x200");
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_MIRROR_EXIT),
            GetDisplayNotificationText());
}

// Special case: tests that exiting mirror mode by removing a display shows the
// correct message.
TEST_F(ScreenLayoutObserverTest,
       ExitMirrorModeNoInternalDisplayBecauseOfDisplayRemovedMessage) {
  UpdateDisplay("500x400,300x200");
  display::Display::SetInternalDisplayId(
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id());

  // Mirroring.
  UpdateDisplay("500x400,300x200");
  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, absl::nullopt);
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_MIRRORING,
                                       GetMirroringDisplayNames()),
            GetDisplayNotificationText());

  // Removing one of the displays. We show that we exited mirror mode.
  CloseNotification();
  UpdateDisplay("500x400");
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_MIRROR_EXIT),
            GetDisplayNotificationText());
}

// Tests notification messages shown when adding and removing displays in
// extended mode.
TEST_F(ScreenLayoutObserverTest, AddingRemovingDisplayExtendedModeMessage) {
  UpdateDisplay("500x400");
  EXPECT_TRUE(GetDisplayNotificationText().empty());

  // Adding a display in extended mode.
  UpdateDisplay("500x400,300x200");
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_DISPLAY_EXTENDED_NO_INTERNAL),
            GetDisplayNotificationText());

  // Removing a display.
  CloseNotification();
  UpdateDisplay("500x400");
  EXPECT_TRUE(base::StartsWith(
      GetDisplayNotificationText(),
      l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_REMOVED, u""),
      base::CompareCase::SENSITIVE));
}

// Tests notification messages shown when entering and exiting unified desktop
// mode.
TEST_F(ScreenLayoutObserverTest, EnteringExitingUnifiedModeMessage) {
  UpdateDisplay("500x400");
  EXPECT_TRUE(GetDisplayNotificationText().empty());

  // Adding a display in extended mode.
  UpdateDisplay("500x400,300x200");
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_DISPLAY_EXTENDED_NO_INTERNAL),
            GetDisplayNotificationText());

  // Enter unified mode.
  display_manager()->SetUnifiedDesktopEnabled(true);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_UNIFIED),
            GetDisplayNotificationText());

  // Exit unified mode.
  display_manager()->SetUnifiedDesktopEnabled(false);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_UNIFIED_EXITING),
      GetDisplayNotificationText());

  // Enter unified mode again and exit via closing the lid. The message "Exiting
  // unified mode" should be shown.
  display_manager()->SetUnifiedDesktopEnabled(true);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_UNIFIED),
            GetDisplayNotificationText());

  // Close the lid.
  display::Display::SetInternalDisplayId(display_manager()->first_display_id());
  UpdateDisplay("300x200");
  display_manager()->SetUnifiedDesktopEnabled(false);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_UNIFIED_EXITING),
      GetDisplayNotificationText());
}

// Special case: Tests notification messages shown when entering docked mode
// by closing the lid and the internal display is the secondary display.
TEST_F(ScreenLayoutObserverTest, DockedModeWithExternalPrimaryDisplayMessage) {
  UpdateDisplay("500x400,300x200");
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_ASH_STATUS_TRAY_DISPLAY_EXTENDED_NO_INTERNAL),
            GetDisplayNotificationText());
  CloseNotification();

  const int64_t primary_id = display_manager()->GetDisplayAt(0).id();
  const int64_t internal_secondary_id = display_manager()->GetDisplayAt(1).id();
  display::Display::SetInternalDisplayId(internal_secondary_id);
  display::DisplayLayoutBuilder builder(primary_id);
  builder.AddDisplayPlacement(internal_secondary_id, primary_id,
                              display::DisplayPlacement::LEFT, 0);
  display_manager()->SetLayoutForCurrentDisplays(builder.Build());
  EXPECT_TRUE(GetDisplayNotificationText().empty());

  // Close the lid. We go to docked mode, but we show no notifications.
  UpdateDisplay("500x400");
  EXPECT_FALSE(IsNotificationShown());
}

TEST_F(ScreenLayoutObserverTest, MirrorModeAddOrRemoveDisplayMessage) {
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  constexpr int first_display_id = 11;
  constexpr int second_display_id = 12;
  display::ManagedDisplayInfo first_display_info =
      display::CreateDisplayInfo(first_display_id, gfx::Rect(1, 1, 500, 400));
  display::ManagedDisplayInfo second_display_info =
      display::CreateDisplayInfo(second_display_id, gfx::Rect(2, 2, 500, 400));
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(display::CreateDisplayInfo(
      internal_display_id, gfx::Rect(0, 0, 200, 100)));
  display_info_list.push_back(first_display_info);
  display_info_list.push_back(second_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  // Mirroring across 3 displays.
  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, absl::nullopt);
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_MIRRORING,
                                       GetMirroringDisplayNames()),
            GetDisplayNotificationText());

  // Mirror mode persists when a display is removed.
  CloseNotification();
  display_info_list.erase(display_info_list.end() - 1);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_TRUE(GetDisplayNotificationText().empty());
  EXPECT_TRUE(display_manager()->IsInMirrorMode());

  // Turn off mirror mode.
  CloseNotification();
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, absl::nullopt);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_MIRROR_EXIT),
            GetDisplayNotificationText());

  // Turn on mirror mode.
  CloseNotification();
  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, absl::nullopt);
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_MIRRORING,
                                       GetMirroringDisplayNames()),
            GetDisplayNotificationText());

  // Mirror mode ends when only one display is left.
  CloseNotification();
  display_info_list.erase(display_info_list.end() - 1);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_MIRROR_EXIT),
            GetDisplayNotificationText());

  // Mirror mode is restored when the display is reconncted.
  CloseNotification();
  display_info_list.push_back(first_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_MIRRORING,
                                       GetMirroringDisplayNames()),
            GetDisplayNotificationText());

  // Add the other display, the mirror mode persists.
  CloseNotification();
  display_info_list.push_back(second_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_TRUE(GetDisplayNotificationText().empty());
  EXPECT_TRUE(display_manager()->IsInMirrorMode());
}

TEST_F(ScreenLayoutObserverTest, ClickNotification) {
  // Create notification.
  UpdateDisplay("500x400,300x200");
  EXPECT_FALSE(GetDisplayNotificationText().empty());

  // Click notification.
  ClickNotification();
  EXPECT_TRUE(GetDisplayNotificationText().empty());
}

}  // namespace ash
