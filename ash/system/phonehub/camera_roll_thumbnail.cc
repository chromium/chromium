// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/camera_roll_thumbnail.h"

#include "base/bind.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

CameraRollThumbnail::CameraRollThumbnail(
    const chromeos::phonehub::CameraRollItem& item)
    : views::Button(base::BindRepeating(&CameraRollThumbnail::ButtonPressed,
                                        base::Unretained(this))),
      key_(item.metadata().key()) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  auto* thumbnail = AddChildView(std::make_unique<views::ImageView>());
  thumbnail->SetImage(item.thumbnail().ToImageSkia());
}

CameraRollThumbnail::~CameraRollThumbnail() = default;

const char* CameraRollThumbnail::GetClassName() const {
  return "CameraRollThumbnail";
}

void CameraRollThumbnail::ButtonPressed() {}

}  // namespace ash
