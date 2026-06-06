// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame/frame_view_ash.h"

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
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/default_frame_header.h"
#include "chromeos/ui/frame/default_highlight_border_overlay_delegate.h"
#include "chromeos/ui/frame/frame_header.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "chromeos/ui/frame/header_view.h"
#include "chromeos/ui/frame/immersive/immersive_fullscreen_controller.h"
#include "chromeos/ui/wm/window_util.h"
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
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/widget.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ash::FrameViewAsh*)

namespace ash {

using ::chromeos::ImmersiveFullscreenController;
using ::chromeos::kFrameActiveColorKey;
using ::chromeos::kFrameInactiveColorKey;
using ::chromeos::kImmersiveImpliedByFullscreen;
using ::chromeos::kTrackDefaultFrameColors;
using ::chromeos::WindowStateType;

DEFINE_UI_CLASS_PROPERTY_KEY(FrameViewAsh*, kFrameViewAshKey, nullptr)

////////////////////////////////////////////////////////////////////////////////
// FrameViewAshImmersiveHelper:

// This helper enables and disables immersive mode in response to state such as
// tablet mode and fullscreen changing. For legacy reasons, it's only
// instantiated for windows that have no WindowStateDelegate provided.
class FrameViewAshImmersiveHelper : public WindowStateObserver,
                                    public aura::WindowObserver,
                                    public display::DisplayObserver {
 public:
  FrameViewAshImmersiveHelper(views::Widget* widget,
                              FrameViewAsh* custom_frame_view)
      : widget_(widget),
        window_state_(WindowState::Get(widget->GetNativeWindow())) {
    window_state_->window()->AddObserver(this);
    window_state_->AddObserver(this);

    immersive_fullscreen_controller_ =
        std::make_unique<ImmersiveFullscreenController>();
    immersive_fullscreen_controller_->SetImmersiveModeChangedCallback(
        base::BindRepeating(&ash::window_util::UpdateUiForImmersiveFullscreen));

    custom_frame_view->InitImmersiveFullscreenControllerForView(
        immersive_fullscreen_controller_.get());
  }
  FrameViewAshImmersiveHelper(const FrameViewAshImmersiveHelper&) = delete;
  FrameViewAshImmersiveHelper& operator=(const FrameViewAshImmersiveHelper&) =
      delete;

  ~FrameViewAshImmersiveHelper() override {
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

//////////////////////////////////////////////////
/////////////////////////////////
// FrameViewAsh::OverlayView:

// View which takes up the entire widget and contains the HeaderView. HeaderView
// is a child of OverlayView to avoid creating a larger texture than necessary
// when painting the HeaderView to its own layer.
class FrameViewAsh::OverlayView : public views::View,
                                  public views::ViewTargeterDelegate {
  METADATA_HEADER(OverlayView, views::View)

 public:
  explicit OverlayView(chromeos::HeaderView* header_view)
      : header_view_(header_view) {
    SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
  }
  OverlayView(const OverlayView&) = delete;
  OverlayView& operator=(const OverlayView&) = delete;
  ~OverlayView() override = default;

  // views::View:
  void Layout(PassKey) override {
    // Layout |header_view_| because layout affects the result of
    // GetPreferredOnScreenHeight().
    header_view_->DeprecatedLayoutImmediately();

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

 private:
  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override {
    DCHECK_EQ(target, this);
    // Grab events in the header view. Return false for other events so that
    // they can be handled by the client view.
    return header_view_->HitTestRect(rect);
  }

  raw_ptr<chromeos::HeaderView> header_view_;
};

BEGIN_METADATA(FrameViewAsh, OverlayView)
END_METADATA

////////////////////////////////////////////////////////////////////////////////
// FrameViewAsh:

FrameViewAsh::FrameViewAsh(views::Widget* widget)
    : views::NativeFrameView(widget),
      frame_context_menu_controller_(
          std::make_unique<chromeos::FrameContextMenuController>(widget,
                                                                 this)) {
  DCHECK(widget_);

  auto header_view = std::make_unique<chromeos::HeaderView>(widget_, this);

  auto overlay_view = std::make_unique<OverlayView>(header_view.get());
  header_view_ = overlay_view->AddChildView(std::move(header_view));
  header_view_->Init();
  overlay_view_ = overlay_view.get();

  // |header_view_| is set as the non client view's overlay view so that it can
  // overlay the web contents in immersive fullscreen.
  widget_->non_client_view()->SetOverlayView(overlay_view.release());

  UpdateDefaultFrameColors();

  header_view_->set_immersive_mode_changed_callback(base::BindRepeating(
      &FrameViewAsh::InvalidateLayout, weak_factory_.GetWeakPtr(),
      // This will always be on a fresh call stack, never mid-layout so the
      // value passed here doesn't matter.
      /*avoid_propagate_during_layout=*/false));

  aura::Window* frame_window = widget->GetNativeWindow();
  chromeos::wm::InstallResizeHandleWindowTargeterForWindow(frame_window);

  // A delegate may be set which takes over the responsibilities of the
  // FrameViewAshImmersiveHelper. This is the case for container apps
  // such as ARC++, and in some tests.
  WindowState* window_state = WindowState::Get(frame_window);
  // A window may be created as a child window of the toplevel (captive portal).
  // TODO(oshima): It should probably be a transient child rather than normal
  // child. Investigate if we can remove this check.
  if (window_state && !window_state->HasDelegate()) {
    immersive_helper_ =
        std::make_unique<FrameViewAshImmersiveHelper>(widget, this);
  }

  frame_window->SetProperty(kFrameViewAshKey, this);
  if (!frame_window->GetProperty(aura::client::kWindowRoundedCornersKey)) {
    frame_window->SetProperty(aura::client::kWindowRoundedCornersKey,
                              chromeos::GetWindowRoundedCorners());
  }

  window_observation_.Observe(frame_window);

  const bool remove_standard_frame =
      frame_window->GetProperty(aura::client::kRemoveStandardFrame);
  SetFrameEnabled(!remove_standard_frame);

  header_view_->set_context_menu_controller(
      frame_context_menu_controller_.get());
}

FrameViewAsh::~FrameViewAsh() {
  header_view_->set_context_menu_controller(nullptr);
}

// static
FrameViewAsh* FrameViewAsh::Get(aura::Window* window) {
  return window->GetProperty(kFrameViewAshKey);
}

void FrameViewAsh::InitImmersiveFullscreenControllerForView(
    ImmersiveFullscreenController* immersive_fullscreen_controller) {
  immersive_fullscreen_controller->Init(GetHeaderView(), widget_,
                                        GetHeaderView());
}

void FrameViewAsh::SetFrameColors(SkColor active_frame_color,
                                  SkColor inactive_frame_color) {
  aura::Window* frame_window = widget_->GetNativeWindow();
  frame_window->SetProperty(kTrackDefaultFrameColors, false);
  frame_window->SetProperty(kFrameActiveColorKey, active_frame_color);
  frame_window->SetProperty(kFrameInactiveColorKey, inactive_frame_color);
}

void FrameViewAsh::SetCaptionButtonModel(
    std::unique_ptr<chromeos::CaptionButtonModel> model) {
  header_view_->caption_button_container()->SetModel(std::move(model));
  header_view_->UpdateCaptionButtons();
}

gfx::Rect FrameViewAsh::GetClientBoundsForWindowBounds(
    const gfx::Rect& window_bounds) const {
  gfx::Rect client_bounds(window_bounds);
  client_bounds.Inset(gfx::Insets::TLBR(NonClientTopBorderHeight(), 0, 0, 0));
  return client_bounds;
}

bool FrameViewAsh::ShouldShowContextMenu(
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

void FrameViewAsh::SetShouldPaintHeader(bool paint) {
  header_view_->SetShouldPaintHeader(paint);
}

int FrameViewAsh::NonClientTopBorderPreferredHeight() const {
  return header_view_->GetPreferredHeight();
}

const views::View* FrameViewAsh::GetAvatarIconViewForTest() const {
  return header_view_->avatar_icon();
}

SkColor FrameViewAsh::GetActiveFrameColorForTest() const {
  return widget_->GetNativeWindow()->GetProperty(kFrameActiveColorKey);
}

SkColor FrameViewAsh::GetInactiveFrameColorForTest() const {
  return widget_->GetNativeWindow()->GetProperty(kFrameInactiveColorKey);
}

chromeos::HeaderView* FrameViewAsh::GetHeaderView() {
  return header_view_;
}

void FrameViewAsh::SetFrameEnabled(bool enabled) {
  if (enabled == frame_enabled_)
    return;

  frame_enabled_ = enabled;
  overlay_view_->SetVisible(frame_enabled_);
  header_view_->SetShouldPaintHeader(frame_enabled_);
  UpdateWindowRoundedCorners();
  InvalidateLayout();
}

void FrameViewAsh::SetFrameOverlapped(bool overlapped) {
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
    const aura::Window* window = widget_->GetNativeWindow();
    if (WindowState::Get(window)->ShouldWindowHaveRoundedCorners()) {
      fills_bounds_opaquely = false;
    }
  }
  if (header_view_->layer()) {
    header_view_->layer()->SetFillsBoundsOpaquely(fills_bounds_opaquely);
  }

  frame_overlapped_ = overlapped;
  InvalidateLayout();
}

int FrameViewAsh::NonClientTopBorderHeight() const {
  // The frame should not occupy the window area when it's in fullscreen,
  // not visible or disabled.
  if (widget_->IsFullscreen() || !GetFrameEnabled() ||
      header_view_->in_immersive_mode()) {
    return 0;
  }
  return header_view_->GetPreferredHeight();
}

void FrameViewAsh::SetToggleResizeLockMenuCallback(
    base::RepeatingCallback<void()> callback) {
  toggle_resize_lock_menu_callback_ = std::move(callback);
}

void FrameViewAsh::ClearToggleResizeLockMenuCallback() {
  toggle_resize_lock_menu_callback_.Reset();
}

gfx::Rect FrameViewAsh::GetBoundsForClientView() const {
  gfx::Rect client_bounds = bounds();
  client_bounds.Inset(gfx::Insets::TLBR(NonClientTopBorderHeight(), 0, 0, 0));
  return client_bounds;
}

gfx::Rect FrameViewAsh::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  gfx::Rect window_bounds = client_bounds;
  window_bounds.Inset(gfx::Insets::TLBR(-NonClientTopBorderHeight(), 0, 0, 0));
  return window_bounds;
}

int FrameViewAsh::NonClientHitTest(const gfx::Point& point) {
  return chromeos::FrameBorderNonClientHitTest(this, point,
                                               non_client_hit_test_callback_);
}

void FrameViewAsh::GetWindowMask(const gfx::Size& size, SkPath* window_mask) {
  // No window masks in Aura.
}

void FrameViewAsh::ResetWindowControls() {
  header_view_->ResetWindowControls();
}

void FrameViewAsh::UpdateWindowTitle() {
  header_view_->SchedulePaintForTitle();
}

void FrameViewAsh::SizeConstraintsChanged() {
  header_view_->UpdateCaptionButtons();
}

views::View::Views FrameViewAsh::GetChildrenInZOrder() {
  return header_view_->GetFrameHeader()->GetAdjustedChildrenInZOrder(this);
}

void FrameViewAsh::Layout(PassKey) {
  LayoutSuperclass<views::FrameView>(this);
  if (!GetFrameEnabled()) {
    return;
  }
  aura::Window* frame_window = widget_->GetNativeWindow();
  frame_window->SetProperty(aura::client::kTopViewInset,
                            NonClientTopBorderHeight());
}

gfx::Size FrameViewAsh::GetMinimumSize() const {
  if (!GetFrameEnabled()) {
    return gfx::Size();
  }

  gfx::Size min_client_view_size(widget_->client_view()->GetMinimumSize());
  return gfx::Size(
      std::max(header_view_->GetMinimumWidth(), min_client_view_size.width()),
      NonClientTopBorderHeight() + min_client_view_size.height());
}

gfx::Size FrameViewAsh::GetMaximumSize() const {
  gfx::Size max_client_size(widget_->client_view()->GetMaximumSize());
  int width = 0;
  int height = 0;

  if (max_client_size.width() > 0) {
    width = std::max(header_view_->GetMinimumWidth(), max_client_size.width());
  }
  if (max_client_size.height() > 0) {
    height = NonClientTopBorderHeight() + max_client_size.height();
  }

  return gfx::Size(width, height);
}

void FrameViewAsh::OnThemeChanged() {
  views::NativeFrameView::OnThemeChanged();
  UpdateDefaultFrameColors();
}

void FrameViewAsh::OnWindowPropertyChanged(aura::Window* window,
                                           const void* key,
                                           intptr_t old) {
  // ChromeOS has rounded windows for certain window states. If these states
  // changes, we need to update the rounded corners of the frame associate with
  // the `window`accordingly.
  if (key == chromeos::kWindowHasRoundedCornersKey) {
    UpdateWindowRoundedCorners();

    // For overlapped frames header_view_ layer needs to non-opaque to avoid
    // visual artifacts at the upper corners.
    // See comment in FrameViewAsh::SetFrameOverlapped.
    bool fills_bounds_opaquely = true;
    if (frame_overlapped_ &&
        WindowState::Get(window)->ShouldWindowHaveRoundedCorners()) {
      fills_bounds_opaquely = false;
    }
    if (header_view_->layer()) {
      header_view_->layer()->SetFillsBoundsOpaquely(fills_bounds_opaquely);
    }
  }
}

void FrameViewAsh::OnWindowDestroying(aura::Window* window) {
  window_observation_.Reset();
}

void FrameViewAsh::UpdateWindowRoundedCorners() {
  if (!GetWidget()) {
    return;
  }

  aura::Window* window = GetWidget()->GetNativeWindow();
  auto* window_state = ash::WindowState::Get(window);

  // For certain windows, we do not window state associated with them. (See
  // `ash::WindowState::Get()` for details)
  if (!window_state) {
    return;
  }

  const gfx::RoundedCornersF window_radii =
      window_state->GetWindowRoundedCorners();

  if (frame_enabled_) {
    CHECK_EQ(window_radii.upper_left(), window_radii.upper_right());
    header_view_->SetHeaderCornerRadius(window_radii.upper_left());
  }

  GetWidget()->client_view()->UpdateWindowRoundedCorners(window_radii);
}

base::RepeatingCallback<void()> FrameViewAsh::GetToggleResizeLockMenuCallback()
    const {
  return toggle_resize_lock_menu_callback_;
}

void FrameViewAsh::OnDidSchedulePaint(const gfx::Rect& r) {
  // We may end up here before |header_view_| has been added to the Widget.
  if (header_view_->GetWidget()) {
    // The HeaderView is not a child of FrameViewAsh. Redirect the
    // paint to HeaderView instead.
    gfx::RectF to_paint(r);
    views::View::ConvertRectToTarget(this, GetHeaderView(), &to_paint);
    header_view_->SchedulePaintInRect(gfx::ToEnclosingRect(to_paint));
  }
}

void FrameViewAsh::AddedToWidget() {
  if (highlight_border_overlay_ ||
      !GetWidget()->GetNativeWindow()->GetProperty(
          chromeos::kShouldHaveHighlightBorderOverlay)) {
    return;
  }

  highlight_border_overlay_ = std::make_unique<HighlightBorderOverlay>(
      GetWidget(),
      std::make_unique<chromeos::DefaultHighlightBorderOverlayDelegate>());
}

bool FrameViewAsh::DoesIntersectRect(const views::View* target,
                                     const gfx::Rect& rect) const {
  DCHECK_EQ(target, this);

  // Give the OverlayView the first chance to handle events.
  if (frame_enabled_ && overlay_view_->HitTestRect(rect)) {
    return false;
  }

  // Handle the event if it's within the bounds of the ClientView.
  gfx::RectF rect_in_client_view_coords_f(rect);
  View::ConvertRectToTarget(this, widget_->client_view(),
                            &rect_in_client_view_coords_f);
  gfx::Rect rect_in_client_view_coords =
      gfx::ToEnclosingRect(rect_in_client_view_coords_f);
  return widget_->client_view()->HitTestRect(rect_in_client_view_coords);
}

chromeos::FrameCaptionButtonContainerView*
FrameViewAsh::GetFrameCaptionButtonContainerViewForTest() {
  return header_view_->caption_button_container();
}

void FrameViewAsh::UpdateDefaultFrameColors() {
  aura::Window* frame_window = widget_->GetNativeWindow();
  if (!frame_window->GetProperty(kTrackDefaultFrameColors))
    return;

  auto* color_provider = widget_->GetColorProvider();
  const SkColor dialog_title_bar_color =
      color_provider->GetColor(cros_tokens::kDialogTitleBarColor);

  frame_window->SetProperty(kFrameActiveColorKey, dialog_title_bar_color);
  frame_window->SetProperty(kFrameInactiveColorKey, dialog_title_bar_color);
}

void FrameViewAsh::PaintAsActiveChanged() {
  if (widget_->GetNativeWindow()->is_destroying()) {
    return;
  }

  header_view_->GetFrameHeader()->SetPaintAsActive(ShouldPaintAsActive());
  widget_->non_client_view()->DeprecatedLayoutImmediately();
}

BEGIN_METADATA(FrameViewAsh)
END_METADATA

}  // namespace ash
