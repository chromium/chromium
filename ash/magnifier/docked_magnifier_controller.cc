// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/magnifier/docked_magnifier_controller.h"

#include <algorithm>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/magnifier/magnifier_scale_utils.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "base/numerics/ranges.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/reflector.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

constexpr float kDefaultMagnifierScale = 4.0f;
constexpr float kMinMagnifierScale = 1.0f;
constexpr float kMaxMagnifierScale = 20.0f;

// The factor by which the offset of scroll events are scaled.
constexpr float kScrollScaleFactor = 0.0125f;

constexpr char kDockedMagnifierViewportWindowName[] =
    "DockedMagnifierViewportWindow";

// Returns true if High Contrast mode is enabled.
bool IsHighContrastEnabled() {
  return Shell::Get()->accessibility_controller()->IsHighContrastEnabled();
}

// Returns the current cursor location in screen coordinates.
inline gfx::Point GetCursorScreenPoint() {
  return display::Screen::GetScreen()->GetCursorScreenPoint();
}

// Returns the InputMethod associated with the WindowTreeHost of the given
// window.
ui::InputMethod* GetInputMethodForWindow(aura::Window* window) {
  DCHECK(window);
  aura::WindowTreeHost* host = window->GetHost();
  DCHECK(host);
  return host->GetInputMethod();
}

// Updates the workarea of the display associated with |window| such that the
// given magnifier viewport |height| is allocated at the top of the screen.
void SetViewportHeightInWorkArea(aura::Window* window, int height) {
  DCHECK(window);
  ash::Shelf* shelf = ash::Shelf::ForWindow(window);
  ash::ShelfLayoutManager* shelf_layout_manager =
      shelf ? shelf->shelf_layout_manager() : nullptr;
  if (shelf_layout_manager)
    shelf_layout_manager->SetDockedMagnifierHeight(height);
}

// Gets the bounds of the Docked Magnifier viewport widget when placed in the
// display whose root window is |root|. The bounds returned correspond to the
// top quarter portion of the screen.
gfx::Rect GetViewportWidgetBoundsInRoot(aura::Window* root) {
  DCHECK(root);
  DCHECK(root->IsRootWindow());

  auto root_bounds = root->GetBoundsInRootWindow();
  root_bounds.set_height(root_bounds.height() /
                         DockedMagnifierController::kScreenHeightDivisor);
  return root_bounds;
}

// Returns the separator layer bounds from the given |viewport_bounds|. The
// separator layer is to be placed right below the viewport.
inline gfx::Rect SeparatorBoundsFromViewportBounds(
    const gfx::Rect& viewport_bounds) {
  return gfx::Rect(viewport_bounds.x(), viewport_bounds.bottom(),
                   viewport_bounds.width(),
                   DockedMagnifierController::kSeparatorHeight);
}

// Returns the angle by which the magnifier layer should be rotated such that
// the effect of the current screen rotation is undone, and the magnifier
// layer shows its contents in the correct rotation.
double GetMagnifierLayerRotationAngle(
    display::Display::Rotation current_display_active_rotation) {
  switch (current_display_active_rotation) {
    case display::Display::ROTATE_0:
      return 0.0;

    case display::Display::ROTATE_90:
      return -90.0;

    case display::Display::ROTATE_180:
      return -180.0;

    case display::Display::ROTATE_270:
      return -270.0;
  }

  NOTREACHED();
  return 0.0;
}

// Returns the child container in |root| that should be used as the parent of
// viewport widget and the separator layer.
aura::Window* GetViewportParentContainerForRoot(aura::Window* root) {
  return root->GetChildById(kShellWindowId_DockedMagnifierContainer);
}

}  // namespace

DockedMagnifierController::DockedMagnifierController() : binding_(this) {
  Shell::Get()->session_controller()->AddObserver(this);
}

DockedMagnifierController::~DockedMagnifierController() {
  Shell* shell = Shell::Get();
  shell->session_controller()->RemoveObserver(this);

  if (GetEnabled()) {
    shell->window_tree_host_manager()->RemoveObserver(this);
    shell->RemovePreTargetHandler(this);
  }
}

