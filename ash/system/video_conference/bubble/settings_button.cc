// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/settings_button.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_id.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash::video_conference {

namespace {

constexpr gfx::Size kIconSize{20, 20};

}  // namespace

SettingsButton::SettingsButton() {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->SetOrientation(views::BoxLayout::Orientation::kHorizontal);

  AddChildView(views::Builder<views::ImageView>()
                   .SetImage(ui::ImageModel::FromVectorIcon(
                       kSystemMenuSettingsIcon, cros_tokens::kCrosSysOnSurface))
                   .SetImageSize(kIconSize)
                   .Build());
  AddChildView(views::Builder<views::ImageView>()
                   .SetImage(ui::ImageModel::FromVectorIcon(
                       kDropDownArrowIcon, cros_tokens::kCrosSysOnSurface))
                   .SetImageSize(kIconSize)
                   .Build());
}

BEGIN_METADATA(SettingsButton)
END_METADATA

}  // namespace ash::video_conference
