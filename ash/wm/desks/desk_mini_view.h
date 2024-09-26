// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_MINI_VIEW_H_
#define ASH_WM_DESKS_DESK_MINI_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/animation/animation_abort_handle.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_observer.h"

namespace ash {

class DeskActionContextMenu;
class DeskActionView;
class DeskBarViewBase;
class DeskNameView;
class DeskPreviewView;
class DeskProfilesButton;
class WindowOcclusionCalculator;

// A view that acts as a mini representation (a.k.a. desk thumbnail) of a
// virtual desk in the desk bar view when overview mode is active. This view
// shows a preview of the contents of the associated desk, its title, and
// supports desk activation and removal.
class ASH_EXPORT DeskMiniView : public views::View,
                                public Desk::Observer,
                                public views::TextfieldController,
                                public views::ViewObserver {
  METADATA_HEADER(DeskMiniView, views::View)

 public:
  // Returns the width of the desk preview based on its |preview_height| and the
  // aspect ratio of the root window taken from |root_window_size|.
  static int GetPreviewWidth(const gfx::Size& root_window_size,
                             int preview_height);

  // The desk preview bounds are proportional to the bounds of the display on
  // which it resides.
  static gfx::Rect GetDeskPreviewBounds(aura::Window* root_window);

  DeskMiniView(
      DeskBarViewBase* owner_bar,
      aura::Window* root_window,
      Desk* desk,
      base::WeakPtr<WindowOcclusionCalculator> window_occlusion_calculator);

  DeskMiniView(const DeskMiniView&) = delete;
  DeskMiniView& operator=(const DeskMiniView&) = delete;

  ~DeskMiniView() override;

  aura::Window* root_window() { return root_window_; }

  const Desk* desk() const { return desk_; }
  Desk* desk() { return desk_; }

  DeskNameView* desk_name_view() { return desk_name_view_; }

  const DeskActionView* desk_action_view() const { return desk_action_view_; }
  DeskActionView* desk_action_view() { return desk_action_view_; }
  DeskActionContextMenu* context_menu() { return context_menu_.get(); }

  DeskProfilesButton* desk_profiles_button() { return desk_profile_button_; }

  DeskBarViewBase* owner_bar() { return owner_bar_; }
  const DeskBarViewBase* owner_bar() const { return owner_bar_; }
  const DeskPreviewView* desk_preview() const { return desk_preview_; }
  DeskPreviewView* desk_preview() { return desk_preview_; }

  bool is_animating_to_remove() const { return is_animating_to_remove_; }
  void set_is_animating_to_remove(bool value) {
    is_animating_to_remove_ = value;
  }

  // Sets the animation abort handle. Please note, it will abort the existing
  // animation first (if there is one) when a new one comes.
  void set_animation_abort_handle(
      std::unique_ptr<views::AnimationAbortHandle> animation_abort_handle) {
    animation_abort_handle_ = std::move(animation_abort_handle);
  }

  gfx::Rect GetPreviewBoundsInScreen() const;

  // Returns the associated desk's container window on the display this
  // mini_view resides on.
  aura::Window* GetDeskContainer() const;

  // Returns true if the desk's name is being modified (i.e. the DeskNameView
  // has the focus).
  bool IsDeskNameBeingModified() const;

  // Updates the visibility state of the desk buttons depending on whether this
  // view is mouse hovered, or if switch access is enabled.
  void UpdateDeskButtonVisibility();

  // Gesture tapping may affect the visibility of the desk buttons. There's only
  // one mini_view that shows the desk buttons on long press at any time.
  // This is useful for touch-only UIs.
  void OnWidgetGestureTap(const gfx::Rect& screen_rect, bool is_long_gesture);

  // Returns the expected focus color of `DeskPreviewView` based on the
  // activation state of the corresponding desk and whether the saved desk
  // library is visible.
  std::optional<ui::ColorId> GetFocusColor() const;

  // Updates the focus color of `DeskPreviewView`.
  void UpdateFocusColor();

  // Gets the preview border's insets.
  gfx::Insets GetPreviewBorderInsets() const;

  bool IsPointOnMiniView(const gfx::Point& screen_location) const;

  // Hides the `desk_action_view_` and opens `context_menu_`. Called when
  // `desk_preview_` is right-clicked or long-pressed. `source` is the type of
  // action that caused the context menu to be opened (e.g. long press versus
  // mouse click), and is provided to the context menu runner when the menu is
  // open in `DeskActionContextMenu::ShowContextMenuForViewImpl` so that it can
  // further evaluate menu positioning. This ends up doing nothing in particular
  // in the case of the `DeskActionContextMenu` because we use a
  // `views::MenuRunner::FIXED_ANCHOR` run type parameter, but the
  // `MenuRunner::RunMenuAt` function still requires this parameter, so we pass
  // it down to the function through this parameter.
  void OpenContextMenu(ui::MenuSourceType source);

  // Closes context menu on this mini view if one exists.
  void MaybeCloseContextMenu();

  // Invoked when the user has clicked a desk close button.
  void OnRemovingDesk(DeskCloseType close_type);

  // Notifies the mini-view that the preview or profile button is about to
  // request focus from a reverse tab traversal so that it can show and focus
  // the desk action view first if it was not already focused.
  void OnPreviewOrProfileAboutToBeFocusedByReverseTab();

  // views::View:
  void Layout(PassKey) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnThemeChanged() override;

  // Desk::Observer:
  void OnContentChanged() override;
  void OnDeskDestroyed(const Desk* desk) override;
  void OnDeskNameChanged(const std::u16string& new_name) override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;
  bool HandleMouseEvent(views::Textfield* sender,
                        const ui::MouseEvent& mouse_event) override;

  // views::ViewObserver:
  void OnViewFocused(views::View* observed_view) override;
  void OnViewBlurred(views::View* observed_view) override;

 private:
  friend class DesksTestApi;

  // Callback for when `context_menu_` is closed. Makes `desk_action_view_`
  // visible.
  void OnContextMenuClosed();

  // Callback for when a user selects a lacros profile from `context_menu_`.
  void OnSetLacrosProfileId(uint64_t lacros_profile_id);

  void OnDeskPreviewPressed();

  // Callbacks for when a user selects the save desk options in the context
  // menu.
  void OnSaveDeskAsTemplateButtonPressed();
  void OnSaveDeskForLaterButtonPressed();

  // Layout |desk_name_view_| given the current bounds of the desk preview.
  void LayoutDeskNameView(const gfx::Rect& preview_bounds);

  void UpdateAccessibleName();

  const raw_ptr<DeskBarViewBase> owner_bar_;

  // The root window on which this mini_view is created.
  const raw_ptr<aura::Window> root_window_;

  // The associated desk. This can become null if the desk is deleted before the
  // mini view is done. Desk deletion is monitored by `OnDeskDestroyed`.
  raw_ptr<Desk> desk_;  // Not owned.

  // The view that shows a preview of the desk contents.
  raw_ptr<DeskPreviewView> desk_preview_ = nullptr;

  // The view that shows what profile the desk belongs to.
  raw_ptr<DeskProfilesButton> desk_profile_button_ = nullptr;

  // The editable desk name.
  raw_ptr<DeskNameView> desk_name_view_ = nullptr;

  // Stores the hover interface for desk actions.
  raw_ptr<DeskActionView> desk_action_view_ = nullptr;

  // The context menu that appears when `desk_preview_` is right-clicked or
  // long-pressed.
  std::unique_ptr<DeskActionContextMenu> context_menu_;

  // The view containing the desk shortcut icons and labels displaying the
  // shortcut to activate the desk.
  raw_ptr<views::BoxLayoutView> desk_shortcut_view_ = nullptr;

  // The label for the desk shortcut view containing the desk number.
  raw_ptr<views::Label> desk_shortcut_label_ = nullptr;

  // True when this mini view is being animated to be removed from the bar.
  bool is_animating_to_remove_ = false;

  // We force showing desk buttons when the mini_view is long pressed or
  // tapped using touch gestures.
  bool force_show_desk_buttons_ = false;

  // When the DeskNameView is focused, we select all its text. However, if it is
  // focused via a mouse press event, on mouse release will clear the selection.
  // Therefore, we defer selecting all text until we receive that mouse release.
  bool defer_select_all_ = false;

  bool is_desk_name_being_modified_ = false;

  // This is initialized to true and tells the OnViewBlurred function if the
  // user wants to set a new desk name. We set this to false if the
  // HandleKeyEvent function detects that the escape key was pressed so that
  // OnViewBlurred does not change the name of `desk_`.
  bool should_commit_name_changes_ = true;

  // A handle that aborts the active mini view animation when:
  //   1. The mini view is destroyed as the whole bar view is gone.
  //   2. Another new animation is triggered for the same mini view.
  std::unique_ptr<views::AnimationAbortHandle> animation_abort_handle_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_MINI_VIEW_H_
