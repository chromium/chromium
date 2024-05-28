// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_SYMBOL_ITEM_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_SYMBOL_ITEM_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/picker/views/picker_item_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

// Picker item which contains just an symbol.
class ASH_EXPORT PickerSymbolItemView : public PickerItemView {
  METADATA_HEADER(PickerSymbolItemView, PickerItemView)

 public:
  PickerSymbolItemView(SelectItemCallback select_item_callback,
                       const std::u16string& symbol);
  PickerSymbolItemView(const PickerSymbolItemView&) = delete;
  PickerSymbolItemView& operator=(const PickerSymbolItemView&) = delete;
  ~PickerSymbolItemView() override;

 private:
  raw_ptr<views::Label> symbol_label_ = nullptr;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_SYMBOL_ITEM_VIEW_H_
