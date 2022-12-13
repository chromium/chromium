// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/return_to_app_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ui/base/models/image_model.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"

namespace ash::video_conference {

namespace {

const int kBorderInsetDimension = 10;
const int kBackgroundRoundedRectRadius = 10;

}  // namespace

// TODO(b/253274599, b/253274147, b/253272945) Implement actual "return to app"
// button functionality. This is a temporary placeholder for facilitating VC
// bubble development.
ReturnToAppButton::ReturnToAppButton() {
  SetID(BubbleViewID::kReturnToApp);

  views::FlexLayout* layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kHorizontal);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

  auto camera = std::make_unique<views::ImageView>();
  camera->SetImage(ui::ImageModel::FromVectorIcon(kPrivacyIndicatorsCameraIcon,
                                                  kColorAshIconColorPrimary));
  AddChildView(std::move(camera));

  auto microphone = std::make_unique<views::ImageView>();
  microphone->SetImage(ui::ImageModel::FromVectorIcon(
      kPrivacyIndicatorsMicrophoneIcon, kColorAshIconColorPrimary));
  AddChildView(std::move(microphone));

  AddChildView(std::make_unique<views::Label>(u"Meet"));

  SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(kBorderInsetDimension, kBorderInsetDimension)));
  SetBackground(views::CreateRoundedRectBackground(
      gfx::kGoogleBlue800, kBackgroundRoundedRectRadius));
}

}  // namespace ash::video_conference