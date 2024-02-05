// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_NON_CLIENT_FRAME_VIEW_ASH_H_
#define ASH_FRAME_NON_CLIENT_FRAME_VIEW_ASH_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/frame/frame_context_menu_controller.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ui/frame/header_view.h"
#include "chromeos/ui/frame/highlight_border_overlay.h"
#include "chromeos/ui/frame/non_client_frame_view_base.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/widget/widget.h"

namespace chromeos {
class FrameCaptionButtonContainerView;
class ImmersiveFullscreenController;
}  // namespace chromeos

namespace views {
class Widget;
}  // namespace views

namespace ash {

class NonClientFrameViewAshImmersiveHelper;

// A NonClientFrameView used for packaged apps, dialogs and other non-browser
// windows. It supports immersive fullscreen. When in immersive fullscreen, the
// client view takes up the entire widget and the window header is an overlay.
// The window header overlay slides onscreen when the user hovers the mouse at
// the top of the screen. See also views::CustomFrameView and
// BrowserNonClientFrameViewAsh.
class ASH_EXPORT NonClientFrameViewAsh
    : public chromeos::NonClientFrameViewBase,
      public FrameContextMenuController::Delegate,
      public aura::WindowObserver {
  METADATA_HEADER(NonClientFrameViewAsh, chromeos::NonClientFrameViewBase)

 public:
  // |control_immersive| controls whether ImmersiveFullscreenController is
  // created for the NonClientFrameViewAsh; if true and a WindowStateDelegate
  // has not been set on the WindowState associated with |frame|, then an
  // ImmersiveFullscreenController is created.
  explicit NonClientFrameViewAsh(views::Widget* frame);
  NonClientFrameViewAsh(const NonClientFrameViewAsh&) = delete;
  NonClientFrameViewAsh& operator=(const NonClientFrameViewAsh&) = delete;
  ~NonClientFrameViewAsh() override;

  static NonClientFrameViewAsh* Get(aura::Window* window);

  // Sets the caption button modeland updates the caption buttons.
  void SetCaptionButtonModel(
      std::unique_ptr<chromeos::CaptionButtonModel> model);

  // Inits |immersive_fullscreen_controller| so that the controller reveals
  // and hides |header_view_| in immersive fullscreen.
  // NonClientFrameViewAsh does not take ownership of
  // |immersive_fullscreen_controller|.
  void InitImmersiveFullscreenControllerForView(
      chromeos::ImmersiveFullscreenController* immersive_fullscreen_controller);

  // Sets the active and inactive frame colors. Note the inactive frame color
  // will have some transparency added when the frame is drawn.
  void SetFrameColors(SkColor active_frame_color, SkColor inactive_frame_color);

  // Calculate the client bounds for given window bounds.
  gfx::Rect GetClientBoundsForWindowBounds(
      const gfx::Rect& window_bounds) const;

  // FrameContextMenuController::Delegate:
  bool ShouldShowContextMenu(views::View* source,
                             const gfx::Point& screen_coords_point) override;

  // If |paint| is false, we should not paint the header. Used for overview mode
  // with OnOverviewModeStarting() and OnOverviewModeEnded() to hide/show the
  // header of v2 and ARC apps.
  virtual void SetShouldPaintHeader(bool paint);

  // Expected height from top of window to top of client area when non client
  // view is visible.
  int NonClientTopBorderPreferredHeight() const;

  const views::View* GetAvatarIconViewForTest() const;

  SkColor GetActiveFrameColorForTest() const;
  SkColor GetInactiveFrameColorForTest() const;

  views::Widget* frame() { return frame_; }

  bool GetFrameEnabled() const { return frame_enabled_; }
  bool GetFrameOverlapped() const { return frame_overlapped_; }
  void SetFrameEnabled(bool enabled);
  void SetFrameOverlapped(bool overlapped);

  // Sets the callback to toggle the ARC++ resize-lock menu for this container
  // if applicable, which will be invoked via the keyboard shortcut.
  void SetToggleResizeLockMenuCallback(
      base::RepeatingCallback<void()> callback);
  base::RepeatingCallback<void()> GetToggleResizeLockMenuCallback() const;
  void ClearToggleResizeLockMenuCallback();

  // views::NonClientFrameView:
  void UpdateWindowRoundedCorners() override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroying(aura::Window* window) override;

 protected:
  // views::View:
  void OnDidSchedulePaint(const gfx::Rect& r) override;
  void AddedToWidget() override;

  bool frame_overlapped_ = false;

 private:
  friend class TestWidgetConstraintsDelegate;
  friend class WindowServiceDelegateImplTest;

  // Returns the container for the minimize/maximize/close buttons that is
  // held by the HeaderView. Used in testing.
  chromeos::FrameCaptionButtonContainerView*
  GetFrameCaptionButtonContainerViewForTest();

  // Updates the windows default frame colors if necessary.
  void UpdateDefaultFrameColors() override;

  // Generates a nine patch layer painted with a highlight border.
  std::unique_ptr<HighlightBorderOverlay> highlight_border_overlay_;

  std::unique_ptr<NonClientFrameViewAshImmersiveHelper> immersive_helper_;

  std::unique_ptr<FrameContextMenuController> frame_context_menu_controller_;

  // Observes property changes to window of `target_widget_`.
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};

  base::RepeatingCallback<void()> toggle_resize_lock_menu_callback_;

  base::WeakPtrFactory<NonClientFrameViewAsh> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_FRAME_NON_CLIENT_FRAME_VIEW_ASH_H_
