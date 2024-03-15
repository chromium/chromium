// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_BIRCH_BIRCH_CHIP_BUTTON_H_
#define ASH_WM_OVERVIEW_BIRCH_BIRCH_CHIP_BUTTON_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/metadata/view_factory.h"

namespace views {
class FlexLayout;
class ImageView;
class Label;
}  // namespace views

namespace ash {

class BirchItem;
class PillButton;

// A compact view of an app, displaying its icon, name, a brief description, and
// an optional call to action.
class BirchChipButton : public views::Button,
                        public ui::SimpleMenuModel::Delegate {
  METADATA_HEADER(BirchChipButton, views::Button)

 public:
  // The delegate executes the actions when the chip is removed.
  class Delegate {
   public:
    virtual void RemoveChip(BirchChipButton* chip) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  BirchChipButton();
  BirchChipButton(const BirchChipButton&) = delete;
  BirchChipButton& operator=(const BirchChipButton&) = delete;
  ~BirchChipButton() override;

  // Chip configuration methods.
  void Init(BirchItem* item);

  template <typename T>
  T* SetAddon(std::unique_ptr<T> addon_view) {
    T* ptr = addon_view.get();
    SetAddonInternal(std::move(addon_view));
    return ptr;
  }

  void SetDelegate(Delegate* delegate);

  // views::Button:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  class RemovalChipMenuController;

  void SetAddonInternal(std::unique_ptr<views::View> addon_view);

  // The callback when the removal button or removal panel is pressed.
  void OnRemoveComponentPressed();

  // Sets the item icon.
  void SetIconImage(const ui::ImageModel& icon_image);

  // The removal chip context menu controller.
  std::unique_ptr<RemovalChipMenuController> removal_chip_menu_controller_;

  // The source of the chip.
  raw_ptr<BirchItem> item_ = nullptr;

  // The components owned by the chip view.
  raw_ptr<views::FlexLayout> flex_layout_ = nullptr;
  raw_ptr<views::ImageView> icon_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> subtitle_ = nullptr;
  raw_ptr<views::View> addon_view_ = nullptr;

  raw_ptr<Delegate> delegate_ = nullptr;

  base::WeakPtrFactory<BirchChipButton> weak_factory_{this};
};

BEGIN_VIEW_BUILDER(/*no export*/, BirchChipButton, views::Button)
VIEW_BUILDER_METHOD(Init, BirchItem*)
VIEW_BUILDER_VIEW_TYPE_PROPERTY(views::View, Addon)
VIEW_BUILDER_PROPERTY(BirchChipButton::Delegate*, Delegate)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/*no export*/, ash::BirchChipButton)

#endif  // ASH_WM_OVERVIEW_BIRCH_BIRCH_CHIP_BUTTON_H_
