// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/notification_counter_view.h"

#include <algorithm>

#include "ash/public/cpp/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/message_center/ash_message_center_lock_screen_controller.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "base/i18n/number_formatting.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/message_center/message_center.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

// Maximum count of notification shown by a number label. "+" icon is shown
// instead if it exceeds this limit.
constexpr size_t kTrayNotificationMaxCount = 9;

constexpr double kTrayNotificationCircleIconRadius = 8;

// The size of the number font inside the icon. Should be updated when
// kUnifiedTrayIconSize is changed.
constexpr int kNumberIconFontSize = 11;

gfx::FontList GetNumberIconFontList() {
  // |kNumberIconFontSize| is hard-coded as 11, which whould be updated when
  // the tray icon size is changed.
  DCHECK_EQ(20, kUnifiedTrayIconSize);

  gfx::Font default_font;
  int font_size_delta = kNumberIconFontSize - default_font.GetFontSize();
  gfx::Font font = default_font.Derive(font_size_delta, gfx::Font::NORMAL,
                                       gfx::Font::Weight::BOLD);
  DCHECK_EQ(kNumberIconFontSize, font.GetFontSize());
  return gfx::FontList(font);
}

class NumberIconImageSource : public gfx::CanvasImageSource {
 public:
  explicit NumberIconImageSource(size_t count)
      : CanvasImageSource(
            gfx::Size(kUnifiedTrayIconSize, kUnifiedTrayIconSize)),
        count_(count) {
    DCHECK_LE(count_, kTrayNotificationMaxCount + 1);
  }

  void Draw(gfx::Canvas* canvas) override {
    SkColor tray_icon_color =
        TrayIconColor(Shell::Get()->session_controller()->GetSessionState());
    // Paint the contents inside the circle background. The color doesn't matter
    // as it will be hollowed out by the XOR operation.
    if (count_ > kTrayNotificationMaxCount) {
      canvas->DrawImageInt(
          gfx::CreateVectorIcon(kSystemTrayNotificationCounterPlusIcon,
                                size().width(), tray_icon_color),
          0, 0);
    } else {
      canvas->DrawStringRectWithFlags(
          base::FormatNumber(count_), GetNumberIconFontList(), tray_icon_color,
          gfx::Rect(size()),
          gfx::Canvas::TEXT_ALIGN_CENTER | gfx::Canvas::NO_SUBPIXEL_RENDERING);
    }
    cc::PaintFlags flags;
    flags.setBlendMode(SkBlendMode::kXor);
    flags.setAntiAlias(true);
    flags.setColor(tray_icon_color);
    canvas->DrawCircle(gfx::RectF(gfx::SizeF(size())).CenterPoint(),
                       kTrayNotificationCircleIconRadius, flags);
  }

 private:
  size_t count_;

  DISALLOW_COPY_AND_ASSIGN(NumberIconImageSource);
};

}  // namespace

NotificationCounterView::NotificationCounterView(Shelf* shelf)
    : TrayItemView(shelf) {
  CreateImageView();
  SetVisible(false);
  Shell::Get()->session_controller()->AddObserver(this);
}

NotificationCounterView::~NotificationCounterView() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void NotificationCounterView::Update() {
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  size_t notification_count =
      message_center::MessageCenter::Get()->NotificationCount();
  if (notification_count == 0 ||
      message_center::MessageCenter::Get()->IsQuietMode() ||
      !session_controller->ShouldShowNotificationTray() ||
      (session_controller->IsScreenLocked() &&
       !AshMessageCenterLockScreenController::IsEnabled())) {
    SetVisible(false);
    return;
  }
  int icon_id = std::min(notification_count, kTrayNotificationMaxCount + 1);
  if (icon_id != count_for_display_) {
    image_view()->SetImage(
        gfx::CanvasImageSource::MakeImageSkia<NumberIconImageSource>(icon_id));
    count_for_display_ = icon_id;
  }
  image_view()->set_tooltip_text(l10n_util::GetPluralStringFUTF16(
      IDS_ASH_STATUS_TRAY_NOTIFICATIONS_COUNT_TOOLTIP, notification_count));
  SetVisible(true);
}

void NotificationCounterView::OnSessionStateChanged(
    session_manager::SessionState state) {
  Update();
}

const char* NotificationCounterView::GetClassName() const {
  return "NotificationCounterView";
}

QuietModeView::QuietModeView(Shelf* shelf) : TrayItemView(shelf) {
  CreateImageView();
  image_view()->set_tooltip_text(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_QUIET_MODE_TOOLTIP));
  SetVisible(false);
  Shell::Get()->session_controller()->AddObserver(this);
}

QuietModeView::~QuietModeView() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void QuietModeView::Update() {
  // TODO(yamaguchi): Add this check when new style of the system tray is
  // implemented, so that icon resizing will not happen here.
  // DCHECK_EQ(kTrayIconSize,
  //     gfx::GetDefaultSizeOfVectorIcon(kSystemTrayDoNotDisturbIcon));
  if (message_center::MessageCenter::Get()->IsQuietMode()) {
    image_view()->SetImage(gfx::CreateVectorIcon(
        kSystemTrayDoNotDisturbIcon,
        TrayIconColor(Shell::Get()->session_controller()->GetSessionState())));
    SetVisible(true);
  } else {
    SetVisible(false);
  }
}

void QuietModeView::OnSessionStateChanged(session_manager::SessionState state) {
  Update();
}

const char* QuietModeView::GetClassName() const {
  return "QuietModeView";
}

}  // namespace ash
