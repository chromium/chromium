// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_BUTTON_BAR_H_
#define ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_BUTTON_BAR_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/view.h"

namespace views {
class Label;
class ToggleImageButton;
}  // namespace views

namespace message_center {
class MessageCenter;
}  // namespace message_center

namespace ash {

class MessageCenterView;

// MessageCenterButtonBar is the class that shows the content outside the main
// notification area - the label and the buttons.
class MessageCenterButtonBar : public views::View,
                               public views::ButtonListener,
                               public ui::ImplicitAnimationObserver {
 public:
  MessageCenterButtonBar(MessageCenterView* message_center_view,
                         message_center::MessageCenter* message_center,
                         bool locked);
  ~MessageCenterButtonBar() override;

  void SetQuietModeState(bool is_quiet_mode);

  // Overridden from views::View:
  void ChildVisibilityChanged(views::View* child) override;
  void Layout() override;
  gfx::Size CalculatePreferredSize() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Overridden from ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // Enables or disables all of the buttons in the center.  This is used to
  // prevent user clicks during the close-all animation.
  void SetSettingsAndQuietModeButtonsEnabled(bool enabled);

  // Sometimes we shouldn't see the close-all button.
  void SetCloseAllButtonEnabled(bool enabled);

  ASH_EXPORT views::Button* GetCloseAllButtonForTest() const;
  ASH_EXPORT views::Button* GetSettingsButtonForTest() const;
  ASH_EXPORT views::Button* GetQuietModeButtonForTest() const;
  ASH_EXPORT views::Button* GetCollapseButtonForTest() const;

  // Sometimes we shouldn't see the back arrow (not in settings).
  void SetBackArrowVisible(bool visible);

  // Updates notification label and buttons for state specified by |locked|.
  void SetIsLocked(bool locked);

 private:
  MessageCenterView* message_center_view() const {
    return message_center_view_;
  }
  message_center::MessageCenter* message_center() const {
    return message_center_;
  }

  // Returns title for state specified by |message_center_visible|.
  base::string16 GetTitle(bool message_center_visible) const;

  // Updates notification label for state specified by |message_center_visible|.
  void UpdateLabel(bool message_center_visible);

  void SetButtonsVisible(bool locked);

  MessageCenterView* message_center_view_;
  message_center::MessageCenter* message_center_;

  // Sub-views of the button bar.
  views::Label* notification_label_;
  views::View* button_container_;
  views::ToggleImageButton* close_all_button_;
  // A view of a separator between |close_all_button_| and |quiet_mode_button_|.
  views::Separator* separator_1_;
  views::ToggleImageButton* quiet_mode_button_;
  // A view of a separator between |quiet_mode_button_| and |settings_button_|.
  views::Separator* separator_2_;
  views::ToggleImageButton* settings_button_;
  views::ToggleImageButton* collapse_button_;

  bool collapse_button_visible_ = false;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterButtonBar);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_MESSAGE_CENTER_BUTTON_BAR_H_
