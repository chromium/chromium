// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_util.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/stop_recording_button_tray.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "ash/public/cpp/style/scoped_light_mode_as_default.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/check.h"
#include "base/notreached.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/events/keyboard_layout_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/views/notification_background_painter.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"

namespace {
constexpr int kBannerViewTopRadius = 0;
constexpr int kBannerViewBottomRadius = 8;
}  // namespace

namespace ash {

namespace capture_mode_util {

bool IsCaptureModeActive() {
  return CaptureModeController::Get()->IsActive();
}

gfx::Point GetLocationForFineTunePosition(const gfx::Rect& rect,
                                          FineTunePosition position) {
  switch (position) {
    case FineTunePosition::kTopLeft:
      return rect.origin();
    case FineTunePosition::kTopCenter:
      return rect.top_center();
    case FineTunePosition::kTopRight:
      return rect.top_right();
    case FineTunePosition::kRightCenter:
      return rect.right_center();
    case FineTunePosition::kBottomRight:
      return rect.bottom_right();
    case FineTunePosition::kBottomCenter:
      return rect.bottom_center();
    case FineTunePosition::kBottomLeft:
      return rect.bottom_left();
    case FineTunePosition::kLeftCenter:
      return rect.left_center();
    default:
      break;
  }

  NOTREACHED();
  return gfx::Point();
}

bool IsCornerFineTunePosition(FineTunePosition position) {
  switch (position) {
    case FineTunePosition::kTopLeft:
    case FineTunePosition::kTopRight:
    case FineTunePosition::kBottomRight:
    case FineTunePosition::kBottomLeft:
      return true;
    default:
      break;
  }
  return false;
}

void SetStopRecordingButtonVisibility(aura::Window* root, bool visible) {
  DCHECK(root);
  DCHECK(root->IsRootWindow());

  // Recording can end when a display being fullscreen-captured gets removed, in
  // this case, we don't need to hide the button.
  if (root->is_destroying()) {
    DCHECK(!visible);
    return;
  }

  // Can be null while shutting down.
  auto* root_window_controller = RootWindowController::ForWindow(root);
  if (!root_window_controller)
    return;

  auto* stop_recording_button = root_window_controller->GetStatusAreaWidget()
                                    ->stop_recording_button_tray();
  DCHECK(stop_recording_button);
  stop_recording_button->SetVisiblePreferred(visible);
}

void TriggerAccessibilityAlert(const std::string& message) {
  Shell::Get()
      ->accessibility_controller()
      ->TriggerAccessibilityAlertWithMessage(message);
}

void TriggerAccessibilityAlert(int message_id) {
  TriggerAccessibilityAlert(l10n_util::GetStringUTF8(message_id));
}

std::unique_ptr<views::View> CreateClipboardShortcutView() {
  std::unique_ptr<views::View> clipboard_shortcut_view =
      std::make_unique<views::View>();

  auto* color_provider = AshColorProvider::Get();
  const SkColor background_color = color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorActive);
  // The text and icon are showing on the background with |background_color|
  // so its color is same with kButtonLabelColorPrimary although they're
  // not theoretically showing on a button.
  const SkColor text_icon_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonLabelColorPrimary);
  clipboard_shortcut_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));

  const std::u16string shortcut_key = l10n_util::GetStringUTF16(
      ui::DeviceUsesKeyboardLayout2() ? IDS_ASH_SHORTCUT_MODIFIER_LAUNCHER
                                      : IDS_ASH_SHORTCUT_MODIFIER_SEARCH);

  const std::u16string label_text = l10n_util::GetStringFUTF16(
      IDS_ASH_MULTIPASTE_SCREENSHOT_NOTIFICATION_NUDGE, shortcut_key);

  views::Label* shortcut_label =
      clipboard_shortcut_view->AddChildView(std::make_unique<views::Label>());
  shortcut_label->SetText(label_text);
  shortcut_label->SetBackgroundColor(background_color);
  shortcut_label->SetEnabledColor(text_icon_color);

  return clipboard_shortcut_view;
}

// Creates the banner view that will show on top of the notification image.
std::unique_ptr<views::View> CreateBannerView() {
  std::unique_ptr<views::View> banner_view = std::make_unique<views::View>();

  // Use the light mode as default as notification is still using light
  // theme as the default theme.
  ScopedLightModeAsDefault scoped_light_mode_as_default;

  auto* color_provider = AshColorProvider::Get();
  const SkColor background_color = color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorActive);
  // The text and icon are showing on the background with |background_color|
  // so its color is same with kButtonLabelColorPrimary although they're
  // not theoretically showing on a button.
  const SkColor text_icon_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonLabelColorPrimary);
  auto* layout =
      banner_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(kBannerVerticalInsetDip, kBannerHorizontalInsetDip),
          kBannerIconTextSpacingDip));

  if (features::IsNotificationsRefreshEnabled()) {
    banner_view->SetBackground(views::CreateBackgroundFromPainter(
        std::make_unique<message_center::NotificationBackgroundPainter>(
            kBannerViewTopRadius, kBannerViewBottomRadius, background_color)));
  } else {
    banner_view->SetBackground(views::CreateSolidBackground(background_color));
  }

  views::ImageView* icon =
      banner_view->AddChildView(std::make_unique<views::ImageView>());
  icon->SetImage(gfx::CreateVectorIcon(kCaptureModeCopiedToClipboardIcon,
                                       kBannerIconSizeDip, text_icon_color));

  views::Label* label = banner_view->AddChildView(
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_ASH_SCREEN_CAPTURE_SCREENSHOT_COPIED_TO_CLIPBOARD)));
  label->SetBackgroundColor(background_color);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetEnabledColor(text_icon_color);

  if (!Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    if (features::IsClipboardHistoryScreenshotNudgeEnabled()) {
      banner_view->AddChildView(CreateClipboardShortcutView());
      layout->SetFlexForView(label, 1);
    }

    // Notify the clipboard history of the created notification.
    ClipboardHistoryController::Get()->OnScreenshotNotificationCreated();
  }
  return banner_view;
}

// Creates the play icon view which shows on top of the video thumbnail in the
// notification.
std::unique_ptr<views::View> CreatePlayIconView() {
  auto play_view = std::make_unique<views::ImageView>();
  auto* color_provider = AshColorProvider::Get();
  const SkColor icon_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary);
  play_view->SetImage(gfx::CreateVectorIcon(kCaptureModePlayIcon,
                                            kPlayIconSizeDip, icon_color));
  play_view->SetHorizontalAlignment(views::ImageView::Alignment::kCenter);
  play_view->SetVerticalAlignment(views::ImageView::Alignment::kCenter);
  const SkColor background_color = color_provider->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80);
  play_view->SetBackground(views::CreateRoundedRectBackground(
      background_color, kPlayIconBackgroundCornerRadiusDip));
  return play_view;
}

}  // namespace capture_mode_util

}  // namespace ash
