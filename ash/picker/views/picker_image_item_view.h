// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_IMAGE_ITEM_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_IMAGE_ITEM_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace views {
class ImageView;
}  // namespace views

namespace ash {

// Picker item which contains just an image.
class ASH_EXPORT PickerImageItemView : public views::Button {
  METADATA_HEADER(PickerImageItemView, views::Button)

 public:
  PickerImageItemView(views::Button::PressedCallback callback,
                      std::unique_ptr<views::ImageView> image);
  PickerImageItemView(const PickerImageItemView&) = delete;
  PickerImageItemView& operator=(const PickerImageItemView&) = delete;
  ~PickerImageItemView() override;

  void SetImageSizeFromWidth(int width);

 private:
  raw_ptr<views::ImageView> image_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_IMAGE_ITEM_VIEW_H_
