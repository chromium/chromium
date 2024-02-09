// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/screen_layout_observer.h"

#include <string>

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/util/display_manager_test_util.h"
#include "ui/display/manager/util/display_manager_util.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/display/util/display_util.h"
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
  std::u16string GetDisplayNotificationText() const;
  std::u16string GetDisplayNotificationAdditionalText() const;

  std::u16string GetFirstDisplayName();

  std::u16string GetSecondDisplayName();

  std::u16string GetMirroringDisplayNames();

  std::u16string GetUnifiedDisplayName();

  bool IsNotificationShown() const;

  display::ManagedDisplayInfo CreateDisplayInfo(int64_t id,
                                                const gfx::Rect& bounds);

 private:
  const message_center::Notification* GetDisplayNotification() const;
};
ScreenLayoutObserverTest::ScreenLayoutObserverTest() = default;

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

display::ManagedDisplayInfo ScreenLayoutObserverTest::CreateDisplayInfo(
    int64_t id,
    const gfx::Rect& bounds) {
  display::ManagedDisplayInfo info = display::CreateDisplayInfo(id, bounds);
  // Each display should have at least one native mode.
  display::ManagedDisplayMode mode(bounds.size(), /*refresh_rate=*/60.f,
                                   /*is_interlaced=*/true,
                                   /*native=*/true);
  info.SetManagedDisplayModes({mode});
  return info;
}

const message_center::Notification*
ScreenLayoutObserverTest::GetDisplayNotification() const {
  const message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  for (const message_center::Notification* notification : notifications) {
    if (notification->id() == ScreenLayoutObserver::kNotificationId)
      return notification;
  }

  return nullptr;
}

// This test is flaky. crbug.com/1222612
TEST_F(ScreenLayoutObserverTest, DISABLED_DisplayNotifications) {
  UpdateDisplay("500x400");
  display::SetInternalDisplayIds({display_manager()->first_display_id()});
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
      display::SynthesizeDisplayIdFromSeed(first_display_id);
  display::ManagedDisplayInfo first_display_info =
      CreateDisplayInfo(first_display_id, gfx::Rect(1, 1, 500, 500));
  display::ManagedDisplayInfo second_display_info =
      CreateDisplayInfo(second_display_id, gfx::Rect(2, 2, 500, 500));
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
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, std::nullopt);
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
  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, std::nullopt);
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
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, std::nullopt);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DISPLAY_MIRROR_EXIT),
            GetDisplayNotificationText());
  EXPECT_TRUE(GetDisplayNotificationAdditionalText().empty());

  // Enters closed lid mode.
  UpdateDisplay("500x400@1.5,300x200");
  display::SetInternalDisplayIds(
      {display::test::DisplayManagerTestApi(display_manager())
           .GetSecondaryDisplay()
           .id()});
  UpdateDisplay("500x400@1.5");
  EXPECT_TRUE(GetDisplayNotificationText().empty());
}

TEST_F(ScreenLayoutObserverTest, DisplayNotificationsDisabled) {
  UpdateDisplay("500x400");
  display::SetInternalDisplayIds({display_manager()->first_display_id()});
  EXPECT_TRUE(GetDisplayNotificationText().empty());

  // Adding a display.
  UpdateDisplay("500x400,300x200");
  EXPECT_FALSE(IsNotificationShown());

  const int64_t first_display_id =
      display::Screen::GetScreen()->GetPrimaryDisplay().id();
  const int64_t second_display_id =
      display::SynthesizeDisplayIdFromSeed(first_display_id);
  display::ManagedDisplayInfo first_display_info =
      CreateDisplayInfo(first_display_id, gfx::Rect(1, 1, 500, 400));
  display::ManagedDisplayInfo second_display_info =
      CreateDisplayInfo(second_display_id, gfx::Rect(2, 2, 500, 400));
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
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, std::nullopt);
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

// Verify that no notification is shown when overscan of a screen is changed.
TEST_F(ScreenLayoutObserverTest, OverscanDisplay) {
  UpdateDisplay("500x400, 400x300");
  // Close the notification that is shown from initially adding a monitor.
  CloseNotification();
  display::SetInternalDisplayIds({display_manager()->first_display_id()});

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
}  // namespace ash
