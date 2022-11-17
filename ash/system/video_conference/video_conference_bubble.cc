// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/video_conference_bubble.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ui/views/controls/image_view.h"

namespace ash {

VideoConferenceBubbleView::VideoConferenceBubbleView(
    const InitParams& init_params)
    : TrayBubbleView(init_params) {
  // TODO(b/253088232): Added an icon so that the bubble can show. Will remove
  // this with the newly created class VideoConferenceBubbleView.
  auto icon = std::make_unique<views::ImageView>();
  icon->SetImage(gfx::CreateVectorIcon(
      kPrivacyIndicatorsMicrophoneIcon,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary)));
  AddChildView(std::move(icon));
}

}  // namespace ash
