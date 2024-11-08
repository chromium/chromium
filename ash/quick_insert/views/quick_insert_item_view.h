// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_ITEM_VIEW_H_
#define ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

enum class QuickInsertActionType;
class PickerSubmenuController;

// View for a Quick Insert item which can be selected.
class ASH_EXPORT QuickInsertItemView : public views::Button {
  METADATA_HEADER(QuickInsertItemView, views::Button)

 public:
  // Used to determine how the item looks and how the user can interact with it.
  enum class ItemState {
    // Normal state.
    kNormal,
    // Pseudo focused state. The item is painted as if it was focused to
    // indicate that it responds to certain user actions, e.g. it can be
    // selected if the user presses the enter key. Note that the item might not
    // have actual view focus (which generally stays on the Quick Insert search
    // field to allow the user to easily type and modify their search query).
    kPseudoFocused,
  };

  // Used to determine the style of focus indicator used for the item.
  enum class FocusIndicatorStyle {
    // Indicate focus using a rounded rectangular ring around the item.
    kFocusRing,
    // Similar to `kFocusRing`, but clips the QuickInsertItemView with a 1dp
    // border
    // as well as adding a rounded rectangular ring.
    kFocusRingWithInsetGap,
    // Indicate focus using a vertical bar with half rounded corners at the left
    // edge of the item.
    kFocusBar,
  };

  using SelectItemCallback = base::RepeatingClosure;

  explicit QuickInsertItemView(SelectItemCallback select_item_callback,
                               FocusIndicatorStyle focus_indicator_style =
                                   FocusIndicatorStyle::kFocusRing);
  QuickInsertItemView(const QuickInsertItemView&) = delete;
  QuickInsertItemView& operator=(const QuickInsertItemView&) = delete;
  ~QuickInsertItemView() override;

  // views::Button:
  void StateChanged(ButtonState old_state) override;
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;

  void SelectItem();

  void SetCornerRadius(int corner_radius);

  PickerSubmenuController* GetSubmenuController();
  void SetSubmenuController(PickerSubmenuController* submenu_controller);

  ItemState GetItemState() const;
  virtual void SetItemState(ItemState item_state);

 private:
  void UpdateClipPathForFocusRingWithInsetGap();

  // Updates the background radius and color based on the item state.
  void UpdateBackground();

  SelectItemCallback select_item_callback_;

  ItemState item_state_ = ItemState::kNormal;

  FocusIndicatorStyle focus_indicator_style_ = FocusIndicatorStyle::kFocusRing;

  // Corner radius of the item background and highlight.
  int corner_radius_ = 0;

  raw_ptr<PickerSubmenuController> submenu_controller_ = nullptr;
};

BEGIN_VIEW_BUILDER(ASH_EXPORT, QuickInsertItemView, views::Button)
VIEW_BUILDER_PROPERTY(PickerSubmenuController*, SubmenuController)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::QuickInsertItemView)

#endif  // ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_ITEM_VIEW_H_
