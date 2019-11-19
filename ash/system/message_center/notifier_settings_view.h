// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_NOTIFIER_SETTINGS_VIEW_H_
#define ASH_SYSTEM_MESSAGE_CENTER_NOTIFIER_SETTINGS_VIEW_H_

#include <memory>
#include <set>

#include "ash/ash_export.h"
#include "ash/public/cpp/notifier_settings_observer.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

namespace views {
class Label;
class ScrollBar;
class ToggleButton;
}  // namespace views

namespace ash {

// A class to show the list of notifier extensions / URL patterns and allow
// users to customize the settings.
class ASH_EXPORT NotifierSettingsView : public views::View,
                                        public views::ButtonListener,
                                        public NotifierSettingsObserver {
 public:
  explicit NotifierSettingsView();
  ~NotifierSettingsView() override;

  bool IsScrollable();

  void SetQuietModeState(bool is_quiet_mode);

  // NotifierSettingsObserver:
  void OnNotifiersUpdated(
      const std::vector<NotifierMetadata>& notifiers) override;
  void OnNotifierIconUpdated(const message_center::NotifierId& notifier_id,
                             const gfx::ImageSkia& icon) override;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  const char* GetClassName() const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(NotifierSettingsViewTest, TestLearnMoreButton);
  FRIEND_TEST_ALL_PREFIXES(NotifierSettingsViewTest, TestEmptyNotifierView);

  class ASH_EXPORT NotifierButton : public views::Button,
                                    public views::ButtonListener {
   public:
    NotifierButton(const NotifierMetadata& notifier,
                   views::ButtonListener* listener);
    ~NotifierButton() override;

    void UpdateIconImage(const gfx::ImageSkia& icon);
    void SetChecked(bool checked);
    bool GetChecked() const;
    const message_center::NotifierId& notifier_id() const {
      return notifier_id_;
    }
    // views::Button:
    const char* GetClassName() const override;

   private:
    // Overridden from views::ButtonListener:
    void ButtonPressed(views::Button* button, const ui::Event& event) override;
    void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

    // Helper function to reset the layout when the view has substantially
    // changed.
    void GridChanged();

    message_center::NotifierId notifier_id_;
    views::ImageView* icon_view_ = nullptr;
    views::Label* name_view_ = nullptr;
    views::Checkbox* checkbox_ = nullptr;

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

  views::ImageView* quiet_mode_icon_ = nullptr;
  views::ToggleButton* quiet_mode_toggle_ = nullptr;
  views::View* header_view_ = nullptr;
  views::Label* top_label_ = nullptr;
  views::ScrollBar* scroll_bar_ = nullptr;
  views::ScrollView* scroller_ = nullptr;
  views::View* no_notifiers_view_ = nullptr;
  std::set<NotifierButton*> buttons_;

  DISALLOW_COPY_AND_ASSIGN(NotifierSettingsView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_NOTIFIER_SETTINGS_VIEW_H_
