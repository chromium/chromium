// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_EMOTICON_ITEM_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_EMOTICON_ITEM_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

// Picker item which contains just an emoticon.
class ASH_EXPORT PickerEmoticonItemView : public views::Button {
  METADATA_HEADER(PickerEmoticonItemView, views::Button)

 public:
  PickerEmoticonItemView(views::Button::PressedCallback callback,
                         const std::u16string& emoticon);
  PickerEmoticonItemView(const PickerEmoticonItemView&) = delete;
  PickerEmoticonItemView& operator=(const PickerEmoticonItemView&) = delete;
  ~PickerEmoticonItemView() override;

 private:
  raw_ptr<views::Label> emoticon_label_ = nullptr;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_EMOTICON_ITEM_VIEW_H_
