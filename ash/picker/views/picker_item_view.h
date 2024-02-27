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
  // Used to determine how the item looks and how the user can interact with it.
  enum class ItemState {
    // Normal state.
    kNormal,
    // Pseudo focused state. The item is painted as if it was focused to
    // indicate that it responds to certain user actions, e.g. it can be
    // selected if the user presses the enter key. Note that the item might not
    // have actual view focus (which generally stays on the Picker search field
    // to allow the user to easily type and modify their search query).
    kPseudoFocused,
  };

  using SelectItemCallback = base::RepeatingClosure;

  explicit PickerItemView(SelectItemCallback select_item_callback);
  PickerItemView(const PickerItemView&) = delete;
  PickerItemView& operator=(const PickerItemView&) = delete;
  ~PickerItemView() override;

  // views::Button:
  void PaintButtonContents(gfx::Canvas* canvas) override;

  void SelectItem();

  void SetCornerRadius(int corner_radius);

  ItemState GetItemState() const;
  void SetItemState(ItemState item_state);

 private:
  SelectItemCallback select_item_callback_;

  ItemState item_state_ = ItemState::kNormal;

  // Corner radius of the item background and highlight.
  int corner_radius_ = 0;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_ITEM_VIEW_H_
