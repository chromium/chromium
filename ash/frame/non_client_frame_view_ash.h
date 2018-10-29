// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_NON_CLIENT_FRAME_VIEW_ASH_H_
#define ASH_FRAME_NON_CLIENT_FRAME_VIEW_ASH_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/frame/header_view.h"
#include "ash/public/cpp/menu_utils.h"
#include "ash/public/interfaces/menu.mojom.h"
#include "ash/shell_observer.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "base/macros.h"
#include "base/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/window/non_client_view.h"

namespace views {
class Widget;
}

namespace ash {

class FrameCaptionButtonContainerView;
class HeaderView;
class ImmersiveFullscreenController;
class ImmersiveFullscreenControllerDelegate;
class NonClientFrameViewAshImmersiveHelper;

// A NonClientFrameView used for packaged apps, dialogs and other non-browser
// windows. It supports immersive fullscreen. When in immersive fullscreen, the
// client view takes up the entire widget and the window header is an overlay.
// The window header overlay slides onscreen when the user hovers the mouse at
// the top of the screen. See also views::CustomFrameView and
// BrowserNonClientFrameViewAsh.
class ASH_EXPORT NonClientFrameViewAsh : public views::NonClientFrameView,
                                         public ShellObserver,
                                         public SplitViewController::Observer,
                                         public views::ContextMenuController,
                                         public ui::SimpleMenuModel::Delegate {
 public:
  // Internal class name.
  static const char kViewClassName[];

  // |control_immersive| controls whether ImmersiveFullscreenController is
  // created for the NonClientFrameViewAsh; if true and a WindowStateDelegate
  // has not been set on the WindowState associated with |frame|, then an
  // ImmersiveFullscreenController is created.
  // If ImmersiveFullscreenControllerDelegate is not supplied, HeaderView is
  // used as the ImmersiveFullscreenControllerDelegate.
  explicit NonClientFrameViewAsh(views::Widget* frame);
  ~NonClientFrameViewAsh() override;

  static NonClientFrameViewAsh* Get(aura::Window* window);

  // Sets the caption button modeland updates the caption buttons.
  void SetCaptionButtonModel(std::unique_ptr<CaptionButtonModel> model);

  // Inits |immersive_fullscreen_controller| so that the controller reveals
  // and hides |header_view_| in immersive fullscreen.
  // NonClientFrameViewAsh does not take ownership of
  // |immersive_fullscreen_controller|.
  void InitImmersiveFullscreenControllerForView(
      ImmersiveFullscreenController* immersive_fullscreen_controller);

  // Sets the active and inactive frame colors. Note the inactive frame color
  // will have some transparency added when the frame is drawn.
  void SetFrameColors(SkColor active_frame_color, SkColor inactive_frame_color);

  // Sets the height of the header. If |height| has no value (the default), the
  // preferred height is used.
  void SetHeaderHeight(base::Optional<int> height);

  // Get the view of the header.
  HeaderView* GetHeaderView();

  // Calculate the client bounds for given window bounds.
  gfx::Rect GetClientBoundsForWindowBounds(
      const gfx::Rect& window_bounds) const;

  // Sets the menu items to show in the context menu. If |menu_item_list| is
  // empty, no context menu will be shown. Menu item activation is dispatched to
  // |delegate|.
  void SetWindowFrameMenuItems(const menu_utils::MenuItemList& menu_item_list,
                               mojom::MenuDelegatePtr delegate);

  // views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask) override;
  void ResetWindowControls() override;
  void UpdateWindowIcon() override;
  void UpdateWindowTitle() override;
  void SizeConstraintsChanged() override;
  void ActivationChanged(bool active) override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  const char* GetClassName() const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void SchedulePaintInRect(const gfx::Rect& r) override;
  void SetVisible(bool visible) override;

  // If |paint| is false, we should not paint the header. Used for overview mode
  // with OnOverviewModeStarting() and OnOverviewModeEnded() to hide/show the
  // header of v2 and ARC apps.
  virtual void SetShouldPaintHeader(bool paint);

  // ShellObserver:
  void OnOverviewModeStarting() override;
  void OnOverviewModeEnded() override;

  // SplitViewController::Observer:
  void OnSplitViewStateChanged(SplitViewController::State previous_state,
                               SplitViewController::State state) override;

  // views::ContextMenuController:
  void ShowContextMenuForView(View* source,
                              const gfx::Point& point,
                              ui::MenuSourceType source_type) override;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  const views::View* GetAvatarIconViewForTest() const;

  SkColor GetActiveFrameColorForTest() const;
  SkColor GetInactiveFrameColorForTest() const;

  views::Widget* frame() { return frame_; }

 protected:
  // Called when overview mode or split view state changed. If overview mode
  // and split view mode are both active at the same time, the header of the
  // window in split view should be visible, but the headers of other windows
  // in overview are not.
  void UpdateHeaderView();

 private:
  class OverlayView;
  friend class NonClientFrameViewAshSizeLock;
  friend class NonClientFrameViewAshTestWidgetDelegate;
  friend class TestWidgetConstraintsDelegate;

  // views::NonClientFrameView:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  // Returns the container for the minimize/maximize/close buttons that is
  // held by the HeaderView. Used in testing.
  FrameCaptionButtonContainerView* GetFrameCaptionButtonContainerViewForTest();

  // Height from top of window to top of client area.
  int NonClientTopBorderHeight() const;

  // Not owned.
  views::Widget* frame_;

  // View which contains the title and window controls.
  HeaderView* header_view_ = nullptr;

  OverlayView* overlay_view_ = nullptr;

  static bool use_empty_minimum_size_for_test_;

  // Track whether the device is in overview mode. Set this to true when
  // overview mode started and false when overview mode finished. Use this to
  // check whether we should paint when splitview state changes instead of
  // Shell::Get()->window_selector_controller()->IsSelecting() because the
  // later actually may be still be false after overview mode has started.
  bool in_overview_mode_ = false;

  // Helpers for the context menu users will see when right-clicking on
  // |header_view_|.
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  mojom::MenuDelegatePtr menu_delegate_;

  std::unique_ptr<NonClientFrameViewAshImmersiveHelper> immersive_helper_;

  DISALLOW_COPY_AND_ASSIGN(NonClientFrameViewAsh);
};

}  // namespace ash

#endif  // ASH_FRAME_NON_CLIENT_FRAME_VIEW_ASH_H_
