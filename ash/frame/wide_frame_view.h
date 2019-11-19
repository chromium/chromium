// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_WIDE_FRAME_VIEW_H_
#define ASH_FRAME_WIDE_FRAME_VIEW_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/caption_buttons/caption_button_model.h"
#include "ash/public/cpp/immersive/immersive_fullscreen_controller_delegate.h"
#include "ash/wm/overview/overview_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class Widget;
}

namespace ash {
class HeaderView;
class ImmersiveFullscreenController;

// WideFrameView is used for the case where the widget's maximzed/fullscreen
// doesn't cover the entire workarea/display area but the caption frame should
// occupy the full width and placed at the top of the display. Its widget is
// created as WIDGET_OWNS_NATIVE_WIDGET and caller is supposed to own and manage
// its lifetime.
//
// TODO(oshima): Currently client is responsible for hooking this up to
// the target widget because ImmersiveFullscreenController is not owned by
// NonClientFrameViewAsh. Investigate if we integrate this into
// NonClientFrameViewAsh.
class ASH_EXPORT WideFrameView : public views::WidgetDelegateView,
                                 public aura::WindowObserver,
                                 public display::DisplayObserver,
                                 public ImmersiveFullscreenControllerDelegate {
 public:
  explicit WideFrameView(views::Widget* target);
  ~WideFrameView() override;

  // Initialize |immersive_fullscreen_controller| so that the controller reveals
  // and |hides_header_| in immersive mode.
  void Init(ImmersiveFullscreenController* controller);

  // Set the caption model for caption buttions on this frame.
  void SetCaptionButtonModel(std::unique_ptr<CaptionButtonModel> mode);

  HeaderView* header_view() { return header_view_; }

 private:
  static gfx::Rect GetFrameBounds(views::Widget* target);

  // views::WidgetDelegateView:
  void DeleteDelegate() override;

  // views::View:
  void Layout() override;
  void OnMouseEvent(ui::MouseEvent* event) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // ImmersiveFullscreenControllerDelegate:
  void OnImmersiveRevealStarted() override;
  void OnImmersiveRevealEnded() override;
  void OnImmersiveFullscreenEntered() override;
  void OnImmersiveFullscreenExited() override;
  void SetVisibleFraction(double visible_fraction) override;
  std::vector<gfx::Rect> GetVisibleBoundsInScreen() const override;

  HeaderView* GetTargetHeaderView();

  // The target widget this frame will control.
  views::Widget* target_;

  std::unique_ptr<views::Widget> widget_;

  HeaderView* header_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WideFrameView);
};

}  // namespace ash

#endif  // ASH_FRAME_WIDE_FRAME_VIEW_H_
