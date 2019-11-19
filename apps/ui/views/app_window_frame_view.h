// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_UI_VIEWS_APP_WINDOW_FRAME_VIEW_H_
#define APPS_UI_VIEWS_APP_WINDOW_FRAME_VIEW_H_

#include <string>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/non_client_view.h"

namespace extensions {
class NativeAppWindow;
}

namespace gfx {
class Canvas;
class Point;
}

namespace ui {
class Event;
}

namespace views {
class ImageButton;
class Widget;
}

namespace apps {

// A frameless or non-Ash, non-panel NonClientFrameView for app windows.
class AppWindowFrameView : public views::NonClientFrameView,
                           public views::ButtonListener {
 public:
  static const char kViewClassName[];

  // AppWindowFrameView is used to draw frames for app windows when a non
  // standard frame is needed. This occurs if there is no frame needed, or if
  // there is a frame color.
  // If |draw_frame| is true, the view draws its own window title area and
  // controls, using |frame_color|. If |draw_frame| is not true, no frame is
  // drawn.
  // TODO(benwells): Refactor this to split out frameless and colored frame
  // views. See http://crbug.com/359432.
  AppWindowFrameView(views::Widget* widget,
                     extensions::NativeAppWindow* window,
                     bool draw_frame,
                     const SkColor& active_frame_color,
                     const SkColor& inactive_frame_color);
  ~AppWindowFrameView() override;

  void Init();

  void SetResizeSizes(int resize_inside_bounds_size,
                      int resize_outside_bounds_size,
                      int resize_area_corner_size);
  int resize_inside_bounds_size() const { return resize_inside_bounds_size_; }

 private:
  // views::NonClientFrameView implementation.
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, SkPath* window_mask) override;
  void ResetWindowControls() override {}
  void UpdateWindowIcon() override {}
  void UpdateWindowTitle() override {}
  void SizeConstraintsChanged() override;

  // views::View implementation.
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  const char* GetClassName() const override;
  void OnPaint(gfx::Canvas* canvas) override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;

  // views::ButtonListener implementation.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Some button images we use depend on the color of the frame. This
  // will set these images based on the color of the frame.
  void SetButtonImagesForFrame();

  // Return the current frame color based on the active state of the window.
  SkColor CurrentFrameColor();

  views::Widget* widget_;
  extensions::NativeAppWindow* window_;
  bool draw_frame_;
  SkColor active_frame_color_;
  SkColor inactive_frame_color_;
  views::ImageButton* close_button_ = nullptr;
  views::ImageButton* maximize_button_ = nullptr;
  views::ImageButton* restore_button_ = nullptr;
  views::ImageButton* minimize_button_ = nullptr;

  // Allow resize for clicks this many pixels inside the bounds.
  int resize_inside_bounds_size_ = 5;

  // Allow resize for clicks  this many pixels outside the bounds.
  int resize_outside_bounds_size_ = 0;

  // Size in pixels of the lower-right corner resize handle.
  int resize_area_corner_size_ = 16;

  DISALLOW_COPY_AND_ASSIGN(AppWindowFrameView);
};

}  // namespace apps

#endif  // APPS_UI_VIEWS_APP_WINDOW_FRAME_VIEW_H_
