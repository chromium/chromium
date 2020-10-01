// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/non_client_frame_view_ash.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "ash/frame/header_view.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/caption_buttons/frame_caption_button_container_view.h"
#include "ash/public/cpp/default_frame_header.h"
#include "ash/public/cpp/frame_utils.h"
#include "ash/public/cpp/immersive/immersive_fullscreen_controller.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_observer.h"
#include "ash/wm/window_util.h"
#include "base/bind.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/widget.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ash::NonClientFrameViewAsh*)

namespace ash {

DEFINE_UI_CLASS_PROPERTY_KEY(NonClientFrameViewAsh*,
                             kNonClientFrameViewAshKey,
                             nullptr)

// This helper enables and disables immersive mode in response to state such as
// tablet mode and fullscreen changing. For legacy reasons, it's only
// instantiated for windows that have no WindowStateDelegate provided.
class NonClientFrameViewAshImmersiveHelper : public WindowStateObserver,
                                             public aura::WindowObserver,
                                             public TabletModeObserver {
 public:
  NonClientFrameViewAshImmersiveHelper(views::Widget* widget,
                                       NonClientFrameViewAsh* custom_frame_view)
      : widget_(widget),
        window_state_(WindowState::Get(widget->GetNativeWindow())) {
    window_state_->window()->AddObserver(this);
    window_state_->AddObserver(this);

    Shell::Get()->tablet_mode_controller()->AddObserver(this);

    immersive_fullscreen_controller_ =
        std::make_unique<ImmersiveFullscreenController>();
    custom_frame_view->InitImmersiveFullscreenControllerForView(
        immersive_fullscreen_controller_.get());
  }

  ~NonClientFrameViewAshImmersiveHelper() override {
    if (Shell::Get()->tablet_mode_controller())
      Shell::Get()->tablet_mode_controller()->RemoveObserver(this);

    if (window_state_) {
      window_state_->RemoveObserver(this);
      window_state_->window()->RemoveObserver(this);
    }
  }

  // TabletModeObserver:
  void OnTabletModeStarted() override {
    if (window_state_->IsFullscreen())
      return;
    if (Shell::Get()->tablet_mode_controller()->ShouldAutoHideTitlebars(
            widget_)) {
      ImmersiveFullscreenController::EnableForWidget(widget_, true);
    }
  }

  void OnTabletModeEnded() override {
    if (window_state_->IsFullscreen())
      return;

    ImmersiveFullscreenController::EnableForWidget(widget_, false);
  }

 private:
  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    window_state_->RemoveObserver(this);
    window->RemoveObserver(this);
    window_state_ = nullptr;
  }

  // WindowStateObserver:
  void OnPostWindowStateTypeChange(WindowState* window_state,
                                   WindowStateType old_type) override {
    views::Widget* widget =
        views::Widget::GetWidgetForNativeWindow(window_state->window());
    if (immersive_fullscreen_controller_ &&
        Shell::Get()->tablet_mode_controller() &&
        Shell::Get()->tablet_mode_controller()->ShouldAutoHideTitlebars(
            widget)) {
      if (window_state->IsMinimized())
        ImmersiveFullscreenController::EnableForWidget(widget_, false);
      else if (window_state->IsMaximized())
        ImmersiveFullscreenController::EnableForWidget(widget_, true);
      return;
    }

    if (!window_state->IsFullscreen() && !window_state->IsMinimized())
      ImmersiveFullscreenController::EnableForWidget(widget_, false);

    if (window_state->IsFullscreen() &&
        window_state->window()->GetProperty(kImmersiveImpliedByFullscreen)) {
      ImmersiveFullscreenController::EnableForWidget(widget_, true);
    }
  }

  NonClientFrameViewAsh* GetFrameView() {
    views::Widget* widget =
        views::Widget::GetWidgetForNativeWindow(window_state_->window());
    return static_cast<NonClientFrameViewAsh*>(
        widget->non_client_view()->frame_view());
  }

  views::Widget* widget_;
  WindowState* window_state_;
  std::unique_ptr<ImmersiveFullscreenController>
      immersive_fullscreen_controller_;

  DISALLOW_COPY_AND_ASSIGN(NonClientFrameViewAshImmersiveHelper);
};

// View which takes up the entire widget and contains the HeaderView. HeaderView
// is a child of OverlayView to avoid creating a larger texture than necessary
// when painting the HeaderView to its own layer.
class NonClientFrameViewAsh::OverlayView : public views::View,
                                           public views::ViewTargeterDelegate {
 public:
  explicit OverlayView(HeaderView* header_view);
  ~OverlayView() override;

  // views::View:
  void Layout() override;
  const char* GetClassName() const override { return "OverlayView"; }

 private:
  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  HeaderView* header_view_;

  DISALLOW_COPY_AND_ASSIGN(OverlayView);
};

