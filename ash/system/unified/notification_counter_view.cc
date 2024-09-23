// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/notification_counter_view.h"

#include <optional>
#include <string>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/notification_center/ash_message_center_lock_screen_controller.h"
#include "ash/system/notification_center/message_center_utils.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/unified/notification_icons_controller.h"
#include "base/i18n/number_formatting.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/message_center/message_center.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/separator.h"

namespace ash {

namespace {

constexpr double kTrayNotificationCircleIconRadius = 8;

// The size of the number font inside the icon. Should be updated when
// kUnifiedTrayIconSize is changed.
constexpr int kNumberIconFontSize = 11;

constexpr auto kSeparatorPadding = gfx::Insets::VH(6, 4);

const gfx::FontList& GetNumberIconFontList() {
  // |kNumberIconFontSize| is hard-coded as 11, which should be updated when
  // the tray icon size is changed.
  DCHECK_EQ(18, kUnifiedTrayIconSize);

  static gfx::FontList font_list({"Roboto"}, gfx::Font::NORMAL,
                                 kNumberIconFontSize,
                                 gfx::Font::Weight::MEDIUM);
  return font_list;
}

ui::ColorId SeparatorIconColorId(session_manager::SessionState state) {
  if (state == session_manager::SessionState::OOBE) {
    return ui::kColorAshIconInOobe;
  }
  return ui::kColorAshSystemUIMenuSeparator;
}

// Returns true if we should show the counter view (e.g. during quiet mode,
// screen lock, etc.).
bool ShouldShowCounterView() {
  // The `NotificationCounterView` should only be hidden if the screen is not
  // locked and quiet mode is enabled.
  return !message_center::MessageCenter::Get()->IsQuietMode() ||
         Shell::Get()->session_controller()->IsScreenLocked();
}

class NumberIconImageSource : public gfx::CanvasImageSource {
 public:
  explicit NumberIconImageSource(
      NotificationCounterView* NotificationCounterView,
      size_t count)
      : CanvasImageSource(
            gfx::Size(kUnifiedTrayIconSize, kUnifiedTrayIconSize)),
        notification_counter_view_(NotificationCounterView),
        count_(count) {
    DCHECK_LE(count_, kTrayNotificationMaxCount + 1);
  }

  NumberIconImageSource(const NumberIconImageSource&) = delete;
  NumberIconImageSource& operator=(const NumberIconImageSource&) = delete;

