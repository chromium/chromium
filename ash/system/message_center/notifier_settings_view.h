// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_NOTIFIER_SETTINGS_VIEW_H_
#define ASH_SYSTEM_MESSAGE_CENTER_NOTIFIER_SETTINGS_VIEW_H_

#include <memory>
#include <set>

#include "ash/ash_export.h"
#include "ash/system/message_center/message_center_controller.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

namespace views {
class Label;
class ToggleButton;
}  // namespace views

namespace ash {

// A class to show the list of notifier extensions / URL patterns and allow
// users to customize the settings.
class ASH_EXPORT NotifierSettingsView
    : public views::View,
      public views::ButtonListener,
      public MessageCenterController::NotifierSettingsListener {
 public:
  explicit NotifierSettingsView();
  ~NotifierSettingsView() override;

  bool IsScrollable();

  void SetQuietModeState(bool is_quiet_mode);

  // NotifierSettingsListener:
  void OnNotifierListUpdated(
      const std::vector<mojom::NotifierUiDataPtr>& ui_data) override;
  void UpdateNotifierIcon(const message_center::NotifierId& notifier_id,
                          const gfx::ImageSkia& icon) override;

  // Overridden from views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(NotifierSettingsViewTest, TestLearnMoreButton);
  FRIEND_TEST_ALL_PREFIXES(NotifierSettingsViewTest, TestEmptyNotifierView);

  class ASH_EXPORT NotifierButton : public views::Button,
                                    public views::ButtonListener {
   public:
    NotifierButton(const mojom::NotifierUiData& notifier_ui_data,
                   views::ButtonListener* listener);
    ~NotifierButton() override;

    void UpdateIconImage(const gfx::ImageSkia& icon);
    void SetChecked(bool checked);
    bool checked() const;
    const message_center::NotifierId& notifier_id() const {
      return notifier_id_;
    }

   private:
    // Overridden from views::ButtonListener:
    void ButtonPressed(views::Button* button, const ui::Event& event) override;
    void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

    // Helper function to reset the layout when the view has substantially
    // changed.
    void GridChanged();

    message_center::NotifierId notifier_id_;
    views::ImageView* icon_view_;
    views::Label* name_view_;
    views::Checkbox* checkbox_;

    DISALLOW_COPY_AND_ASSIGN(NotifierButton);
  };

  // Overridden from views::View:
  void Layout() override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size CalculatePreferredSize() const override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  views::ImageButton* title_arrow_;
  views::ImageView* quiet_mode_icon_;
  views::ToggleButton* quiet_mode_toggle_;
  views::View* header_view_;
  views::Label* top_label_;
  views::ScrollView* scroller_;
  views::View* no_notifiers_view_;
  std::set<NotifierButton*> buttons_;

  DISALLOW_COPY_AND_ASSIGN(NotifierSettingsView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_NOTIFIER_SETTINGS_VIEW_H_
