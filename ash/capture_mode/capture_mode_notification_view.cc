// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_notification_view.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/scoped_light_mode_as_default.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/events/keyboard_layout_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Constants related to the banner view on the image capture notifications.
constexpr int kBannerHeightDip = 36;
constexpr int kBannerHorizontalInsetDip = 12;
constexpr int kBannerVerticalInsetDip = 8;
constexpr int kBannerIconTextSpacingDip = 8;
constexpr int kBannerIconSizeDip = 20;

// Constants related to the play icon view for video capture notifications.
constexpr int kPlayIconSizeDip = 24;
constexpr int kPlayIconBackgroundCornerRadiusDip = 20;
constexpr gfx::Size kPlayIconViewSize{40, 40};

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
  banner_view->SetBackground(views::CreateSolidBackground(background_color));

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

}  // namespace

CaptureModeNotificationView::CaptureModeNotificationView(
    const message_center::Notification& notification,
    CaptureModeType capture_type)
    : message_center::NotificationViewMD(notification),
      capture_type_(capture_type) {
  // Creates the extra view which will depend on the type of the notification.
  if (!notification.image().IsEmpty())
    CreateExtraView();

  // We need to observe this view as |this| view will be re-used for
  // notifications for with/without image scenarios if |this| is not destroyed
  // by the user or by the timeout before the next notification shows up.
  views::View::AddObserver(this);
}

CaptureModeNotificationView::~CaptureModeNotificationView() = default;

// static
std::unique_ptr<message_center::MessageView>
CaptureModeNotificationView::CreateForImage(
    const message_center::Notification& notification) {
  return std::make_unique<CaptureModeNotificationView>(notification,
                                                       CaptureModeType::kImage);
}

// static
std::unique_ptr<message_center::MessageView>
CaptureModeNotificationView::CreateForVideo(
    const message_center::Notification& notification) {
  return std::make_unique<CaptureModeNotificationView>(notification,
                                                       CaptureModeType::kVideo);
}

void CaptureModeNotificationView::Layout() {
  message_center::NotificationViewMD::Layout();
  if (!extra_view_)
    return;

  gfx::Rect extra_view_bounds = image_container_view()->GetContentsBounds();

  if (capture_type_ == CaptureModeType::kImage) {
    // The extra view in this case is a banner laid out at the bottom of the
    // image container.
    extra_view_bounds.set_y(extra_view_bounds.bottom() - kBannerHeightDip);
    extra_view_bounds.set_height(kBannerHeightDip);
  } else {
    DCHECK_EQ(capture_type_, CaptureModeType::kVideo);
    // The extra view in this case is a play icon centered in the view.
    extra_view_bounds.ClampToCenteredSize(kPlayIconViewSize);
  }

  extra_view_->SetBoundsRect(extra_view_bounds);
}

void CaptureModeNotificationView::OnChildViewAdded(views::View* observed_view,
                                                   views::View* child) {
  if (observed_view == this && child == image_container_view())
    CreateExtraView();
}

void CaptureModeNotificationView::OnChildViewRemoved(views::View* observed_view,
                                                     views::View* child) {
  if (observed_view == this && child == image_container_view())
    extra_view_ = nullptr;
}

void CaptureModeNotificationView::OnViewIsDeleting(View* observed_view) {
  DCHECK_EQ(observed_view, this);
  views::View::RemoveObserver(this);
}

void CaptureModeNotificationView::CreateExtraView() {
  DCHECK(image_container_view());
  DCHECK(!image_container_view()->children().empty());
  DCHECK(!extra_view_);
  extra_view_ = image_container_view()->AddChildView(
      capture_type_ == CaptureModeType::kImage ? CreateBannerView()
                                               : CreatePlayIconView());
}

}  // namespace ash
