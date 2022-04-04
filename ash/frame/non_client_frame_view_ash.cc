// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/non_client_frame_view_ash.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "ash/frame/header_view.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/public/cpp/style/scoped_light_mode_as_default.h"
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
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/default_frame_header.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/hit_test.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/widget.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ash::NonClientFrameViewAsh*)

namespace ash {

using ::chromeos::ImmersiveFullscreenController;
using ::chromeos::kFrameActiveColorKey;
using ::chromeos::kFrameInactiveColorKey;
using ::chromeos::kImmersiveImpliedByFullscreen;
using ::chromeos::kTrackDefaultFrameColors;
using ::chromeos::WindowStateType;

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
  NonClientFrameViewAshImmersiveHelper(
      const NonClientFrameViewAshImmersiveHelper&) = delete;
  NonClientFrameViewAshImmersiveHelper& operator=(
      const NonClientFrameViewAshImmersiveHelper&) = delete;

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
};

// View which takes up the entire widget and contains the HeaderView. HeaderView
// is a child of OverlayView to avoid creating a larger texture than necessary
// when painting the HeaderView to its own layer.
class NonClientFrameViewAsh::OverlayView : public views::View,
                                           public views::ViewTargeterDelegate {
 public:
  METADATA_HEADER(OverlayView);
  explicit OverlayView(HeaderView* header_view);
  OverlayView(const OverlayView&) = delete;
  OverlayView& operator=(const OverlayView&) = delete;
  ~OverlayView() override;

  // views::View:
  void Layout() override;

 private:
  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  HeaderView* header_view_;
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

BEGIN_METADATA(NonClientFrameViewAsh, OverlayView, views::View)
END_METADATA

NonClientFrameViewAsh::NonClientFrameViewAsh(views::Widget* frame)
    : frame_(frame),
      header_view_(new HeaderView(frame, this)),
      overlay_view_(new OverlayView(header_view_)),
      frame_context_menu_controller_(
          std::make_unique<FrameContextMenuController>(frame, this)) {
  DCHECK(frame_);

  header_view_->Init();
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

  header_view_->set_context_menu_controller(
      frame_context_menu_controller_.get());

  UpdateDefaultFrameColors();
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
  frame_window->SetProperty(kTrackDefaultFrameColors, false);
  frame_window->SetProperty(kFrameActiveColorKey, active_frame_color);
  frame_window->SetProperty(kFrameInactiveColorKey, inactive_frame_color);
}

void NonClientFrameViewAsh::SetCaptionButtonModel(
    std::unique_ptr<chromeos::CaptionButtonModel> model) {
  header_view_->caption_button_container()->SetModel(std::move(model));
  header_view_->UpdateCaptionButtons();
}

HeaderView* NonClientFrameViewAsh::GetHeaderView() {
  return header_view_;
}

gfx::Rect NonClientFrameViewAsh::GetClientBoundsForWindowBounds(
    const gfx::Rect& window_bounds) const {
  gfx::Rect client_bounds(window_bounds);
  client_bounds.Inset(gfx::Insets::TLBR(NonClientTopBorderHeight(), 0, 0, 0));
  return client_bounds;
}

gfx::Rect NonClientFrameViewAsh::GetBoundsForClientView() const {
  gfx::Rect client_bounds = bounds();
  client_bounds.Inset(gfx::Insets::TLBR(NonClientTopBorderHeight(), 0, 0, 0));
  return client_bounds;
}

gfx::Rect NonClientFrameViewAsh::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  gfx::Rect window_bounds = client_bounds;
  window_bounds.Inset(gfx::Insets::TLBR(-NonClientTopBorderHeight(), 0, 0, 0));
  return window_bounds;
}

