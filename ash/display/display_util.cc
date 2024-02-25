// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_util.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/constants/notifier_catalogs.h"
#include "ash/display/display_configuration_controller.h"
#include "ash/display/extended_mouse_warp_controller.h"
#include "ash/display/null_mouse_warp_controller.h"
#include "ash/display/unified_mouse_warp_controller.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_layout_store.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

const char kDisplayErrorNotificationId[] = "chrome://settings/display/error";
const char kNotifierDisplayError[] = "ash.display.error";

void ConvertPointFromScreenToNative(aura::WindowTreeHost* host,
                                    gfx::Point* point) {
  ::wm::ConvertPointFromScreen(host->window(), point);
  host->ConvertDIPToScreenInPixels(point);
}

}  // namespace

std::unique_ptr<MouseWarpController> CreateMouseWarpController(
    display::DisplayManager* manager,
    aura::Window* drag_source) {
  if (manager->IsInUnifiedMode() && manager->num_connected_displays() >= 2) {
    return std::make_unique<UnifiedMouseWarpController>();
  }
  // Extra check for |num_connected_displays()| is for SystemDisplayApiTest
  // that injects MockScreen.
  if (manager->GetNumDisplays() < 2 || manager->num_connected_displays() < 2) {
    return std::make_unique<NullMouseWarpController>();
  }
  return std::make_unique<ExtendedMouseWarpController>(drag_source);
}

gfx::Rect GetNativeEdgeBounds(AshWindowTreeHost* ash_host,
                              const gfx::Rect& bounds_in_screen) {
  aura::WindowTreeHost* host = ash_host->AsWindowTreeHost();
  gfx::Rect native_bounds = host->GetBoundsInPixels();
  native_bounds.Inset(ash_host->GetHostInsets());
  gfx::Point start_in_native = bounds_in_screen.origin();
  gfx::Point end_in_native = bounds_in_screen.bottom_right();

  ConvertPointFromScreenToNative(host, &start_in_native);
  ConvertPointFromScreenToNative(host, &end_in_native);

  if (std::abs(start_in_native.x() - end_in_native.x()) <
      std::abs(start_in_native.y() - end_in_native.y())) {
    // vertical in native
    int x = std::abs(native_bounds.x() - start_in_native.x()) <
                    std::abs(native_bounds.right() - start_in_native.x())
                ? native_bounds.x()
                : native_bounds.right() - 1;
    return gfx::Rect(x, std::min(start_in_native.y(), end_in_native.y()), 1,
                     std::abs(end_in_native.y() - start_in_native.y()));
  } else {
    // horizontal in native
    int y = std::abs(native_bounds.y() - start_in_native.y()) <
                    std::abs(native_bounds.bottom() - start_in_native.y())
                ? native_bounds.y()
                : native_bounds.bottom() - 1;
    return gfx::Rect(std::min(start_in_native.x(), end_in_native.x()), y,
                     std::abs(end_in_native.x() - start_in_native.x()), 1);
  }
}

// Moves the cursor to the point inside the root that is closest to
// the point_in_screen, which is outside of the root window.
void MoveCursorTo(AshWindowTreeHost* ash_host,
                  const gfx::Point& point_in_screen,
                  bool update_last_location_now) {
  aura::WindowTreeHost* host = ash_host->AsWindowTreeHost();
  gfx::Point point_in_native = point_in_screen;
  ::wm::ConvertPointFromScreen(host->window(), &point_in_native);
  host->ConvertDIPToScreenInPixels(&point_in_native);

  // now fit the point inside the native bounds.
  gfx::Rect native_bounds = host->GetBoundsInPixels();
  gfx::Point native_origin = native_bounds.origin();
  native_bounds.Inset(ash_host->GetHostInsets());
  // Shrink further so that the mouse doesn't warp on the
  // edge. The right/bottom needs to be shrink by 2 to subtract
  // the 1 px from width/height value.
  native_bounds.Inset(gfx::Insets::TLBR(1, 1, 2, 2));

  // Ensure that |point_in_native| is inside the |native_bounds|.
  point_in_native.SetToMax(native_bounds.origin());
  point_in_native.SetToMin(native_bounds.bottom_right());

  gfx::Point point_in_host = point_in_native;

  point_in_host.Offset(-native_origin.x(), -native_origin.y());
  host->MoveCursorToLocationInPixels(point_in_host);

  if (update_last_location_now) {
    gfx::Point new_point_in_screen = point_in_native;
    host->ConvertScreenInPixelsToDIP(&new_point_in_screen);
    ::wm::ConvertPointToScreen(host->window(), &new_point_in_screen);

    if (Shell::Get()->display_manager()->IsInUnifiedMode()) {
      // In unified desktop mode, the mirroring host converts the point to the
      // unified host's pixel coordinates, so we also need to apply the unified
      // host transform to get a point in the unified screen coordinates to take
      // into account any device scale factors or ui scaling.
      Shell::GetPrimaryRootWindow()->GetHost()->ConvertScreenInPixelsToDIP(
          &new_point_in_screen);
    }
    aura::Env::GetInstance()->SetLastMouseLocation(new_point_in_screen);
  }
}

void ShowDisplayErrorNotification(const std::u16string& message,
                                  bool allow_feedback) {
  // Always remove the notification to make sure the notification appears
  // as a popup in any situation.
  message_center::MessageCenter::Get()->RemoveNotification(
      kDisplayErrorNotificationId, false /* by_user */);

  message_center::RichNotificationData data;
  if (allow_feedback) {
    message_center::ButtonInfo send_button(
        l10n_util::GetStringUTF16(IDS_ASH_DISPLAY_FAILURE_SEND_FEEDBACK));
    data.buttons.push_back(send_button);
  }

  std::unique_ptr<message_center::Notification> notification =
      CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE, kDisplayErrorNotificationId,
          std::u16string(),  // title
          message,
          std::u16string(),  // display_source
          GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotifierDisplayError, NotificationCatalogName::kDisplayError),
          data,
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating([](std::optional<int> button_index) {
                if (button_index) {
                  NewWindowDelegate::GetInstance()->OpenFeedbackPage();
                }
              })),
          kNotificationMonitorWarningIcon,
          message_center::SystemNotificationWarningLevel::WARNING);
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

bool IsRectContainedByAnyDisplay(const gfx::Rect& rect_in_screen) {
  const std::vector<display::Display>& displays =
      display::Screen::GetScreen()->GetAllDisplays();
  for (const auto& display : displays) {
    if (display.bounds().Contains(rect_in_screen)) {
      return true;
    }
  }
  return false;
}

std::u16string ConvertRefreshRateToString16(float refresh_rate) {
  std::string str = base::StringPrintf("%.2f", refresh_rate);

  // Remove the mantissa for whole numbers.
  if (EndsWith(str, ".00", base::CompareCase::INSENSITIVE_ASCII)) {
    str.erase(str.length() - 3);
  }

  return base::UTF8ToUTF16(str);
}

std::u16string GetDisplayErrorNotificationMessageForTest() {
  message_center::NotificationList::Notifications notifications =
      message_center::MessageCenter::Get()->GetVisibleNotifications();
  for (message_center::Notification* const notification : notifications) {
    if (notification->id() == kDisplayErrorNotificationId) {
      return notification->message();
    }
  }
  return std::u16string();
}

bool ShouldUndoRotationForMirror() {
  return Shell::Get()->tablet_mode_controller()->is_in_tablet_physical_state();
}

}  // namespace ash
