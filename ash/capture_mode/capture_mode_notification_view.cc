// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_notification_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/scoped_light_mode_as_default.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Constants related to the banner view on the image capture notification.
constexpr int kBannerHeightDip = 36;
constexpr int kBannerHorizontalInsetDip = 12;
constexpr int kBannerVerticalInsetDip = 8;
constexpr int kBannerIconTextSpacingDip = 8;
constexpr int kBannerIconSizeDip = 20;

// Creates the banner view that will show on top of the notification image.
std::unique_ptr<views::View> CreateBannerViewImpl() {
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
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(kBannerVerticalInsetDip, kBannerHorizontalInsetDip),
      kBannerIconTextSpacingDip);
  banner_view->SetLayoutManager(std::move(layout));
  banner_view->SetBackground(views::CreateSolidBackground(background_color));

  views::ImageView* icon =
      banner_view->AddChildView(std::make_unique<views::ImageView>());
  icon->SetImage(gfx::CreateVectorIcon(kCaptureModeCopiedToClipboardIcon,
                                       kBannerIconSizeDip, text_icon_color));

  views::Label* label = banner_view->AddChildView(
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(
          IDS_ASH_SCREEN_CAPTURE_SCREENSHOT_COPIED_TO_CLIPBOARD)));
  label->SetBackgroundColor(background_color);
  label->SetEnabledColor(text_icon_color);

  return banner_view;
}

}  // namespace

CaptureModeNotificationView::CaptureModeNotificationView(
    const message_center::Notification& notification)
    : message_center::NotificationViewMD(notification) {
  // Create the banner view if notification image is not empty. The banner
  // will show on top of the notification image.
  if (!notification.image().IsEmpty())
    CreateBannerView();

  // We need to observe this view as |this| view will be re-used for
  // notifications for with/without image scenarios if |this| is not destroyed
  // by the user or by the timeout before the next notification shows up.
  views::View::AddObserver(this);
}

CaptureModeNotificationView::~CaptureModeNotificationView() = default;

// static
std::unique_ptr<message_center::MessageView>
CaptureModeNotificationView::Create(
    const message_center::Notification& notification) {
  return std::make_unique<CaptureModeNotificationView>(notification);
}

void CaptureModeNotificationView::Layout() {
  message_center::NotificationViewMD::Layout();
  if (!banner_view_)
    return;

  // Calculate the banner view's desired bounds.
  gfx::Rect banner_bounds = image_container_view()->GetContentsBounds();
  banner_bounds.set_y(banner_bounds.bottom() - kBannerHeightDip);
  banner_bounds.set_height(kBannerHeightDip);
  banner_view_->SetBoundsRect(banner_bounds);
}

void CaptureModeNotificationView::OnChildViewAdded(views::View* observed_view,
                                                   views::View* child) {
  if (observed_view == this && child == image_container_view())
    CreateBannerView();
}

void CaptureModeNotificationView::OnChildViewRemoved(views::View* observed_view,
                                                     views::View* child) {
  if (observed_view == this && child == image_container_view())
    banner_view_ = nullptr;
}

void CaptureModeNotificationView::OnViewIsDeleting(View* observed_view) {
  DCHECK_EQ(observed_view, this);
  views::View::RemoveObserver(this);
}

void CaptureModeNotificationView::CreateBannerView() {
  DCHECK(image_container_view());
  DCHECK(!image_container_view()->children().empty());
  DCHECK(!banner_view_);
  banner_view_ = image_container_view()->AddChildView(CreateBannerViewImpl());
}

}  // namespace ash