NonClientFrameViewAsh::OverlayView::OverlayView(HeaderView* header_view)
    : header_view_(header_view) {
  AddChildView(header_view);
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
}

NonClientFrameViewAsh::OverlayView::~OverlayView() = default;

void NonClientFrameViewAsh::OverlayView::Layout() {
  // Layout |header_view_| because layout affects the result of
  // GetPreferredOnScreenHeight().
  header_view_->Layout();

  int onscreen_height = header_view_->GetPreferredOnScreenHeight();
  int height = header_view_->GetPreferredHeight();
  if (onscreen_height == 0 || !GetVisible()) {
    header_view_->SetVisible(false);
    // Make sure the correct width is set even when immersive is enabled, but
    // never revealed yet.
    header_view_->SetBounds(0, 0, width(), height);
  } else {
    header_view_->SetBounds(0, onscreen_height - height, width(), height);
    header_view_->SetVisible(true);
  }
}

bool NonClientFrameViewAsh::OverlayView::DoesIntersectRect(
    const views::View* target,
    const gfx::Rect& rect) const {
  CHECK_EQ(target, this);
  // Grab events in the header view. Return false for other events so that they
  // can be handled by the client view.
  return header_view_->HitTestRect(rect);
}

// static
const char NonClientFrameViewAsh::kViewClassName[] = "NonClientFrameViewAsh";

NonClientFrameViewAsh::NonClientFrameViewAsh(views::Widget* frame)
    : frame_(frame),
      header_view_(new HeaderView(frame, this)),
      overlay_view_(new OverlayView(header_view_)) {
  DCHECK(frame_);

  header_view_->set_immersive_mode_changed_callback(base::BindRepeating(
      &NonClientFrameViewAsh::InvalidateLayout, weak_factory_.GetWeakPtr()));

  aura::Window* frame_window = frame->GetNativeWindow();
  window_util::InstallResizeHandleWindowTargeterForWindow(frame_window);
  // |header_view_| is set as the non client view's overlay view so that it can
  // overlay the web contents in immersive fullscreen.
  // TODO(pkasting): Consider having something like NonClientViewAsh, which
  // would avoid the need to expose an "overlay view" concept on the
  // cross-platform class, and might allow for simpler creation/ownership/
  // plumbing.
  frame->non_client_view()->SetOverlayView(overlay_view_);

  // A delegate may be set which takes over the responsibilities of the
  // NonClientFrameViewAshImmersiveHelper. This is the case for container apps
  // such as ARC++, and in some tests.
  WindowState* window_state = WindowState::Get(frame_window);
  // A window may be created as a child window of the toplevel (captive portal).
  // TODO(oshima): It should probably be a transient child rather than normal
  // child. Investigate if we can remove this check.
  if (window_state && !window_state->HasDelegate()) {
    immersive_helper_ =
        std::make_unique<NonClientFrameViewAshImmersiveHelper>(frame, this);
  }

  frame_window->SetProperty(kNonClientFrameViewAshKey, this);
}

NonClientFrameViewAsh::~NonClientFrameViewAsh() = default;

// static
NonClientFrameViewAsh* NonClientFrameViewAsh::Get(aura::Window* window) {
  return window->GetProperty(kNonClientFrameViewAshKey);
}

void NonClientFrameViewAsh::InitImmersiveFullscreenControllerForView(
    ImmersiveFullscreenController* immersive_fullscreen_controller) {
  immersive_fullscreen_controller->Init(header_view_, frame_, header_view_);
}

void NonClientFrameViewAsh::SetFrameColors(SkColor active_frame_color,
                                           SkColor inactive_frame_color) {
  aura::Window* frame_window = frame_->GetNativeWindow();
  frame_window->SetProperty(kFrameActiveColorKey, active_frame_color);
  frame_window->SetProperty(kFrameInactiveColorKey, inactive_frame_color);
}

void NonClientFrameViewAsh::SetCaptionButtonModel(
    std::unique_ptr<CaptionButtonModel> model) {
  header_view_->caption_button_container()->SetModel(std::move(model));
  header_view_->UpdateCaptionButtons();
}

HeaderView* NonClientFrameViewAsh::GetHeaderView() {
  return header_view_;
}

gfx::Rect NonClientFrameViewAsh::GetClientBoundsForWindowBounds(
    const gfx::Rect& window_bounds) const {
  gfx::Rect client_bounds(window_bounds);
  client_bounds.Inset(0, NonClientTopBorderHeight(), 0, 0);
  return client_bounds;
}

gfx::Rect NonClientFrameViewAsh::GetBoundsForClientView() const {
  gfx::Rect client_bounds = bounds();
  client_bounds.Inset(0, NonClientTopBorderHeight(), 0, 0);
  return client_bounds;
}

gfx::Rect NonClientFrameViewAsh::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  gfx::Rect window_bounds = client_bounds;
  window_bounds.Inset(0, -NonClientTopBorderHeight(), 0, 0);
  return window_bounds;
}

