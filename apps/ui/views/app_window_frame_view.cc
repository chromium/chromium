// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/ui/views/app_window_frame_view.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "cc/paint/paint_flags.h"
#include "chrome/grit/theme_resources.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/common/draggable_region.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/strings/grit/ui_strings.h"  // Accessibility names
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {

const int kCaptionHeight = 25;

}  // namespace

namespace apps {

const char AppWindowFrameView::kViewClassName[] =
    "browser/ui/views/extensions/AppWindowFrameView";

AppWindowFrameView::AppWindowFrameView(views::Widget* widget,
                                       extensions::NativeAppWindow* window,
                                       bool draw_frame,
                                       const SkColor& active_frame_color,
                                       const SkColor& inactive_frame_color)
    : widget_(widget),
      window_(window),
      draw_frame_(draw_frame),
      active_frame_color_(active_frame_color),
      inactive_frame_color_(inactive_frame_color) {}

AppWindowFrameView::~AppWindowFrameView() = default;

void AppWindowFrameView::Init() {
  if (draw_frame_) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    auto close_button = std::make_unique<views::ImageButton>(this);
    close_button->SetImage(
        views::Button::STATE_NORMAL,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_CLOSE).ToImageSkia());
    close_button->SetImage(
        views::Button::STATE_HOVERED,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_CLOSE_H).ToImageSkia());
    close_button->SetImage(
        views::Button::STATE_PRESSED,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_CLOSE_P).ToImageSkia());
    close_button->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));
    close_button_ = AddChildView(std::move(close_button));
    // STATE_NORMAL images are set in SetButtonImagesForFrame, not here.
    auto maximize_button = std::make_unique<views::ImageButton>(this);
    maximize_button->SetImage(
        views::Button::STATE_HOVERED,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_MAXIMIZE_H).ToImageSkia());
    maximize_button->SetImage(
        views::Button::STATE_PRESSED,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_MAXIMIZE_P).ToImageSkia());
    maximize_button->SetImage(
        views::Button::STATE_DISABLED,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_MAXIMIZE_D).ToImageSkia());
    maximize_button->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_APP_ACCNAME_MAXIMIZE));
    maximize_button_ = AddChildView(std::move(maximize_button));
    auto restore_button = std::make_unique<views::ImageButton>(this);
    restore_button->SetImage(
        views::Button::STATE_HOVERED,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_RESTORE_H).ToImageSkia());
    restore_button->SetImage(
        views::Button::STATE_PRESSED,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_RESTORE_P).ToImageSkia());
    restore_button->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_APP_ACCNAME_RESTORE));
    restore_button_ = AddChildView(std::move(restore_button));
    auto minimize_button = std::make_unique<views::ImageButton>(this);
    minimize_button->SetImage(
        views::Button::STATE_HOVERED,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_MINIMIZE_H).ToImageSkia());
    minimize_button->SetImage(
        views::Button::STATE_PRESSED,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_MINIMIZE_P).ToImageSkia());
    minimize_button->SetAccessibleName(
        l10n_util::GetStringUTF16(IDS_APP_ACCNAME_MINIMIZE));
    minimize_button_ = AddChildView(std::move(minimize_button));

    SetButtonImagesForFrame();
  }
}

void AppWindowFrameView::SetResizeSizes(int resize_inside_bounds_size,
                                        int resize_outside_bounds_size,
                                        int resize_area_corner_size) {
  resize_inside_bounds_size_ = resize_inside_bounds_size;
  resize_outside_bounds_size_ = resize_outside_bounds_size;
  resize_area_corner_size_ = resize_area_corner_size;
}

// views::NonClientFrameView implementation.

gfx::Rect AppWindowFrameView::GetBoundsForClientView() const {
  if (!draw_frame_ || widget_->IsFullscreen())
    return bounds();
  return gfx::Rect(
      0, kCaptionHeight, width(), std::max(0, height() - kCaptionHeight));
}

gfx::Rect AppWindowFrameView::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  gfx::Rect window_bounds = client_bounds;
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // Get the difference between the widget's client area bounds and window
  // bounds, and grow |window_bounds| by that amount.
  gfx::Insets native_frame_insets =
      widget_->GetClientAreaBoundsInScreen().InsetsFrom(
          widget_->GetWindowBoundsInScreen());
  window_bounds.Inset(native_frame_insets);