// static
void DockedMagnifierController::RegisterProfilePrefs(
    PrefRegistrySimple* registry,
    bool for_test) {
  if (for_test) {
    // In tests there is no remote pref service. Make ash own the prefs.
    registry->RegisterBooleanPref(prefs::kDockedMagnifierEnabled, false,
                                  PrefRegistry::PUBLIC);
  } else {
    // TODO(warx): move ownership to ash.
    registry->RegisterForeignPref(prefs::kDockedMagnifierEnabled);
  }
  registry->RegisterDoublePref(prefs::kDockedMagnifierScale,
                               kDefaultMagnifierScale, PrefRegistry::PUBLIC);
}

void DockedMagnifierController::BindRequest(
    mojom::DockedMagnifierControllerRequest request) {
  binding_.Bind(std::move(request));
}

bool DockedMagnifierController::GetEnabled() const {
  return active_user_pref_service_ &&
         active_user_pref_service_->GetBoolean(prefs::kDockedMagnifierEnabled);
}

float DockedMagnifierController::GetScale() const {
  if (active_user_pref_service_)
    return active_user_pref_service_->GetDouble(prefs::kDockedMagnifierScale);

  return kDefaultMagnifierScale;
}

void DockedMagnifierController::SetEnabled(bool enabled) {
  if (active_user_pref_service_) {
    active_user_pref_service_->SetBoolean(prefs::kDockedMagnifierEnabled,
                                          enabled);
  }
}

void DockedMagnifierController::SetScale(float scale) {
  if (active_user_pref_service_) {
    active_user_pref_service_->SetDouble(
        prefs::kDockedMagnifierScale,
        base::ClampToRange(scale, kMinMagnifierScale, kMaxMagnifierScale));
  }
}

void DockedMagnifierController::StepToNextScaleValue(int delta_index) {
  SetScale(magnifier_scale_utils::GetNextMagnifierScaleValue(
      delta_index, GetScale(), kMinMagnifierScale, kMaxMagnifierScale));
}

void DockedMagnifierController::CenterOnPoint(
    const gfx::Point& point_in_screen) {
  if (!GetEnabled())
    return;

  auto* screen = display::Screen::GetScreen();
  auto* window = screen->GetWindowAtScreenPoint(point_in_screen);
  if (!window) {
    // In tests and sometimes initially on signin screen, |point_in_screen|
    // maybe invalid and doesn't belong to any existing root window. However, we
    // are here because the Docked Magnifier is enabled. We need to create the
    // viewport widget somewhere, so we'll use the primary root window until we
    // get a valid cursor event.
    window = Shell::GetPrimaryRootWindow();
  }

  auto* root_window = window->GetRootWindow();
  DCHECK(root_window);
  SwitchCurrentSourceRootWindowIfNeeded(root_window,
                                        true /* update_old_root_workarea */);

  auto* host = root_window->GetHost();
  DCHECK(host);

  MaybeCachePointOfInterestMinimumHeight(host);

  gfx::Point point_of_interest(point_in_screen);
  ::wm::ConvertPointFromScreen(root_window, &point_of_interest);

  // The point of interest in pixels.
  gfx::PointF point_in_pixels(point_of_interest);

  // Before transforming to pixels, make sure its y-coordinate doesn't go below
  // the minimum height. Do it here for this PointF since the
  // |minimum_point_of_interest_height_| is a float, in order to avoid rounding
  // in the transform to be able to reliably verify it in tests.
  if (point_in_pixels.y() < minimum_point_of_interest_height_)
    point_in_pixels.set_y(minimum_point_of_interest_height_);

  // The pixel space is the magnified space.
  host->GetRootTransform().TransformPoint(&point_in_pixels);
  const float scale = GetScale();
  point_in_pixels.Scale(scale);

  // Transform steps: (Note that the transform is applied in the opposite order)
  // 1- Scale the layer by |scale|.
  // 2- Translate the point of interest back to the origin so that we can rotate
  //    around the Z-axis.
  // 3- Rotate around the Z-axis to undo the effect of screen rotation (if any).
  // 4- Translate the point of interest to the center point of the viewport
  //    widget.
  gfx::Transform transform;

  // 4- Translate to the center of the viewport widget.
  const gfx::Point viewport_center_point =
      GetViewportWidgetBoundsInRoot(current_source_root_window_).CenterPoint();
  transform.Translate(viewport_center_point.x(), viewport_center_point.y());

  // 3- Rotate around Z-axis. Account for a possibly rotated screen.
  const int64_t display_id =
      screen->GetDisplayNearestPoint(point_in_screen).id();
  DCHECK_NE(display_id, display::kInvalidDisplayId);
  const auto& display_info =
      Shell::Get()->display_manager()->GetDisplayInfo(display_id);
  transform.RotateAboutZAxis(
      GetMagnifierLayerRotationAngle(display_info.GetActiveRotation()));

  // 2- Translate back to origin.
  transform.Translate(-point_in_pixels.x(), -point_in_pixels.y());

  // 1- Scale.
  transform.Scale(scale, scale);

  // When updating the transform, we don't want any animation, otherwise the
  // movement of the mouse won't be very smooth. We want the magnifier layer to
  // update immediately with the movement of the mouse (or the change in the
  // point of interest due to input caret bounds changes ... etc.).
  ui::ScopedLayerAnimationSettings settings(
      viewport_magnifier_layer_->GetAnimator());
  settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(0));
  settings.SetTweenType(gfx::Tween::ZERO);
  settings.SetPreemptionStrategy(ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
  viewport_magnifier_layer_->SetTransform(transform);
}