  void Draw(gfx::Canvas* canvas) override {
    ui::ColorId tray_icon_color_id;
    tray_icon_color_id = notification_counter_view_->is_active()
                             ? cros_tokens::kCrosSysSystemOnPrimaryContainer
                             : cros_tokens::kCrosSysOnSurface;

    const SkColor tray_icon_color =
        notification_counter_view_->GetColorProvider()->GetColor(
            tray_icon_color_id);
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
  raw_ptr<NotificationCounterView> notification_counter_view_;
  size_t count_;
};

}  // namespace

// NotificationCounterView -----------------------------------------------------

NotificationCounterView::NotificationCounterView(
    Shelf* shelf,
    NotificationIconsController* controller)
    : TrayItemView(shelf), controller_(controller) {
  CreateImageView();
  SetVisible(false);
}

NotificationCounterView::~NotificationCounterView() = default;

void NotificationCounterView::Update() {
  if (message_center_utils::GetNotificationCount() == 0 ||
      !ShouldShowCounterView()) {
    SetVisible(false);
    return;
  }

  // If the tray is showing notification icons, display the count of
  // notifications not showing. Otherwise, show the count of total
  // notifications.
  size_t notification_count;
  if (controller_->icons_view_visible() &&
      controller_->TrayItemHasNotification()) {
    notification_count = message_center_utils::GetNotificationCount() -
                         controller_->TrayNotificationIconsCount();
    if (notification_count == 0) {
      SetVisible(false);
      return;
    }
    image_view()->SetTooltipText(l10n_util::GetPluralStringFUTF16(
        IDS_ASH_STATUS_TRAY_NOTIFICATIONS_HIDDEN_COUNT_TOOLTIP,
        notification_count));
  } else {
    notification_count = message_center_utils::GetNotificationCount();
    image_view()->SetTooltipText(l10n_util::GetPluralStringFUTF16(
        IDS_ASH_STATUS_TRAY_NOTIFICATIONS_COUNT_TOOLTIP, notification_count));
  }

  int icon_id = std::min(notification_count, kTrayNotificationMaxCount + 1);
  if (icon_id != count_for_display_) {
    count_for_display_ = icon_id;
    image_view()->SetImage(
        gfx::CanvasImageSource::MakeImageSkia<NumberIconImageSource>(this,
                                                                     icon_id));
    UpdateLabelOrImageViewColor(is_active());
  }
  SetVisible(true);
}

std::optional<std::u16string> NotificationCounterView::GetAccessibleNameString()
    const {
  return GetVisible() ? std::optional(image_view()->GetTooltipText())
                      : std::nullopt;
}

void NotificationCounterView::HandleLocaleChange() {
  Update();
}

void NotificationCounterView::OnThemeChanged() {
  TrayItemView::OnThemeChanged();
  UpdateLabelOrImageViewColor(is_active());
}

void NotificationCounterView::UpdateLabelOrImageViewColor(bool active) {
  TrayItemView::UpdateLabelOrImageViewColor(active);

  image_view()->SetImage(
      gfx::CanvasImageSource::MakeImageSkia<NumberIconImageSource>(
          this, count_for_display_));
}

BEGIN_METADATA(NotificationCounterView)
END_METADATA

// QuietModeView ---------------------------------------------------------------

QuietModeView::QuietModeView(Shelf* shelf) : TrayItemView(shelf) {
  CreateImageView();
  image_view()->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_QUIET_MODE_TOOLTIP));
  SetVisible(false);
}

QuietModeView::~QuietModeView() = default;

const std::u16string& QuietModeView::GetAccessibleNameString() const {
  return image_view()->GetTooltipText();
}

void QuietModeView::Update() {
  if (message_center::MessageCenter::Get()->IsQuietMode() &&
      Shell::Get()->session_controller()->GetSessionState() ==
          session_manager::SessionState::ACTIVE) {
    SetVisible(true);
    UpdateLabelOrImageViewColor(is_active());
  } else {
    SetVisible(false);
  }
}

void QuietModeView::HandleLocaleChange() {
  image_view()->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_QUIET_MODE_TOOLTIP));
}

void QuietModeView::OnThemeChanged() {
  TrayItemView::OnThemeChanged();
  Update();
}

void QuietModeView::UpdateLabelOrImageViewColor(bool active) {
  TrayItemView::UpdateLabelOrImageViewColor(active);

  image_view()->SetImage(ui::ImageModel::FromVectorIcon(
      kSystemTrayDoNotDisturbIcon,
      active ? cros_tokens::kCrosSysSystemOnPrimaryContainer
             : cros_tokens::kCrosSysOnSurface));
}

BEGIN_METADATA(QuietModeView)
END_METADATA

SeparatorTrayItemView::SeparatorTrayItemView(Shelf* shelf)
    : TrayItemView(shelf) {
  auto separator = std::make_unique<views::Separator>();
  separator->SetColorId(SeparatorIconColorId(
      Shell::Get()->session_controller()->GetSessionState()));
  separator->SetBorder(views::CreateEmptyBorder(kSeparatorPadding));
  separator_ = AddChildView(std::move(separator));

  set_use_scale_in_animation(false);
}

SeparatorTrayItemView::~SeparatorTrayItemView() = default;

void SeparatorTrayItemView::HandleLocaleChange() {}

void SeparatorTrayItemView::UpdateColor(session_manager::SessionState state) {
  separator_->SetColorId(SeparatorIconColorId(state));
}

BEGIN_METADATA(SeparatorTrayItemView)
END_METADATA

}  // namespace ash