#endif
  if (!draw_frame_) {
    // Enforce minimum size (1, 1) in case that client_bounds is passed with
    // empty size. This could occur when the frameless window is being
    // initialized.
    if (window_bounds.IsEmpty()) {
      window_bounds.set_width(1);
      window_bounds.set_height(1);
    }
    return window_bounds;
  }

  int closeButtonOffsetX = (kCaptionHeight - close_button_->height()) / 2;
  int header_width = close_button_->width() + closeButtonOffsetX * 2;
  return gfx::Rect(window_bounds.x(),
                   window_bounds.y() - kCaptionHeight,
                   std::max(header_width, window_bounds.width()),
                   window_bounds.height() + kCaptionHeight);
}

int AppWindowFrameView::NonClientHitTest(const gfx::Point& point) {
  if (widget_->IsFullscreen())
    return HTCLIENT;

  gfx::Rect expanded_bounds = bounds();
  if (resize_outside_bounds_size_) {
    expanded_bounds.Inset(gfx::Insets(-resize_outside_bounds_size_,
                                      -resize_outside_bounds_size_,
                                      -resize_outside_bounds_size_,
                                      -resize_outside_bounds_size_));
  }
  // Points outside the (possibly expanded) bounds can be discarded.
  if (!expanded_bounds.Contains(point))
    return HTNOWHERE;

  // Check the frame first, as we allow a small area overlapping the contents
  // to be used for resize handles.
  bool can_ever_resize = widget_->widget_delegate()
                             ? widget_->widget_delegate()->CanResize()
                             : false;
  // Don't allow overlapping resize handles when the window is maximized or
  // fullscreen, as it can't be resized in those states.
  int resize_border = (widget_->IsMaximized() || widget_->IsFullscreen())
                          ? 0
                          : resize_inside_bounds_size_;
  int frame_component = GetHTComponentForFrame(point,
                                               resize_border,
                                               resize_border,
                                               resize_area_corner_size_,
                                               resize_area_corner_size_,
                                               can_ever_resize);
  if (frame_component != HTNOWHERE)
    return frame_component;

  // Check for possible draggable region in the client area for the frameless
  // window.
  SkRegion* draggable_region = window_->GetDraggableRegion();
  if (draggable_region && draggable_region->contains(point.x(), point.y()))
    return HTCAPTION;

  int client_component = widget_->client_view()->NonClientHitTest(point);
  if (client_component != HTNOWHERE)
    return client_component;

  // Then see if the point is within any of the window controls.
  if (close_button_ && close_button_->GetVisible() &&
      close_button_->GetMirroredBounds().Contains(point)) {
    return HTCLOSE;
  }
  if ((maximize_button_ && maximize_button_->GetVisible() &&
       maximize_button_->GetMirroredBounds().Contains(point)) ||
      (restore_button_ && restore_button_->GetVisible() &&
       restore_button_->GetMirroredBounds().Contains(point))) {
    return HTMAXBUTTON;
  }
  if (minimize_button_ && minimize_button_->GetVisible() &&
      minimize_button_->GetMirroredBounds().Contains(point)) {
    return HTMINBUTTON;
  }

  // Caption is a safe default.
  return HTCAPTION;
}

void AppWindowFrameView::GetWindowMask(const gfx::Size& size,
                                       SkPath* window_mask) {
  // We got nothing to say about no window mask.
}

void AppWindowFrameView::SizeConstraintsChanged() {
  if (draw_frame_) {
    maximize_button_->SetEnabled(widget_->widget_delegate() &&
                                 widget_->widget_delegate()->CanMaximize());
  }
}

gfx::Size AppWindowFrameView::CalculatePreferredSize() const {
  gfx::Size pref = widget_->client_view()->GetPreferredSize();
  gfx::Rect bounds(0, 0, pref.width(), pref.height());
  return widget_->non_client_view()
      ->GetWindowBoundsForClientBounds(bounds)
      .size();
}

void AppWindowFrameView::Layout() {
  if (!draw_frame_)
    return;
  gfx::Size close_size = close_button_->GetPreferredSize();
  const int kButtonOffsetY = 0;
  const int kButtonSpacing = 1;
  const int kRightMargin = 3;

  close_button_->SetBounds(width() - kRightMargin - close_size.width(),
                           kButtonOffsetY,
                           close_size.width(),
                           close_size.height());

  maximize_button_->SetEnabled(widget_->widget_delegate() &&
                               widget_->widget_delegate()->CanMaximize());
  gfx::Size maximize_size = maximize_button_->GetPreferredSize();
  maximize_button_->SetBounds(
      close_button_->x() - kButtonSpacing - maximize_size.width(),
      kButtonOffsetY,
      maximize_size.width(),
      maximize_size.height());
  gfx::Size restore_size = restore_button_->GetPreferredSize();
  restore_button_->SetBounds(
      close_button_->x() - kButtonSpacing - restore_size.width(),
      kButtonOffsetY,
      restore_size.width(),
      restore_size.height());

  bool maximized = widget_->IsMaximized();
  maximize_button_->SetVisible(!maximized);
  restore_button_->SetVisible(maximized);
  if (maximized)
    maximize_button_->SetState(views::Button::STATE_NORMAL);
  else
    restore_button_->SetState(views::Button::STATE_NORMAL);

  gfx::Size minimize_size = minimize_button_->GetPreferredSize();
  minimize_button_->SetState(views::Button::STATE_NORMAL);
  minimize_button_->SetBounds(
      maximize_button_->x() - kButtonSpacing - minimize_size.width(),
      kButtonOffsetY,
      minimize_size.width(),
      minimize_size.height());
}

