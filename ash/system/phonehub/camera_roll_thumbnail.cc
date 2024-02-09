// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/camera_roll_thumbnail.h"

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "base/functional/bind.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/camera_roll_manager.h"
#include "chromeos/ash/components/phonehub/user_action_recorder.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {

namespace {

// Appearance in dip.
constexpr int kCameraRollThumbnailBorderRadius = 12;
constexpr gfx::Size kCameraRollThumbnailBorderSize(74, 74);
constexpr gfx::Point kCameraRollThumbnailVideoCircleOrigin(37, 37);
constexpr int kCameraRollThumbnailVideoCircleRadius = 16;
constexpr gfx::Point kCameraRollThumbnailVideoIconOrigin(27, 27);
constexpr int kCameraRollThumbnailVideoIconSize = 20;

}  // namespace

CameraRollThumbnail::CameraRollThumbnail(
    const int index,
    const phonehub::CameraRollItem& item,
    phonehub::CameraRollManager* camera_roll_manager,
    phonehub::UserActionRecorder* user_action_recorder)
    : views::MenuButton(base::BindRepeating(&CameraRollThumbnail::ButtonPressed,
                                            base::Unretained(this))),
      index_(index),
      metadata_(item.metadata()),
      video_type_(metadata_.mime_type().find("video/") == 0),
      image_(item.thumbnail().AsImageSkia()),
      camera_roll_manager_(camera_roll_manager),
      user_action_recorder_(user_action_recorder) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  views::FocusRing::Get(this)->SetColorId(ui::kColorAshFocusRing);
  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(), kCameraRollThumbnailBorderRadius);

  SetClipPath(SkPath::RRect(SkRRect::MakeRectXY(
      SkRect::Make(SkIRect::MakeWH(kCameraRollThumbnailBorderSize.width(),
                                   kCameraRollThumbnailBorderSize.height())),
      SkIntToScalar(kCameraRollThumbnailBorderRadius),
      SkIntToScalar(kCameraRollThumbnailBorderRadius))));

  set_context_menu_controller(this);

  phone_hub_metrics::LogCameraRollContentShown(index_, GetMediaType());
}

CameraRollThumbnail::~CameraRollThumbnail() = default;

void CameraRollThumbnail::PaintButtonContents(gfx::Canvas* canvas) {
  views::MenuButton::PaintButtonContents(canvas);

  canvas->DrawColor(
      GetColorProvider()->GetColor(kColorAshControlBackgroundColorInactive));

  canvas->DrawImageInt(image_, 0, 0, image_.width(), image_.height(), 0, 0,
                       kCameraRollThumbnailBorderSize.width(),
                       kCameraRollThumbnailBorderSize.height(), false);

  if (video_type_) {
    auto* color_provider = AshColorProvider::Get();
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(GetColorProvider()->GetColor(kColorAshShieldAndBase80));
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawCircle(kCameraRollThumbnailVideoCircleOrigin,
                       kCameraRollThumbnailVideoCircleRadius, flags);
    canvas->DrawImageInt(
        CreateVectorIcon(
            kPhoneHubCameraRollItemVideoIcon, kCameraRollThumbnailVideoIconSize,
            color_provider->GetContentLayerColor(
                AshColorProvider::ContentLayerType::kIconColorPrimary)),
        kCameraRollThumbnailVideoIconOrigin.x(),
        kCameraRollThumbnailVideoIconOrigin.y());
  }
}

void CameraRollThumbnail::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  phone_hub_metrics::LogCameraRollContentClicked(index_, GetMediaType());
  menu_runner_ = std::make_unique<views::MenuRunner>(
      GetMenuModel(), views::MenuRunner::CONTEXT_MENU |
                          views::MenuRunner::FIXED_ANCHOR |
                          views::MenuRunner::USE_ASH_SYS_UI_LAYOUT);
  menu_runner_->RunMenuAt(GetWidget(), button_controller(), GetBoundsInScreen(),
                          views::MenuAnchorPosition::kBubbleTopRight,
                          ui::MENU_SOURCE_NONE);
}

void CameraRollThumbnail::ButtonPressed() {
  if (base::TimeTicks::Now() - download_throttle_timestamp_ <
      features::kPhoneHubCameraRollThrottleInterval.Get()) {
    return;
  }

  download_throttle_timestamp_ = base::TimeTicks::Now();
  phone_hub_metrics::LogCameraRollContentClicked(index_, GetMediaType());
  DownloadRequested();
}

ui::SimpleMenuModel* CameraRollThumbnail::GetMenuModel() {
  if (!menu_model_)
    menu_model_ = std::make_unique<CameraRollMenuModel>(base::BindRepeating(
        &CameraRollThumbnail::DownloadRequested, base::Unretained(this)));
  return menu_model_.get();
}

void CameraRollThumbnail::DownloadRequested() {
  PA_LOG(INFO) << "Downloading Camera Roll Item: index=" << index_;
  camera_roll_manager_->DownloadItem(metadata_);
  user_action_recorder_->RecordCameraRollDownloadAttempt();
  phone_hub_metrics::LogCameraRollContextMenuDownload(index_, GetMediaType());
}

phone_hub_metrics::CameraRollMediaType CameraRollThumbnail::GetMediaType() {
  return video_type_ ? phone_hub_metrics::CameraRollMediaType::kVideo
                     : phone_hub_metrics::CameraRollMediaType::kPhoto;
}

BEGIN_METADATA(CameraRollThumbnail)
END_METADATA

}  // namespace ash
