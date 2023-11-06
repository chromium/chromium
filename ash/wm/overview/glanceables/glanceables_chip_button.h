// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_GLANCEABLES_GLANCEABLES_CHIP_BUTTON_H_
#define ASH_WM_OVERVIEW_GLANCEABLES_GLANCEABLES_CHIP_BUTTON_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/metadata/view_factory.h"

namespace views {
class BoxLayout;
class ImageView;
class Label;
}  // namespace views

namespace ash {

class PillButton;

// A compact view of an app, displaying its icon, name, a brief description, and
// an optional call to action.
class GlanceablesChipButton : public views::Button,
                              public ui::SimpleMenuModel::Delegate {
 public:
  METADATA_HEADER(GlanceablesChipButton);

  // The delegate executes the actions when the chip is removed.
  class Delegate {
   public:
    virtual void RemoveChip(GlanceablesChipButton* chip) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  GlanceablesChipButton();
  GlanceablesChipButton(const GlanceablesChipButton&) = delete;
  GlanceablesChipButton& operator=(const GlanceablesChipButton&) = delete;
  ~GlanceablesChipButton() override;

  // Chip configuration methods.
  void SetIconImage(const ui::ImageModel& icon_image);
  void SetTitleText(const std::u16string& title);
  void SetSubtitleText(const std::u16string& subtitle);
  void SetActionButton(const std::u16string& label,
                       views::Button::PressedCallback action);

  void SetDelegate(Delegate* delegate);

  // views::Button:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  class RemovalChipMenuController;

  // The callback when the removal button or removal panel is pressed.
  void OnRemoveComponentPressed();

  // The components owned by the chip view.
  raw_ptr<views::BoxLayout> box_layout_ = nullptr;
  raw_ptr<views::ImageView> icon_ = nullptr;
  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> subtitle_ = nullptr;
  raw_ptr<PillButton> action_button_ = nullptr;

  raw_ptr<Delegate> delegate_ = nullptr;

  // The removal chip context menu controller.
  std::unique_ptr<RemovalChipMenuController> removal_chip_menu_controller_;
};

BEGIN_VIEW_BUILDER(/*no export*/, GlanceablesChipButton, views::Button)
VIEW_BUILDER_PROPERTY(const ui::ImageModel&, IconImage)
VIEW_BUILDER_PROPERTY(const std::u16string&, TitleText)
VIEW_BUILDER_PROPERTY(const std::u16string&, SubtitleText)
VIEW_BUILDER_PROPERTY(GlanceablesChipButton::Delegate*, Delegate)
VIEW_BUILDER_METHOD(SetActionButton,
                    const std::u16string&,
                    views::Button::PressedCallback)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/*no export*/, ash::GlanceablesChipButton)

#endif  // ASH_WM_OVERVIEW_GLANCEABLES_GLANCEABLES_CHIP_BUTTON_H_
