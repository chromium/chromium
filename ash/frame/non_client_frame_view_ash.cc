// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/non_client_frame_view_ash.h"

#include <memory>

#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_observer.h"
#include "ash/wm/window_util.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "chromeos/ui/frame/header_view.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller.h"
#include "chromeos/ui/frame/non_client_frame_view_base.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/hit_test.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/display/display_observer.h"
#include "ui/display/tablet_state.h"
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
                                             public display::DisplayObserver {
 public:
  NonClientFrameViewAshImmersiveHelper(views::Widget* widget,
                                       NonClientFrameViewAsh* custom_frame_view)
      : widget_(widget),
        window_state_(WindowState::Get(widget->GetNativeWindow())) {
    window_state_->window()->AddObserver(this);
    window_state_->AddObserver(this);

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
    if (window_state_) {
      window_state_->RemoveObserver(this);
      window_state_->window()->RemoveObserver(this);
    }
  }

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override {
    if (!window_state_ || window_state_->IsFullscreen()) {
      return;
    }

    switch (state) {
      case display::TabletState::kEnteringTabletMode:
      case display::TabletState::kExitingTabletMode:
        break;
      case display::TabletState::kInTabletMode:
        if (Shell::Get()->tablet_mode_controller()->ShouldAutoHideTitlebars(
                widget_) &&
            !window_state_->IsFloated()) {
          ImmersiveFullscreenController::EnableForWidget(widget_, true);
        }
        break;
      case display::TabletState::kInClamshellMode:
        ImmersiveFullscreenController::EnableForWidget(widget_, false);
        break;
    }
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
      if (window_state->IsMinimized() || window_state->IsFloated())
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

  raw_ptr<views::Widget> widget_;
  raw_ptr<WindowState> window_state_;
  std::unique_ptr<ImmersiveFullscreenController>
      immersive_fullscreen_controller_;
  display::ScopedDisplayObserver display_observer_{this};
};

NonClientFrameViewAsh::NonClientFrameViewAsh(views::Widget* frame)
    : chromeos::NonClientFrameViewBase(frame),
      frame_context_menu_controller_(
          std::make_unique<FrameContextMenuController>(frame, this)) {
  header_view_->set_immersive_mode_changed_callback(base::BindRepeating(
      &NonClientFrameViewAsh::InvalidateLayout, weak_factory_.GetWeakPtr()));

  aura::Window* frame_window = frame->GetNativeWindow();
  window_util::InstallResizeHandleWindowTargeterForWindow(frame_window);

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
  window_observation_.Observe(frame_window);

  header_view_->set_context_menu_controller(
      frame_context_menu_controller_.get());
}

NonClientFrameViewAsh::~NonClientFrameViewAsh() {
  header_view_->set_context_menu_controller(nullptr);
}

// static
NonClientFrameViewAsh* NonClientFrameViewAsh::Get(aura::Window* window) {
  return window->GetProperty(kNonClientFrameViewAshKey);
}

void NonClientFrameViewAsh::InitImmersiveFullscreenControllerForView(
    ImmersiveFullscreenController* immersive_fullscreen_controller) {
  immersive_fullscreen_controller->Init(GetHeaderView(), frame_,
                                        GetHeaderView());
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

gfx::Rect NonClientFrameViewAsh::GetClientBoundsForWindowBounds(
    const gfx::Rect& window_bounds) const {
  gfx::Rect client_bounds(window_bounds);
  client_bounds.Inset(gfx::Insets::TLBR(NonClientTopBorderHeight(), 0, 0, 0));
  return client_bounds;
}

bool NonClientFrameViewAsh::ShouldShowContextMenu(
    views::View* source,
    const gfx::Point& screen_coords_point) {
  if (header_view_->in_immersive_mode()) {
    // If the `header_view_` is in immersive mode, then a `NonClientHitTest`
    // will return HTCLIENT so manually check whether `point` lies inside
    // `header_view_`.
    gfx::Point point_in_header_coords(screen_coords_point);
    views::View::ConvertPointToTarget(this, GetHeaderView(),
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
  UpdateWindowRoundedCorners();
  InvalidateLayout();
}

void NonClientFrameViewAsh::SetFrameOverlapped(bool overlapped) {
  if (overlapped == frame_overlapped_) {
    return;
  }

  bool fills_bounds_opaquely = true;
  if (overlapped) {
    // When frame is overlapped with the window area, we need to draw header
    // view in front of client content.
    // TODO(b/282627319): remove the layer at the right condition.
    header_view_->SetPaintToLayer();
    header_view_->layer()->parent()->StackAtTop(header_view_->layer());

    // Overlapped frames are now painted onto a dedicated header view layer
    // instead of the non-opaque layer that hosts the widget.
    // For windows that have rounded corners, the upper corners of the header
    // are rounded while the compositor still thinks that the layer fills the
    // whole rect, including the two upper corners.
    // Therefore, the header view layer also needs to be non-opaque to prevent
    // visual artifacts from appearing around the upper corners.
    if (chromeos::ShouldWindowHaveRoundedCorners(frame_->GetNativeWindow())) {
      fills_bounds_opaquely = false;
    }
  }
  if (header_view_->layer()) {
    header_view_->layer()->SetFillsBoundsOpaquely(fills_bounds_opaquely);
  }

  frame_overlapped_ = overlapped;
  InvalidateLayout();
}

void NonClientFrameViewAsh::SetToggleResizeLockMenuCallback(
    base::RepeatingCallback<void()> callback) {
  toggle_resize_lock_menu_callback_ = std::move(callback);
}

void NonClientFrameViewAsh::ClearToggleResizeLockMenuCallback() {
  toggle_resize_lock_menu_callback_.Reset();
}

void NonClientFrameViewAsh::OnWindowPropertyChanged(aura::Window* window,
                                                    const void* key,
                                                    intptr_t old) {
  // ChromeOS has rounded frames for certain window states. If these states
  // changes, we need to update the rounded corners of the frame associate with
  // the `window`accordingly.
  if (chromeos::CanPropertyEffectFrameRadius(key)) {
    UpdateWindowRoundedCorners();

    bool fills_bounds_opaquely = true;
    // For overlapped frames header_view_ layer needs to non-opaque to avoid
    // visual artifacts at the upper corners.
    // See comment in NonClientFrameViewAsh::SetFrameOverlapped.
    if (frame_overlapped_ &&
        chromeos::ShouldWindowHaveRoundedCorners(frame_->GetNativeWindow())) {
      fills_bounds_opaquely = false;
    }
    if (header_view_->layer()) {
      header_view_->layer()->SetFillsBoundsOpaquely(fills_bounds_opaquely);
    }
  }
}

void NonClientFrameViewAsh::OnWindowDestroying(aura::Window* window) {
  window_observation_.Reset();
}

void NonClientFrameViewAsh::UpdateWindowRoundedCorners() {
  if (!GetWidget()) {
    return;
  }

  aura::Window* frame_window = GetWidget()->GetNativeWindow();

  const int corner_radius = chromeos::GetFrameCornerRadius(frame_window);
  frame_window->SetProperty(aura::client::kWindowCornerRadiusKey,
                            corner_radius);

  if (frame_enabled_) {
    header_view_->SetHeaderCornerRadius(corner_radius);
  }

  if (!chromeos::features::IsRoundedWindowsEnabled()) {
    return;
  }

  GetWidget()->client_view()->UpdateWindowRoundedCorners(corner_radius);
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
    views::View::ConvertRectToTarget(this, GetHeaderView(), &to_paint);
    header_view_->SchedulePaintInRect(gfx::ToEnclosingRect(to_paint));
  }
}

void NonClientFrameViewAsh::AddedToWidget() {
  if (highlight_border_overlay_ ||
      !GetWidget()->GetNativeWindow()->GetProperty(
          chromeos::kShouldHaveHighlightBorderOverlay)) {
    return;
  }

  highlight_border_overlay_ =
      std::make_unique<HighlightBorderOverlay>(GetWidget());
}

chromeos::FrameCaptionButtonContainerView*
NonClientFrameViewAsh::GetFrameCaptionButtonContainerViewForTest() {
  return header_view_->caption_button_container();
}

void NonClientFrameViewAsh::UpdateDefaultFrameColors() {
  aura::Window* frame_window = frame_->GetNativeWindow();
  if (!frame_window->GetProperty(kTrackDefaultFrameColors))
    return;

  auto* color_provider = frame_->GetColorProvider();
  const SkColor dialog_title_bar_color =
      color_provider->GetColor(cros_tokens::kDialogTitleBarColor);

  frame_window->SetProperty(kFrameActiveColorKey, dialog_title_bar_color);
  frame_window->SetProperty(kFrameInactiveColorKey, dialog_title_bar_color);
}

BEGIN_METADATA(NonClientFrameViewAsh)
END_METADATA

}  // namespace ash
