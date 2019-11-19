// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_STATUS_AREA_WIDGET_H_
#define ASH_SYSTEM_STATUS_AREA_WIDGET_H_

#include "ash/ash_export.h"
#include "ash/login_status.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/macros.h"
#include "ui/views/widget/widget.h"

namespace aura {
class Window;
}

namespace ash {
class ImeMenuTray;
class LogoutButtonTray;
class StatusAreaOverflowButtonTray;
class OverviewButtonTray;
class DictationButtonTray;
class PaletteTray;
class SelectToSpeakTray;
class Shelf;
class StatusAreaWidgetDelegate;
class UnifiedSystemTray;
class TrayBackgroundView;
class VirtualKeyboardTray;

// Widget showing the system tray, notification tray, and other tray views in
// the bottom-right of the screen. Exists separately from ShelfView/ShelfWidget
// so that it can be shown in cases where the rest of the shelf is hidden (e.g.
// on secondary monitors at the login screen).
class ASH_EXPORT StatusAreaWidget : public views::Widget {
 public:
  // Whether the status area is collapsed or expanded. Currently, this is only
  // applicable in in-app tablet mode. Otherwise the state is NOT_COLLAPSIBLE.
  enum class CollapseState { NOT_COLLAPSIBLE, COLLAPSED, EXPANDED };

  StatusAreaWidget(aura::Window* status_container, Shelf* shelf);
  ~StatusAreaWidget() override;

  // Creates the child tray views, initializes them, and shows the widget. Not
  // part of the constructor because some child views call back into this object
  // during construction.
  void Initialize();

  // Update the alignment of the widget and tray views.
  void UpdateAfterShelfAlignmentChange();

  // Called by the client when the login status changes. Caches login_status
  // and calls UpdateAfterLoginStatusChange for the system tray and the web
  // notification tray.
  void UpdateAfterLoginStatusChange(LoginStatus login_status);

  // Updates the collapse state of the status area after the state of the shelf
  // changes.
  void UpdateCollapseState();

  // Sets system tray visibility. Shows or hides widget if needed.
  void SetSystemTrayVisibility(bool visible);

  // Get the tray button that the system tray bubble and the notification center
  // bubble will be anchored. Usually |unified_system_tray_|, but when the
  // overview button is visible (i.e. tablet mode is enabled), it returns
  // |overview_button_tray_|.
  TrayBackgroundView* GetSystemTrayAnchor() const;

  StatusAreaWidgetDelegate* status_area_widget_delegate() {
    return status_area_widget_delegate_;
  }
  UnifiedSystemTray* unified_system_tray() {
    return unified_system_tray_.get();
  }
  DictationButtonTray* dictation_button_tray() {
    return dictation_button_tray_.get();
  }
  StatusAreaOverflowButtonTray* overflow_button_tray() {
    return overflow_button_tray_.get();
  }
  OverviewButtonTray* overview_button_tray() {
    return overview_button_tray_.get();
  }
  PaletteTray* palette_tray() { return palette_tray_.get(); }
  ImeMenuTray* ime_menu_tray() { return ime_menu_tray_.get(); }
  SelectToSpeakTray* select_to_speak_tray() {
    return select_to_speak_tray_.get();
  }

  Shelf* shelf() { return shelf_; }

  LoginStatus login_status() const { return login_status_; }

  // Returns true if the shelf should be visible. This is used when the
  // shelf is configured to auto-hide and test if the shelf should force
  // the shelf to remain visible.
  bool ShouldShowShelf() const;

  // True if any message bubble is shown.
  bool IsMessageBubbleShown() const;

  // Notifies child trays, and the |status_area_widget_delegate_| to schedule a
  // paint.
  void SchedulePaint();

  // Overridden from views::Widget:
  const ui::NativeTheme* GetNativeTheme() const override;
  bool OnNativeWidgetActivationChanged(bool active) override;

  // TODO(jamescook): Introduce a test API instead of these methods.
  LogoutButtonTray* logout_button_tray_for_testing() {
    return logout_button_tray_.get();
  }
  VirtualKeyboardTray* virtual_keyboard_tray_for_testing() {
    return virtual_keyboard_tray_.get();
  }

  CollapseState collapse_state() const { return collapse_state_; }

 private:
  friend class StatusAreaWidgetTestApi;

  // views::Widget:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  StatusAreaWidgetDelegate* status_area_widget_delegate_;

  std::unique_ptr<StatusAreaOverflowButtonTray> overflow_button_tray_;
  std::unique_ptr<OverviewButtonTray> overview_button_tray_;
  std::unique_ptr<DictationButtonTray> dictation_button_tray_;
  std::unique_ptr<UnifiedSystemTray> unified_system_tray_;
  std::unique_ptr<LogoutButtonTray> logout_button_tray_;
  std::unique_ptr<PaletteTray> palette_tray_;
  std::unique_ptr<VirtualKeyboardTray> virtual_keyboard_tray_;
  std::unique_ptr<ImeMenuTray> ime_menu_tray_;
  std::unique_ptr<SelectToSpeakTray> select_to_speak_tray_;

  LoginStatus login_status_ = LoginStatus::NOT_LOGGED_IN;

  CollapseState collapse_state_ = CollapseState::NOT_COLLAPSIBLE;

  Shelf* shelf_;

  DISALLOW_COPY_AND_ASSIGN(StatusAreaWidget);
};

}  // namespace ash

#endif  // ASH_SYSTEM_STATUS_AREA_WIDGET_H_