void DockedMagnifierController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  active_user_pref_service_ = pref_service;
  InitFromUserPrefs();
}

void DockedMagnifierController::OnSigninScreenPrefServiceInitialized(
    PrefService* prefs) {
  OnActiveUserPrefServiceChanged(prefs);
}

void DockedMagnifierController::OnMouseEvent(ui::MouseEvent* event) {
  DCHECK(GetEnabled());
  CenterOnPoint(GetCursorScreenPoint());
}

void DockedMagnifierController::OnScrollEvent(ui::ScrollEvent* event) {
  DCHECK(GetEnabled());
  if (!event->IsAltDown() || !event->IsControlDown())
    return;

  if (event->type() == ui::ET_SCROLL_FLING_START ||
      event->type() == ui::ET_SCROLL_FLING_CANCEL) {
    event->StopPropagation();
    return;
  }

  if (event->type() == ui::ET_SCROLL) {
    // Notes: - Clamping of the new scale value happens inside SetScale().
    //        - Refreshing the viewport happens in the handler of the scale pref
    //          changes.
    SetScale(magnifier_scale_utils::GetScaleFromScroll(
        event->y_offset() * kScrollScaleFactor, GetScale(), kMaxMagnifierScale,
        kMinMagnifierScale));
    event->StopPropagation();
  }
}

void DockedMagnifierController::OnTouchEvent(ui::TouchEvent* event) {
  DCHECK(GetEnabled());

  aura::Window* target = static_cast<aura::Window*>(event->target());
  aura::Window* event_root = target->GetRootWindow();
  gfx::Point event_screen_point = event->root_location();
  ::wm::ConvertPointToScreen(event_root, &event_screen_point);
  CenterOnPoint(event_screen_point);
}

void DockedMagnifierController::OnCaretBoundsChanged(
    const ui::TextInputClient* client) {
  if (!GetEnabled()) {
    // There is a small window between the time the "enabled" pref is updated to
    // false, and the time we're notified with this change, upon which we remove
    // the magnifier's viewport widget and stop observing the input method.
    // During this short interval, if focus is in an editable element, the input
    // method can notify us here. In this case, we should just return.
    return;
  }

  aura::client::DragDropClient* drag_drop_client =
      aura::client::GetDragDropClient(current_source_root_window_);
  if (drag_drop_client && drag_drop_client->IsDragDropInProgress()) {
    // Ignore caret bounds change events when they result from changes in window
    // bounds due to dragging. This is to leave the viewport centered around the
    // cursor.
    return;
  }

  const gfx::Rect caret_screen_bounds = client->GetCaretBounds();
  // In many cases, espcially text input events coming from webpages, the caret
  // screen width is 0. Hence we can't check IsEmpty() since it returns true if
  // either of the width or height is 0. We want to abort if only both are 0s.
  if (!caret_screen_bounds.width() && !caret_screen_bounds.height())
    return;

  CenterOnPoint(caret_screen_bounds.CenterPoint());
}

