// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/camera_roll_thumbnail.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "base/bind.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"

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
    const chromeos::phonehub::CameraRollItem& item)
    : views::ImageButton(
          base::BindRepeating(&CameraRollThumbnail::ButtonPressed,
                              base::Unretained(this))),
      key_(item.metadata().key()) {
  // True if mime type string starts with "video/"
  video_type_ = (item.metadata().mime_type().find("video/") == 0);

  SetFocusBehavior(FocusBehavior::ALWAYS);
  views::FocusRing::Get(this)->SetColor(
      AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kFocusRingColor));
  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(), kCameraRollThumbnailBorderRadius);

  SetImageHorizontalAlignment(ALIGN_CENTER);
  SetImageVerticalAlignment(ALIGN_MIDDLE);
  SetImage(STATE_NORMAL, item.thumbnail().ToImageSkia());

  SetClipPath(SkPath::RRect(SkRRect::MakeRectXY(
      SkRect::Make(SkIRect::MakeWH(kCameraRollThumbnailBorderSize.width(),
                                   kCameraRollThumbnailBorderSize.height())),
      SkIntToScalar(kCameraRollThumbnailBorderRadius),
      SkIntToScalar(kCameraRollThumbnailBorderRadius))));
}

CameraRollThumbnail::~CameraRollThumbnail() = default;

void CameraRollThumbnail::PaintButtonContents(gfx::Canvas* canvas) {
  auto* color_provider = AshColorProvider::Get();
  canvas->DrawColor(color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive));

  views::ImageButton::PaintButtonContents(canvas);

  if (video_type_) {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(color_provider->GetBaseLayerColor(
        AshColorProvider::BaseLayerType::kTransparent80));
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

const char* CameraRollThumbnail::GetClassName() const {
  return "CameraRollThumbnail";
}

void CameraRollThumbnail::ButtonPressed() {}

}  // namespace ash
