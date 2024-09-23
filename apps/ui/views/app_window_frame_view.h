// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_UI_VIEWS_APP_WINDOW_FRAME_VIEW_H_
#define APPS_UI_VIEWS_APP_WINDOW_FRAME_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/window/non_client_view.h"

namespace extensions {
class NativeAppWindow;
}

namespace gfx {
class Canvas;
class Point;
}

namespace views {
class ImageButton;
class Widget;
}

namespace apps {

// A frameless or non-Ash, non-panel NonClientFrameView for app windows.
class AppWindowFrameView : public views::NonClientFrameView {
  METADATA_HEADER(AppWindowFrameView, views::NonClientFrameView)

 public:
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
  AppWindowFrameView(const AppWindowFrameView&) = delete;
  AppWindowFrameView& operator=(const AppWindowFrameView&) = delete;
  ~AppWindowFrameView() override;

  void Init();

  void SetResizeSizes(int resize_inside_bounds_size,
                      int resize_outside_bounds_size,
                      int resize_area_corner_size);
  void SetFrameCornerRadius(int radius);
  int resize_inside_bounds_size() const { return resize_inside_bounds_size_; }

 protected:
  bool draw_frame() const { return draw_frame_; }

 private:
  // views::NonClientFrameView implementation.
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void SizeConstraintsChanged() override;

  // views::View implementation.
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void Layout(PassKey) override;
  void OnPaint(gfx::Canvas* canvas) override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;

  // Some button images we use depend on the color of the frame. This
  // will set these images based on the color of the frame.
  void SetButtonImagesForFrame();

  // Return the current frame color based on the active state of the window.
  SkColor CurrentFrameColor();

  raw_ptr<views::Widget, DanglingUntriaged> widget_;
  raw_ptr<extensions::NativeAppWindow, DanglingUntriaged> window_;
  bool draw_frame_;
  SkColor active_frame_color_;
  SkColor inactive_frame_color_;
  raw_ptr<views::ImageButton, DanglingUntriaged> close_button_ = nullptr;
  raw_ptr<views::ImageButton, DanglingUntriaged> maximize_button_ = nullptr;
  raw_ptr<views::ImageButton, DanglingUntriaged> restore_button_ = nullptr;
  raw_ptr<views::ImageButton, DanglingUntriaged> minimize_button_ = nullptr;

  // Allow resize for clicks this many pixels inside the bounds.
  int resize_inside_bounds_size_ = 5;

  // Allow resize for clicks  this many pixels outside the bounds.
  int resize_outside_bounds_size_ = 0;

  // Size in pixels of the lower-right corner resize handle.
  int resize_area_corner_size_ = 16;

  // Radius for the top two corners of the frame.
  int frame_corner_radius_ = 0;
};

}  // namespace apps

#endif  // APPS_UI_VIEWS_APP_WINDOW_FRAME_VIEW_H_