int NonClientFrameViewAsh::NonClientHitTest(const gfx::Point& point) {
  return FrameBorderNonClientHitTest(this, point);
}

void NonClientFrameViewAsh::GetWindowMask(const gfx::Size& size,
                                          SkPath* window_mask) {
  // No window masks in Aura.
}

void NonClientFrameViewAsh::ResetWindowControls() {
  header_view_->ResetWindowControls();
}

void NonClientFrameViewAsh::UpdateWindowIcon() {}

void NonClientFrameViewAsh::UpdateWindowTitle() {
  header_view_->SchedulePaintForTitle();
}

void NonClientFrameViewAsh::SizeConstraintsChanged() {
  header_view_->UpdateCaptionButtons();
}

gfx::Size NonClientFrameViewAsh::CalculatePreferredSize() const {
  gfx::Size pref = frame_->client_view()->GetPreferredSize();
  gfx::Rect bounds(0, 0, pref.width(), pref.height());
  return frame_->non_client_view()
      ->GetWindowBoundsForClientBounds(bounds)
      .size();
}

void NonClientFrameViewAsh::Layout() {
  if (!GetEnabled())
    return;
  views::NonClientFrameView::Layout();
  aura::Window* frame_window = frame_->GetNativeWindow();
  frame_window->SetProperty(aura::client::kTopViewInset,
                            NonClientTopBorderHeight());
}

const char* NonClientFrameViewAsh::GetClassName() const {
  return kViewClassName;
}

gfx::Size NonClientFrameViewAsh::GetMinimumSize() const {
  if (!GetEnabled())
    return gfx::Size();

  gfx::Size min_client_view_size(frame_->client_view()->GetMinimumSize());
  return gfx::Size(
      std::max(header_view_->GetMinimumWidth(), min_client_view_size.width()),
      NonClientTopBorderHeight() + min_client_view_size.height());
}

gfx::Size NonClientFrameViewAsh::GetMaximumSize() const {
  gfx::Size max_client_size(frame_->client_view()->GetMaximumSize());
  int width = 0;
  int height = 0;

  if (max_client_size.width() > 0)
    width = std::max(header_view_->GetMinimumWidth(), max_client_size.width());
  if (max_client_size.height() > 0)
    height = NonClientTopBorderHeight() + max_client_size.height();

  return gfx::Size(width, height);
}

void NonClientFrameViewAsh::SetVisible(bool visible) {
  overlay_view_->SetVisible(visible);
  views::View::SetVisible(visible);
  // We need to re-layout so that client view will occupy entire window.
  InvalidateLayout();
}

void NonClientFrameViewAsh::SetShouldPaintHeader(bool paint) {
  header_view_->SetShouldPaintHeader(paint);
}

int NonClientFrameViewAsh::NonClientTopBorderHeight() const {
  // The frame should not occupy the window area when it's in fullscreen,
  // not visible or disabled.
  if (frame_->IsFullscreen() || !GetVisible() || !GetEnabled() ||
      header_view_->in_immersive_mode()) {
    return 0;
  }
  return header_view_->GetPreferredHeight();
}

const views::View* NonClientFrameViewAsh::GetAvatarIconViewForTest() const {
  return header_view_->avatar_icon();
}

SkColor NonClientFrameViewAsh::GetActiveFrameColorForTest() const {
  return frame_->GetNativeWindow()->GetProperty(kFrameActiveColorKey);
}

SkColor NonClientFrameViewAsh::GetInactiveFrameColorForTest() const {
  return frame_->GetNativeWindow()->GetProperty(kFrameInactiveColorKey);
}

void NonClientFrameViewAsh::OnDidSchedulePaint(const gfx::Rect& r) {
  // We may end up here before |header_view_| has been added to the Widget.
  if (header_view_->GetWidget()) {
    // The HeaderView is not a child of NonClientFrameViewAsh. Redirect the
    // paint to HeaderView instead.
    gfx::RectF to_paint(r);
    views::View::ConvertRectToTarget(this, header_view_, &to_paint);
    header_view_->SchedulePaintInRect(gfx::ToEnclosingRect(to_paint));
  }
}

// views::NonClientFrameView:
bool NonClientFrameViewAsh::DoesIntersectRect(const views::View* target,
                                              const gfx::Rect& rect) const {
  CHECK_EQ(target, this);
  // NonClientView hit tests the NonClientFrameView first instead of going in
  // z-order. Return false so that events get to the OverlayView.
  return false;
}

FrameCaptionButtonContainerView*
NonClientFrameViewAsh::GetFrameCaptionButtonContainerViewForTest() {
  return header_view_->caption_button_container();
}

void NonClientFrameViewAsh::PaintAsActiveChanged() {
  header_view_->GetFrameHeader()->SetPaintAsActive(ShouldPaintAsActive());
  frame_->non_client_view()->Layout();
}

}  // namespace ash