void AppWindowFrameView::OnPaint(gfx::Canvas* canvas) {
  if (!draw_frame_)
    return;

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (ShouldPaintAsActive()) {
    close_button_->SetImage(
        views::Button::STATE_NORMAL,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_CLOSE).ToImageSkia());
  } else {
    close_button_->SetImage(
        views::Button::STATE_NORMAL,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_CLOSE_U).ToImageSkia());
  }

  SetButtonImagesForFrame();
  // TODO(benwells): different look for inactive by default.
  cc::PaintFlags flags;
  flags.setAntiAlias(false);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(CurrentFrameColor());
  SkPath path;
  path.moveTo(0, 0);
  path.lineTo(width(), 0);
  path.lineTo(width(), kCaptionHeight);
  path.lineTo(0, kCaptionHeight);
  path.close();
  canvas->DrawPath(path, flags);
}

const char* AppWindowFrameView::GetClassName() const { return kViewClassName; }

gfx::Size AppWindowFrameView::GetMinimumSize() const {
  gfx::Size min_size = widget_->client_view()->GetMinimumSize();
  if (!draw_frame_) {
    min_size.SetToMax(gfx::Size(1, 1));
    return min_size;
  }

  // Ensure we can display the top of the caption area.
  gfx::Rect client_bounds = GetBoundsForClientView();
  min_size.Enlarge(0, client_bounds.y());
  // Ensure we have enough space for the window icon and buttons.  We allow
  // the title string to collapse to zero width.
  int closeButtonOffsetX = (kCaptionHeight - close_button_->height()) / 2;
  int header_width = close_button_->width() + closeButtonOffsetX * 2;
  if (header_width > min_size.width())
    min_size.set_width(header_width);
  return min_size;
}

gfx::Size AppWindowFrameView::GetMaximumSize() const {
  gfx::Size max_size = widget_->client_view()->GetMaximumSize();

  // Add to the client maximum size the height of any title bar and borders.
  gfx::Size client_size = GetBoundsForClientView().size();
  if (max_size.width())
    max_size.Enlarge(width() - client_size.width(), 0);
  if (max_size.height())
    max_size.Enlarge(0, height() - client_size.height());

  return max_size;
}

void AppWindowFrameView::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  DCHECK(draw_frame_);
  if (sender == close_button_)
    widget_->Close();
  else if (sender == maximize_button_)
    widget_->Maximize();
  else if (sender == restore_button_)
    widget_->Restore();
  else if (sender == minimize_button_)
    widget_->Minimize();
}

SkColor AppWindowFrameView::CurrentFrameColor() {
  return widget_->IsActive() ? active_frame_color_ : inactive_frame_color_;
}

void AppWindowFrameView::SetButtonImagesForFrame() {
  DCHECK(draw_frame_);

  // If the frame is dark, we should use the light images so they have some
  // contrast.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (color_utils::IsDark(CurrentFrameColor())) {
    maximize_button_->SetImage(
        views::Button::STATE_NORMAL,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_MAXIMIZE_L).ToImageSkia());
    restore_button_->SetImage(
        views::Button::STATE_NORMAL,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_RESTORE_L).ToImageSkia());
    minimize_button_->SetImage(
        views::Button::STATE_NORMAL,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_MINIMIZE_L).ToImageSkia());
  } else {
    maximize_button_->SetImage(
        views::Button::STATE_NORMAL,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_MAXIMIZE).ToImageSkia());
    restore_button_->SetImage(
        views::Button::STATE_NORMAL,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_RESTORE).ToImageSkia());
    minimize_button_->SetImage(
        views::Button::STATE_NORMAL,
        rb.GetNativeImageNamed(IDR_APP_WINDOW_MINIMIZE).ToImageSkia());
  }
}

}  // namespace apps