int NonClientFrameViewAsh::NonClientHitTest(const gfx::Point& point) {
  return chromeos::FrameBorderNonClientHitTest(this, point);
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

views::View::Views NonClientFrameViewAsh::GetChildrenInZOrder() {
  return header_view_->GetFrameHeader()->GetAdjustedChildrenInZOrder(this);
}

gfx::Size NonClientFrameViewAsh::CalculatePreferredSize() const {
  gfx::Size pref = frame_->client_view()->GetPreferredSize();
  gfx::Rect bounds(0, 0, pref.width(), pref.height());
  return frame_->non_client_view()
      ->GetWindowBoundsForClientBounds(bounds)
      .size();
}

void NonClientFrameViewAsh::Layout() {
  views::NonClientFrameView::Layout();
  if (!GetFrameEnabled())
    return;
  aura::Window* frame_window = frame_->GetNativeWindow();
  frame_window->SetProperty(aura::client::kTopViewInset,
                            NonClientTopBorderHeight());
}

gfx::Size NonClientFrameViewAsh::GetMinimumSize() const {
  if (!GetFrameEnabled())
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

void NonClientFrameViewAsh::OnThemeChanged() {
  NonClientFrameView::OnThemeChanged();
  UpdateDefaultFrameColors();
}

bool NonClientFrameViewAsh::ShouldShowContextMenu(
    views::View* source,
    const gfx::Point& screen_coords_point) {
  if (header_view_->in_immersive_mode()) {
    // If the `header_view_` is in immersive mode, then a `NonClientHitTest`
    // will return HTCLIENT so manually check whether `point` lies inside
    // `header_view_`.
    gfx::Point point_in_header_coords(screen_coords_point);
    views::View::ConvertPointToTarget(this, header_view_,
                                      &point_in_header_coords);
    return header_view_->HitTestRect(
        gfx::Rect(point_in_header_coords, gfx::Size(1, 1)));
  }

  // Only show the context menu if `screen_coords_point` is in the caption area.
  gfx::Point point_in_view_coords(screen_coords_point);
  views::View::ConvertPointFromScreen(this, &point_in_view_coords);
  return NonClientHitTest(point_in_view_coords) == HTCAPTION;
}

void NonClientFrameViewAsh::SetShouldPaintHeader(bool paint) {
  header_view_->SetShouldPaintHeader(paint);
}

int NonClientFrameViewAsh::NonClientTopBorderHeight() const {
  // The frame should not occupy the window area when it's in fullscreen,
  // not visible or disabled.
  if (frame_->IsFullscreen() || !GetFrameEnabled() ||
      header_view_->in_immersive_mode()) {
    return 0;
  }
  return header_view_->GetPreferredHeight();
}

int NonClientFrameViewAsh::NonClientTopBorderPreferredHeight() const {
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

void NonClientFrameViewAsh::SetFrameEnabled(bool enabled) {
  if (enabled == frame_enabled_)
    return;

  frame_enabled_ = enabled;
  overlay_view_->SetVisible(frame_enabled_);
  InvalidateLayout();
}

void NonClientFrameViewAsh::SetToggleResizeLockMenuCallback(
    base::RepeatingCallback<void()> callback) {
  toggle_resize_lock_menu_callback_ = std::move(callback);
}

void NonClientFrameViewAsh::ClearToggleResizeLockMenuCallback() {
  toggle_resize_lock_menu_callback_.Reset();
}

base::RepeatingCallback<void()>
NonClientFrameViewAsh::GetToggleResizeLockMenuCallback() const {
  return toggle_resize_lock_menu_callback_;
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

  // Give the OverlayView the first chance to handle events.
  if (frame_enabled_ && overlay_view_->HitTestRect(rect))
    return false;

  // Handle the event if it's within the bounds of the ClientView.
  gfx::RectF rect_in_client_view_coords_f(rect);
  View::ConvertRectToTarget(this, frame_->client_view(),
                            &rect_in_client_view_coords_f);
  gfx::Rect rect_in_client_view_coords =
      gfx::ToEnclosingRect(rect_in_client_view_coords_f);
  return frame_->client_view()->HitTestRect(rect_in_client_view_coords);
}

chromeos::FrameCaptionButtonContainerView*
NonClientFrameViewAsh::GetFrameCaptionButtonContainerViewForTest() {
  return header_view_->caption_button_container();
}

void NonClientFrameViewAsh::PaintAsActiveChanged() {
  header_view_->GetFrameHeader()->SetPaintAsActive(ShouldPaintAsActive());
  frame_->non_client_view()->Layout();
}

void NonClientFrameViewAsh::UpdateDefaultFrameColors() {
  auto* color_provider = ash::ColorProvider::Get();
  aura::Window* frame_window = frame_->GetNativeWindow();
  if (!frame_window->GetProperty(kTrackDefaultFrameColors))
    return;

  // Use scoped light mode to ensure we use light mode colors when the
  // DarkLightMode feature is disabled. Do this because color mode is DARK by
  // default when it is disabled currently (see crbug.com/1291354).
  ash::ScopedLightModeAsDefault scoped_light_mode_as_default;
  frame_window->SetProperty(kFrameActiveColorKey,
                            color_provider->GetActiveDialogTitleBarColor());
  frame_window->SetProperty(kFrameInactiveColorKey,
                            color_provider->GetInactiveDialogTitleBarColor());
}

BEGIN_METADATA(NonClientFrameViewAsh, views::NonClientFrameView)
END_METADATA

}  // namespace ash
