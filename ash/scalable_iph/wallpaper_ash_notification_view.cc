// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scalable_iph/wallpaper_ash_notification_view.h"

#include "ash/public/cpp/rounded_image_view.h"
#include "ash/scalable_iph/scalable_iph_ash_notification_view.h"
#include "base/check.h"
#include "base/notreached.h"
#include "build/buildflag.h"
#include "chromeos/ash/components/scalable_iph/buildflags.h"
#include "chromeos/ash/grit/ash_resources.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/layout/flex_layout_view.h"

namespace ash {

namespace {

constexpr int kNumPreviews = 4;
constexpr int kCornerRadiusDip = 8;

#if BUILDFLAG(ENABLE_CROS_SCALABLE_IPH)
int GetResourceId(int preview_index) {
  switch (preview_index) {
    case 0:
      return IDR_SCALABLE_IPH_NOTIFICATION_WALLPAPER_1_PNG;
    case 1:
      return IDR_SCALABLE_IPH_NOTIFICATION_WALLPAPER_2_PNG;
    case 2:
      return IDR_SCALABLE_IPH_NOTIFICATION_WALLPAPER_3_PNG;
    case 3:
      return IDR_SCALABLE_IPH_NOTIFICATION_WALLPAPER_4_PNG;
    default:
      CHECK(false);
  }
  NOTREACHED();
}
#endif  // BUILDFLAG(ENABLE_CROS_SCALABLE_IPH)

void SetImage(RoundedImageView* image_view, int preview_index) {
#if BUILDFLAG(ENABLE_CROS_SCALABLE_IPH)
  constexpr gfx::Size kImageSizeDip = gfx::Size(60, 48);
  constexpr int kMarginDip = 4;
  gfx::ImageSkia* image =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          GetResourceId(preview_index));
  image->EnsureRepsForSupportedScales();
  image_view->SetImage(*image, kImageSizeDip);

  if (preview_index > 0) {
    image_view->SetProperty(views::kMarginsKey,
                            gfx::Insets::TLBR(0, kMarginDip, 0, 0));
  }
#endif  // BUILDFLAG(ENABLE_CROS_SCALABLE_IPH)
}

}  // namespace

WallpaperAshNotificationView::WallpaperAshNotificationView(
    const message_center::Notification& notification,
    bool shown_in_popup)
    : ScalableIphAshNotificationView(notification, shown_in_popup) {
  UpdateWithNotification(notification);
}

WallpaperAshNotificationView::~WallpaperAshNotificationView() = default;

// static
std::unique_ptr<message_center::MessageView>
WallpaperAshNotificationView::CreateWithPreview(
    const message_center::Notification& notification,
    bool shown_in_popup) {
  return std::make_unique<WallpaperAshNotificationView>(notification,
                                                        shown_in_popup);
}

void WallpaperAshNotificationView::UpdateWithNotification(
    const message_center::Notification& notification) {
  preview_ = nullptr;

  ScalableIphAshNotificationView::UpdateWithNotification(notification);
  CreatePreview();
}

void WallpaperAshNotificationView::CreatePreview() {
  DCHECK(image_container_view());
  DCHECK(!preview_);

  // Replace current notification image with four preview images.
  image_container_view()->RemoveAllChildViews();
  preview_ = image_container_view()->AddChildView(
      std::make_unique<views::FlexLayoutView>());

  for (int i = 0; i < kNumPreviews; ++i) {
    image_views_[i] = preview_->AddChildView(std::make_unique<RoundedImageView>(
        kCornerRadiusDip, RoundedImageView::Alignment::kLeading));
    SetImage(image_views_[i], i);
  }
}

BEGIN_METADATA(WallpaperAshNotificationView)
END_METADATA

}  // namespace ash
