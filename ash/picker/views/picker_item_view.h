// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_ITEM_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace ash {

// View for a Picker item which can be selected.
class ASH_EXPORT PickerItemView : public views::Button {
  METADATA_HEADER(PickerItemView, views::Button)

 public:
  using SelectItemCallback = base::RepeatingClosure;

  explicit PickerItemView(SelectItemCallback select_item_callback);
  PickerItemView(const PickerItemView&) = delete;
  PickerItemView& operator=(const PickerItemView&) = delete;
  ~PickerItemView() override;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_ITEM_VIEW_H_