void DockedMagnifierController::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(widget, viewport_widget_);

  SwitchCurrentSourceRootWindowIfNeeded(nullptr,
                                        false /* update_old_root_workarea */);
}

void DockedMagnifierController::OnDisplayConfigurationChanged() {
  DCHECK(GetEnabled());

  // The viewport might have been on a display that just got removed, and hence
  // the viewport widget and its associated layers are already destroyed. In
  // that case we also cleared the |current_source_root_window_|.
  if (current_source_root_window_) {
    // Resolution may have changed. Update all bounds.
    const auto viewport_bounds =
        GetViewportWidgetBoundsInRoot(current_source_root_window_);
    viewport_widget_->SetBounds(viewport_bounds);
    viewport_background_layer_->SetBounds(viewport_bounds);
    viewport_magnifier_layer_->SetBounds(viewport_bounds);
    separator_layer_->SetBounds(
        SeparatorBoundsFromViewportBounds(viewport_bounds));
    SetViewportHeightInWorkArea(current_source_root_window_,
                                separator_layer_->bounds().bottom());

    // Resolution changes, screen rotation, etc. can reset the host to confine
    // the mouse cursor inside the root window. We want to make sure the cursor
    // is confined properly outside the viewport.
    ConfineMouseCursorOutsideViewport();
  }

  // A change in display configuration, such as resolution, rotation, ... etc.
  // invalidates the currently cached minimum height of the point of interest.
  is_minimum_point_of_interest_height_valid_ = false;

  // Update the viewport magnifier layer transform.
  CenterOnPoint(GetCursorScreenPoint());
}

bool DockedMagnifierController::GetFullscreenMagnifierEnabled() const {
  return active_user_pref_service_ &&
         active_user_pref_service_->GetBoolean(
             prefs::kAccessibilityScreenMagnifierEnabled);
}

void DockedMagnifierController::SetFullscreenMagnifierEnabled(bool enabled) {
  if (active_user_pref_service_) {
    active_user_pref_service_->SetBoolean(
        prefs::kAccessibilityScreenMagnifierEnabled, enabled);
  }
}

const views::Widget* DockedMagnifierController::GetViewportWidgetForTesting()
    const {
  return viewport_widget_;
}

const ui::Layer*
DockedMagnifierController::GetViewportMagnifierLayerForTesting() const {
  return viewport_magnifier_layer_.get();
}

float DockedMagnifierController::GetMinimumPointOfInterestHeightForTesting()
    const {
  return minimum_point_of_interest_height_;
}

void DockedMagnifierController::SwitchCurrentSourceRootWindowIfNeeded(
    aura::Window* new_root_window,
    bool update_old_root_workarea) {
  if (current_source_root_window_ == new_root_window)
    return;

  aura::Window* old_root_window = current_source_root_window_;
  current_source_root_window_ = new_root_window;

  // Current window changes means the minimum height of the point of interest is
  // no longer valid.
  is_minimum_point_of_interest_height_valid_ = false;

  if (old_root_window) {
    if (separator_layer_) {
      GetViewportParentContainerForRoot(old_root_window)
          ->layer()
          ->Remove(separator_layer_.get());
    }
    if (update_old_root_workarea)
      SetViewportHeightInWorkArea(old_root_window, 0);
    ui::InputMethod* old_input_method =
        GetInputMethodForWindow(old_root_window);
    if (old_input_method)
      old_input_method->RemoveObserver(this);

    // Reset mouse cursor confinement to default.
    RootWindowController::ForWindow(old_root_window)
        ->ash_host()
        ->ConfineCursorToRootWindow();
  }

  // A change in the current root window means we must clear the existing
  // reflector and the viewport widget and its layers. New viewport and
  // reflector may be recreated later if |new_root_window| is not |nullptr|.
  if (reflector_) {
    Shell::Get()->aura_env()->context_factory_private()->RemoveReflector(
        reflector_.get());
    reflector_.reset();
  }

  separator_layer_ = nullptr;

  if (viewport_widget_) {
    viewport_widget_->RemoveObserver(this);
    viewport_widget_->Close();
    viewport_widget_ = nullptr;
  }

  viewport_background_layer_ = nullptr;
  viewport_magnifier_layer_ = nullptr;

  if (!current_source_root_window_) {
    // No need to create a new magnifier viewport.
    return;
  }

  CreateMagnifierViewport();

  ui::InputMethod* new_input_method =
      GetInputMethodForWindow(current_source_root_window_);
  if (new_input_method)
    new_input_method->AddObserver(this);

  DCHECK(Shell::Get()->aura_env()->context_factory_private());
  DCHECK(viewport_widget_);
  reflector_ =
      Shell::Get()->aura_env()->context_factory_private()->CreateReflector(
          current_source_root_window_->layer()->GetCompositor(),
          viewport_magnifier_layer_.get());
}

