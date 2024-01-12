// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_ITEM_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_ITEM_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace ash {

// View for a Picker list item. Has text and an icon.
class ASH_EXPORT PickerItemView : public views::Button {
  METADATA_HEADER(PickerItemView, views::Button)

 public:
  PickerItemView(views::Button::PressedCallback callback,
                 const std::u16string& text);
  PickerItemView(const PickerItemView&) = delete;
  PickerItemView& operator=(const PickerItemView&) = delete;
  ~PickerItemView() override;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_ITEM_VIEW_H_
