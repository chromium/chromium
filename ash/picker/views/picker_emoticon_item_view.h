// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_EMOTICON_ITEM_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_EMOTICON_ITEM_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/picker/views/picker_item_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

// Picker item which contains just an emoticon.
class ASH_EXPORT PickerEmoticonItemView : public PickerItemView {
  METADATA_HEADER(PickerEmoticonItemView, PickerItemView)

 public:
  PickerEmoticonItemView(SelectItemCallback select_item_callback,
                         const std::u16string& emoticon);
  PickerEmoticonItemView(const PickerEmoticonItemView&) = delete;
  PickerEmoticonItemView& operator=(const PickerEmoticonItemView&) = delete;
  ~PickerEmoticonItemView() override;

  std::u16string_view GetTextForTesting() const;

 private:
  raw_ptr<views::Label> emoticon_label_ = nullptr;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_EMOTICON_ITEM_VIEW_H_
