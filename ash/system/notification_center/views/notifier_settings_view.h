// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_NOTIFIER_SETTINGS_VIEW_H_
#define ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_NOTIFIER_SETTINGS_VIEW_H_

#include <memory>
#include <set>

#include "ash/ash_export.h"
#include "ash/public/cpp/notifier_settings_observer.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
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
                                        public NotifierSettingsObserver {
  METADATA_HEADER(NotifierSettingsView, views::View)

 public:
  NotifierSettingsView();

  NotifierSettingsView(const NotifierSettingsView&) = delete;
  NotifierSettingsView& operator=(const NotifierSettingsView&) = delete;

  ~NotifierSettingsView() override;

  bool IsScrollable();

  void SetQuietModeState(bool is_quiet_mode);

  // NotifierSettingsObserver:
  void OnNotifiersUpdated(
      const std::vector<NotifierMetadata>& notifiers) override;
  void OnNotifierIconUpdated(const message_center::NotifierId& notifier_id,
                             const gfx::ImageSkia& icon) override;

  views::ScrollView* get_scroller_view_for_test() { return scroller_; }
  views::Label* get_notification_settings_lable_for_test() {
    return notification_settings_label_;
  }
  views::ImageView* get_quiet_mode_icon_view_for_test() {
    return quiet_mode_icon_;
  }
  views::ToggleButton* get_quiet_mode_toggle_for_test() {
    return quiet_mode_toggle_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(NotifierSettingsViewTest, TestLearnMoreButton);
  FRIEND_TEST_ALL_PREFIXES(NotifierSettingsViewTest, TestEmptyNotifierView);

  class ASH_EXPORT NotifierButton : public views::Button {
    METADATA_HEADER(NotifierButton, views::Button)

   public:
    explicit NotifierButton(const NotifierMetadata& notifier);

    NotifierButton(const NotifierButton&) = delete;
    NotifierButton& operator=(const NotifierButton&) = delete;

    ~NotifierButton() override;

    void UpdateIconImage(const gfx::ImageSkia& icon);
    void SetChecked(bool checked);
    bool GetChecked() const;
    const message_center::NotifierId& notifier_id() const {
      return notifier_id_;
    }

   private:
    // views::Button:
    void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

    // Helper function to reset the layout when the view has substantially
    // changed.
    void GridChanged();

    message_center::NotifierId notifier_id_;
    raw_ptr<views::ImageView> icon_view_ = nullptr;
    raw_ptr<views::Label> name_view_ = nullptr;
    raw_ptr<views::Checkbox> checkbox_ = nullptr;
  };

  // Overridden from views::View:
  void Layout(PassKey) override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;

  // Utility function that creates a row containing a toggle button, label,
  // and icon. All passed in views will be added to the returned row view.
  std::unique_ptr<views::View> CreateToggleButtonRow(
      std::unique_ptr<views::ImageView> icon,
      std::unique_ptr<views::Label> label,
      std::unique_ptr<views::ToggleButton> toggle_button);

  void AppBadgingTogglePressed();
  void QuietModeTogglePressed();
  void NotifierButtonPressed(NotifierButton* button);

  raw_ptr<views::ToggleButton> app_badging_toggle_ = nullptr;
  raw_ptr<views::ImageView> quiet_mode_icon_ = nullptr;
  raw_ptr<views::ToggleButton> quiet_mode_toggle_ = nullptr;
  raw_ptr<views::View> header_view_ = nullptr;
  raw_ptr<views::Label> notification_settings_label_ = nullptr;
  raw_ptr<views::Label> top_label_ = nullptr;
  raw_ptr<views::ScrollBar> scroll_bar_ = nullptr;
  raw_ptr<views::ScrollView> scroller_ = nullptr;
  raw_ptr<views::View> no_notifiers_view_ = nullptr;
  // TODO(crbug/1194632): remove |buttons_| and all related views.
  std::set<raw_ptr<NotifierButton, SetExperimental>> buttons_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_NOTIFICATION_CENTER_VIEWS_NOTIFIER_SETTINGS_VIEW_H_
