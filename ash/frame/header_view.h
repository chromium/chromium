// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_HEADER_VIEW_H_
#define ASH_FRAME_HEADER_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/frame_header.h"
#include "ash/public/cpp/immersive/immersive_fullscreen_controller_delegate.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/view.h"

namespace gfx {
class ImageSkia;
}

namespace views {
class FrameCaptionButton;
class ImageView;
class Widget;
}

namespace ash {

class DefaultFrameHeader;
class FrameCaptionButtonContainerView;
enum class FrameBackButtonState;

// View which paints the frame header (title, caption buttons...). It slides off
// and on screen in immersive fullscreen.
class ASH_EXPORT HeaderView : public views::View,
                              public ImmersiveFullscreenControllerDelegate,
                              public TabletModeObserver,
                              public aura::WindowObserver {
 public:
  // |target_widget| is the widget that the caption buttons act on.
  // |target_widget| is not necessarily the same as the widget the header is
  // placed in. For example, in immersive fullscreen this view may be painted in
  // a widget that slides in and out on top of the main app or browser window.
  // However, clicking a caption button should act on the target widget.
  explicit HeaderView(views::Widget* target_widget);
  ~HeaderView() override;

  void set_immersive_mode_changed_callback(base::RepeatingClosure callback) {
    immersive_mode_changed_callback_ = std::move(callback);
  }

  bool should_paint() { return should_paint_; }

  // Schedules a repaint for the entire title.
  void SchedulePaintForTitle();

  // Tells the window controls to reset themselves to the normal state.
  void ResetWindowControls();

  // Returns the amount of the view's pixels which should be on screen.
  int GetPreferredOnScreenHeight();

  // Returns the view's preferred height.
  int GetPreferredHeight();

  // Returns the view's minimum width.
  int GetMinimumWidth() const;

  // Sets the avatar icon to be displayed on the frame header.
  void SetAvatarIcon(const gfx::ImageSkia& avatar);

  void UpdateCaptionButtons();

  void SetWidthInPixels(int width_in_pixels);

  // views::View:
  void Layout() override;
  void ChildPreferredSizeChanged(views::View* child) override;
  bool IsDrawn() const override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroying(aura::Window* window) override;

  FrameCaptionButtonContainerView* caption_button_container() {
    return caption_button_container_;
  }

  views::View* avatar_icon() const;

  bool in_immersive_mode() const { return in_immersive_mode_; }
  bool is_revealed() const { return fullscreen_visible_fraction_ > 0.0; }

  void SetShouldPaintHeader(bool paint);

  views::FrameCaptionButton* GetBackButton();

  // ImmersiveFullscreenControllerDelegate:
  void OnImmersiveRevealStarted() override;
  void OnImmersiveRevealEnded() override;
  void OnImmersiveFullscreenEntered() override;
  void OnImmersiveFullscreenExited() override;
  void SetVisibleFraction(double visible_fraction) override;
  std::vector<gfx::Rect> GetVisibleBoundsInScreen() const override;
  void Relayout() override;

  DefaultFrameHeader* GetFrameHeader() { return frame_header_.get(); }

 private:
  class HeaderContentView;
  friend class HeaderContentView;

  // Paint the header content.
  void PaintHeaderContent(gfx::Canvas* canvas);

  void UpdateBackButton();
  void UpdateCaptionButtonsVisibility();

  // The widget that the caption buttons act on.
  views::Widget* target_widget_;

  // A callback to run when |in_immersive_mode_| changes.
  base::RepeatingClosure immersive_mode_changed_callback_;

  // Helper for painting the header. The exact type of FrameHeader will depend
  // on the type of window: In Mash, Chrome Browser windows use
  // CustomFrameHeader which is aware of theming. In classic Ash, Chrome Browser
  // windows won't use HeaderView at all. In either configuration, non Browser
  // windows will use DefaultFrameHeader.
  std::unique_ptr<DefaultFrameHeader> frame_header_;

  views::ImageView* avatar_icon_ = nullptr;

  // View which draws the content of the frame.
  HeaderContentView* header_content_view_ = nullptr;

  // View which contains the window caption buttons.
  FrameCaptionButtonContainerView* caption_button_container_ = nullptr;

  // The fraction of the header's height which is visible while in fullscreen.
  // This value is meaningless when not in fullscreen.
  double fullscreen_visible_fraction_ = 0;

  // True if a layer should be used for the immersive mode reveal. Some code
  // needs HeaderView to always paint to a layer instead of only during
  // immersive reveal (see WmNativeWidgetAura).
  bool add_layer_for_immersive_ = false;

  bool did_layout_ = false;

  // False to skip painting. Used for overview mode to hide the header.
  bool should_paint_ = true;

  bool in_immersive_mode_ = false;

  // This is used to compute visible bounds.
  mutable bool is_drawn_override_ = false;

  // Observes property changes to |target_widget_|'s window.
  ScopedObserver<aura::Window, aura::WindowObserver> window_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(HeaderView);
};

}  // namespace ash

#endif  // ASH_FRAME_HEADER_VIEW_H_