void DockedMagnifierController::InitFromUserPrefs() {
  DCHECK(active_user_pref_service_);

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(active_user_pref_service_);
  pref_change_registrar_->Add(
      prefs::kDockedMagnifierEnabled,
      base::BindRepeating(&DockedMagnifierController::OnEnabledPrefChanged,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kDockedMagnifierScale,
      base::BindRepeating(&DockedMagnifierController::OnScalePrefChanged,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityScreenMagnifierEnabled,
      base::BindRepeating(
          &DockedMagnifierController::OnFullscreenMagnifierEnabledPrefChanged,
          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityHighContrastEnabled,
      base::BindRepeating(
          &DockedMagnifierController::OnHighContrastEnabledPrefChanged,
          base::Unretained(this)));

  OnEnabledPrefChanged();
}

void DockedMagnifierController::OnEnabledPrefChanged() {
  // When switching from the signin screen to a newly created profile while the
  // Docked Magnifier is enabled, the prefs will be copied from the signin
  // profile to the user profile, and the Docked Magnifier will remain enabled.
  // We don't want to redo the below operations if the status doesn't change,
  // for example readding the same observer to the WindowTreeHostManager will
  // cause a crash on DCHECK on debug builds.
  const bool current_enabled = !!current_source_root_window_;
  const bool new_enabled = GetEnabled();
  if (current_enabled == new_enabled)
    return;

  // Toggling the status of the docked magnifier, changes the display's work
  // area. However, display's work area changes are not allowed while overview
  // mode is active (See https://crbug.com/834400). For this reason, we exit
  // overview mode, before we actually update the state of docked magnifier
  // below. https://crbug.com/894256.
  Shell* shell = Shell::Get();
  auto* window_selector_controller = shell->window_selector_controller();
  if (window_selector_controller->IsSelecting()) {
    auto* split_view_controller = shell->split_view_controller();
    if (split_view_controller->IsSplitViewModeActive()) {
      // In this case, we're in a single-split-view mode, i.e. a window is
      // snapped to one side of the split view, while the other side has the
      // window selector active.
      // We need to exit split view as well as exiting overview mode, otherwise
      // we'll be in an invalid state.
      split_view_controller->EndSplitView(
          SplitViewController::EndReason::kNormal);
    }

    window_selector_controller->ToggleOverview();
  }

  if (new_enabled) {
    // Enabling the Docked Magnifier disables the Fullscreen Magnifier.
    SetFullscreenMagnifierEnabled(false);
    // Calling refresh will result in the creation of the magnifier viewport and
    // its associated layers.
    Refresh();
    // Make sure we are in front of the fullscreen magnifier which also handles
    // scroll events.
    shell->AddPreTargetHandler(this, ui::EventTarget::Priority::kSystem);
    shell->window_tree_host_manager()->AddObserver(this);
  } else {
    shell->window_tree_host_manager()->RemoveObserver(this);
    shell->RemovePreTargetHandler(this);

    // Setting the current root window to |nullptr| will remove the viewport and
    // all its associated layers.
    SwitchCurrentSourceRootWindowIfNeeded(nullptr,
                                          true /* update_old_root_workarea */);
  }

  // Update the green checkmark status in the accessibility menu in the system
  // tray.
  shell->accessibility_controller()->NotifyAccessibilityStatusChanged();

  // We use software composited mouse cursor so that it can be mirrored into the
  // magnifier viewport.
  shell->UpdateCursorCompositingEnabled();
}

void DockedMagnifierController::OnScalePrefChanged() {
  if (GetEnabled()) {
    // Invalidate the cached minimum height of the point of interest since the
    // change in scale changes that height.
    is_minimum_point_of_interest_height_valid_ = false;
    Refresh();
  }
}

void DockedMagnifierController::OnFullscreenMagnifierEnabledPrefChanged() {
  // Enabling the Fullscreen Magnifier disables the Docked Magnifier.
  if (GetFullscreenMagnifierEnabled())
    SetEnabled(false);
}

void DockedMagnifierController::OnHighContrastEnabledPrefChanged() {
  if (!GetEnabled())
    return;

  viewport_magnifier_layer_->SetLayerInverted(IsHighContrastEnabled());
}

void DockedMagnifierController::Refresh() {
  DCHECK(GetEnabled());
  CenterOnPoint(GetCursorScreenPoint());
}

void DockedMagnifierController::CreateMagnifierViewport() {
  DCHECK(GetEnabled());
  DCHECK(current_source_root_window_);
  DCHECK(!reflector_);

  const auto viewport_bounds =
      GetViewportWidgetBoundsInRoot(current_source_root_window_);

  // 1- Create the viewport widget.
  viewport_widget_ = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.accept_events = false;
  params.bounds = viewport_bounds;
  params.opacity = views::Widget::InitParams::OPAQUE_WINDOW;
  params.parent =
      GetViewportParentContainerForRoot(current_source_root_window_);
  params.name = kDockedMagnifierViewportWindowName;
  viewport_widget_->Init(params);
  viewport_widget_->Show();
  viewport_widget_->AddObserver(this);

  // 2- Create the separator layer right below the viwport widget, parented to
  //    the layer of the root window.
  separator_layer_ = std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
  separator_layer_->SetColor(SK_ColorBLACK);
  separator_layer_->SetBounds(
      SeparatorBoundsFromViewportBounds(viewport_bounds));
  params.parent->layer()->Add(separator_layer_.get());

  // 3- Create a background layer that will show a dark gray color behind the
  //    magnifier layer. It has the same bounds as the viewport.
  viewport_background_layer_ =
      std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
  viewport_background_layer_->SetColor(SK_ColorDKGRAY);
  viewport_background_layer_->SetBounds(viewport_bounds);
  aura::Window* viewport_window = viewport_widget_->GetNativeView();
  ui::Layer* viewport_layer = viewport_window->layer();
  viewport_layer->Add(viewport_background_layer_.get());

  // 4- Create the layer in which the contents of the screen will be mirrored
  //    and magnified.
  viewport_magnifier_layer_ =
      std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
  viewport_magnifier_layer_->SetLayerInverted(IsHighContrastEnabled());
  viewport_layer->Add(viewport_magnifier_layer_.get());
  viewport_layer->SetMasksToBounds(true);

  // 5- Update the workarea of the current screen such that an area enough to
  //    contain the viewport and the separator is allocated at the top of the
  //    screen.
  SetViewportHeightInWorkArea(current_source_root_window_,
                              separator_layer_->bounds().bottom());

  // 6- Confine the mouse cursor within the remaining part of the display.
  ConfineMouseCursorOutsideViewport();
}

void DockedMagnifierController::MaybeCachePointOfInterestMinimumHeight(
    aura::WindowTreeHost* host) {
  DCHECK(GetEnabled());
  DCHECK(current_source_root_window_);
  DCHECK(host);

  if (is_minimum_point_of_interest_height_valid_)
    return;

  // Adjust the point of interest so that we don't end up magnifying the
  // magnifier. This means we don't allow the point of interest to go beyond a
  // minimum y-coordinate value. Here's how we find that minimum value:
  //
  // +-----------------+     +-----------------------------------+
  // |     Viewport    |     |                                   |
  // +====separator====+     |        Magnified Viewport         |
  // |                (+) b  |                                   |
  // |                 |     +==============separator===========(+) A
  // |                 |     |                  Distance (D) --> |
  // |                 |     |                                  (+) B
  // +-----------------+     |                                   |
  //    Screen in Non        |                                   |
  //   Magnified Space       |                                   |
  //                         |                                   |
  //                         |                                   |
  //                         +-----------------------------------+
  //                               Screen in Magnified Space
  //                     (the contents of |viewport_magnifier_layer_|)
  //
  // Problem: Find the height of the point of interest (b) in the non-magnified
  //          coordinates space, which corresponds to the height of point (B) in
  //          the magnified coordinates space, such that when point (A) is
  //          translated from the magnified coordinates space to the non-
  //          magnified coordinates space, its y coordinate is 0 (i.e. aligns
  //          with the top of the magnifier viewport).
  //
  // 1- The height of Point (A) in the magnified space is the bottom of the
  //    entire magnifier (which is actually the bottom of the separator) in the
  //    magnified coordinates space.
  //    Note that the magnified space is in pixels. This point should be
  //    translated such that its y-coordiante is not greater than 0 (in the non-
  //    magnified coordinates space), otherwise the magnifier will magnify and
  //    mirror itself.
  // 2- Point (B) is the scaled point of interest in the magnified space. The
  //    point of interest is always translated to the center point of the
  //    viewport. Hence, if point (A) goes to y = 0, and point (B) goes to a
  //    height equals to the height of the center point of the viewport,
  //    therefore means distance (D) = viewport_center_point.y().
  // 3- Now that we found the height of point (B) in the magnified space,
  //    find the the height of point (b) which is the corresponding height in
  //    the non-magnified space. This height is the minimum height below which
  //    the point of interest may not go.

  const gfx::Rect viewport_bounds =
      GetViewportWidgetBoundsInRoot(current_source_root_window_);

  // 1- Point (A)'s height.
  // Note we use a Vector3dF to actually represent a 2D point. The reason is
  // Vector3dF provides an API to get the Length() of the vector without
  // converting the object to another temporary object. We need to get the
  // Length() rather than y() because screen rotation transform can make the
  // height we are interested in either x() or y() depending on the rotation
  // angle, so we just simply use Length().
  // Note: Why transform the point to the magnified scale and back? The reason
  // is that we need to go through the root window transform to go to the pixel
  // space (the reflector copies pixels). This will account for device scale
  // factors, screen rotations, and any other transforms that we cannot
  // anticipate ourselves.
  gfx::Vector3dF scaled_magnifier_bottom_in_pixels(
      0.0f, viewport_bounds.bottom() + kSeparatorHeight, 0.0f);
  host->GetRootTransform().TransformVector(&scaled_magnifier_bottom_in_pixels);
  const float scale = GetScale();
  scaled_magnifier_bottom_in_pixels.Scale(scale);

  // 2- Point (B)'s height.
  const gfx::PointF viewport_center_point(viewport_bounds.CenterPoint());
  gfx::Vector3dF minimum_height_vector(
      0.0f,
      viewport_center_point.y() + scaled_magnifier_bottom_in_pixels.Length(),
      0.0f);

  // 3- Back to non-magnified space to get point (b)'s height.
  minimum_height_vector.Scale(1 / scale);
  host->GetInverseRootTransform().TransformVector(&minimum_height_vector);
  minimum_point_of_interest_height_ = minimum_height_vector.Length();
  is_minimum_point_of_interest_height_valid_ = true;
}

void DockedMagnifierController::ConfineMouseCursorOutsideViewport() {
  DCHECK(current_source_root_window_);

  gfx::Rect confine_bounds =
      current_source_root_window_->GetBoundsInRootWindow();
  const int docked_height = separator_layer_->bounds().bottom();
  confine_bounds.Offset(0, docked_height);
  confine_bounds.set_height(confine_bounds.height() - docked_height);
  RootWindowController::ForWindow(current_source_root_window_)
      ->ash_host()
      ->ConfineCursorToBoundsInRoot(confine_bounds);
}

}  // namespace ash
