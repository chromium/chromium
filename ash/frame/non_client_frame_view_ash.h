// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_NON_CLIENT_FRAME_VIEW_ASH_H_
#define ASH_FRAME_NON_CLIENT_FRAME_VIEW_ASH_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/frame/header_view.h"
#include "ash/wm/overview/overview_observer.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

namespace views {
class Widget;
}

namespace ash {

class FrameCaptionButtonContainerView;
class ImmersiveFullscreenController;
class NonClientFrameViewAshImmersiveHelper;

// A NonClientFrameView used for packaged apps, dialogs and other non-browser
// windows. It supports immersive fullscreen. When in immersive fullscreen, the
// client view takes up the entire widget and the window header is an overlay.
// The window header overlay slides onscreen when the user hovers the mouse at
// the top of the screen. See also views::CustomFrameView and
// BrowserNonClientFrameViewAsh.
class ASH_EXPORT NonClientFrameViewAsh : public views::NonClientFrameView {
 public:
  // Internal class name.
  static const char kViewClassName[];

  // |control_immersive| controls whether ImmersiveFullscreenController is
  // created for the NonClientFrameViewAsh; if true and a WindowStateDelegate
  // has not been set on the WindowState associated with |frame|, then an
  // ImmersiveFullscreenController is created.
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

  // Get the view of the header.
  HeaderView* GetHeaderView();

  // Calculate the client bounds for given window bounds.
  gfx::Rect GetClientBoundsForWindowBounds(
      const gfx::Rect& window_bounds) const;

  // views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override;
  void ResetWindowControls() override;
  void UpdateWindowIcon() override;
  void UpdateWindowTitle() override;
  void SizeConstraintsChanged() override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  const char* GetClassName() const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  void SetVisible(bool visible) override;

  // If |paint| is false, we should not paint the header. Used for overview mode
  // with OnOverviewModeStarting() and OnOverviewModeEnded() to hide/show the
  // header of v2 and ARC apps.
  virtual void SetShouldPaintHeader(bool paint);

  // Height from top of window to top of client area.
  int NonClientTopBorderHeight() const;

  const views::View* GetAvatarIconViewForTest() const;

  SkColor GetActiveFrameColorForTest() const;
  SkColor GetInactiveFrameColorForTest() const;

  views::Widget* frame() { return frame_; }

 protected:
  // views::View:
  void OnDidSchedulePaint(const gfx::Rect& r) override;

 private:
  class OverlayView;
  friend class NonClientFrameViewAshTestWidgetDelegate;
  friend class TestWidgetConstraintsDelegate;
  friend class WindowServiceDelegateImplTest;

  // views::NonClientFrameView:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  // Returns the container for the minimize/maximize/close buttons that is
  // held by the HeaderView. Used in testing.
  FrameCaptionButtonContainerView* GetFrameCaptionButtonContainerViewForTest();

  // Called when |frame_|'s "paint as active" state has changed.
  void PaintAsActiveChanged();

  // Not owned.
  views::Widget* const frame_;

  // View which contains the title and window controls.
  HeaderView* header_view_ = nullptr;

  OverlayView* overlay_view_ = nullptr;

  std::unique_ptr<NonClientFrameViewAshImmersiveHelper> immersive_helper_;

  std::unique_ptr<views::Widget::PaintAsActiveCallbackList::Subscription>
      paint_as_active_subscription_ =
          frame_->RegisterPaintAsActiveChangedCallback(
              base::BindRepeating(&NonClientFrameViewAsh::PaintAsActiveChanged,
                                  base::Unretained(this)));

  base::WeakPtrFactory<NonClientFrameViewAsh> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NonClientFrameViewAsh);
};

}  // namespace ash

#endif  // ASH_FRAME_NON_CLIENT_FRAME_VIEW_ASH_H_
