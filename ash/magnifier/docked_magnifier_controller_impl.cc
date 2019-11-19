// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/magnifier/docked_magnifier_controller_impl.h"

#include <algorithm>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/magnifier/magnifier_utils.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/work_area_insets.h"
#include "base/bind.h"
#include "base/numerics/ranges.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/viz/common/features.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/ime_bridge.h"
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

// Returns the current cursor location in screen coordinates.
inline gfx::Point GetCursorScreenPoint() {
  return display::Screen::GetScreen()->GetCursorScreenPoint();
}

// Updates the workarea of the display associated with |window| such that the
// given magnifier viewport |height| is allocated at the top of the screen.
void SetViewportHeightInWorkArea(aura::Window* window, int height) {
  DCHECK(window);
  WorkAreaInsets::ForWindow(window->GetRootWindow())
      ->SetDockedMagnifierHeight(height);
}

// Gets the bounds of the Docked Magnifier viewport widget when placed in the
// display whose root window is |root|. The bounds returned correspond to the
// top quarter portion of the screen.
gfx::Rect GetViewportWidgetBoundsInRoot(aura::Window* root) {
  DCHECK(root);
  DCHECK(root->IsRootWindow());

  auto root_bounds = root->GetBoundsInRootWindow();
  root_bounds.set_height(root_bounds.height() /
                         DockedMagnifierControllerImpl::kScreenHeightDivisor);
  return root_bounds;
}

// Returns the separator layer bounds from the given |viewport_bounds|. The
// separator layer is to be placed right below the viewport.
inline gfx::Rect SeparatorBoundsFromViewportBounds(
    const gfx::Rect& viewport_bounds) {
  return gfx::Rect(viewport_bounds.x(), viewport_bounds.bottom(),
                   viewport_bounds.width(),
                   DockedMagnifierControllerImpl::kSeparatorHeight);
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

// Returns true if the docked magnifier should use layer mirroring rather than
// ui::Reflector. Layer mirroring is used when OOP-D is enabled.
bool ShouldUseLayerMirroring() {
  return features::IsVizDisplayCompositorEnabled();
}

}  // namespace

// static
DockedMagnifierController* DockedMagnifierController::Get() {
  return Shell::Get()->docked_magnifier_controller();
}

DockedMagnifierControllerImpl::DockedMagnifierControllerImpl() {
  Shell::Get()->session_controller()->AddObserver(this);
  if (ui::IMEBridge::Get())
    ui::IMEBridge::Get()->AddObserver(this);
}

DockedMagnifierControllerImpl::~DockedMagnifierControllerImpl() {
  if (input_method_)
    input_method_->RemoveObserver(this);
  input_method_ = nullptr;
  if (ui::IMEBridge::Get())
    ui::IMEBridge::Get()->RemoveObserver(this);

  Shell* shell = Shell::Get();
  shell->session_controller()->RemoveObserver(this);

  if (GetEnabled()) {
    shell->window_tree_host_manager()->RemoveObserver(this);
    shell->RemovePreTargetHandler(this);
  }
}

// static
void DockedMagnifierControllerImpl::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kDockedMagnifierEnabled, false);
  registry->RegisterDoublePref(prefs::kDockedMagnifierScale,
                               kDefaultMagnifierScale);
}

bool DockedMagnifierControllerImpl::GetEnabled() const {
  return active_user_pref_service_ &&
         active_user_pref_service_->GetBoolean(prefs::kDockedMagnifierEnabled);
}

float DockedMagnifierControllerImpl::GetScale() const {
  if (active_user_pref_service_)
    return active_user_pref_service_->GetDouble(prefs::kDockedMagnifierScale);

  return kDefaultMagnifierScale;
}

void DockedMagnifierControllerImpl::SetEnabled(bool enabled) {
  if (active_user_pref_service_) {
    active_user_pref_service_->SetBoolean(prefs::kDockedMagnifierEnabled,
                                          enabled);
  }
}

void DockedMagnifierControllerImpl::SetScale(float scale) {
  if (active_user_pref_service_) {
    active_user_pref_service_->SetDouble(
        prefs::kDockedMagnifierScale,
        base::ClampToRange(scale, kMinMagnifierScale, kMaxMagnifierScale));
  }
}

void DockedMagnifierControllerImpl::StepToNextScaleValue(int delta_index) {
  SetScale(magnifier_utils::GetNextMagnifierScaleValue(
      delta_index, GetScale(), kMinMagnifierScale, kMaxMagnifierScale));
}

void DockedMagnifierControllerImpl::CenterOnPoint(
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
  if (!ShouldUseLayerMirroring()) {
    // When ui::Reflector is used, the texture has the root transform applied to
    // it. The same transform should be applied to the |point_in_pixels| to map
    // it to the corresponding location in the texture. This is not the case
    // when layer mirroring is used.
    host->GetRootTransform().TransformPoint(&point_in_pixels);
  }
  const float scale = GetScale();
  point_in_pixels.Scale(scale);

  gfx::Transform transform;
  if (ShouldUseLayerMirroring()) {
    // When layer mirroring is used, the mirrored content is not rotated around
    // Z-axis; so, there is no need to compnesate for it.
    // Transform steps: (Note that the transform is applied in the opposite
    // order)
    // 1- Scale the layer by |scale|.
    // 2- Translate the point of interest to the center point of the viewport
    //    widget.

    // 2- Translate to the center of the viewport widget.
    const gfx::Point viewport_center_point =
        GetViewportWidgetBoundsInRoot(current_source_root_window_)
            .CenterPoint();
    transform.Translate(viewport_center_point.x() - point_in_pixels.x(),
                        viewport_center_point.y() - point_in_pixels.y());

    // 1- Scale.
    transform.Scale(scale, scale);
  } else {
    // When ui::Reflector is used, the mirrored content is rotated around
    // Z-axis; so, we need to compnesate for it.
    // Transform steps: (Note that the transform is applied in the opposite
    // order)
    // 1- Scale the layer by |scale|.
    // 2- Translate the point of interest back to the origin so that we can
    //    rotate around the Z-axis.
    // 3- Rotate around the Z-axis to undo the effect of screen rotation (if
    //    any).
    // 4- Translate the point of interest to the center point of the viewport
    //    widget.

    // 4- Translate to the center of the viewport widget.
    const gfx::Point viewport_center_point =
        GetViewportWidgetBoundsInRoot(current_source_root_window_)
            .CenterPoint();
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
  }

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

int DockedMagnifierControllerImpl::GetMagnifierHeightForTesting() const {
  return GetTotalMagnifierHeight();
}

void DockedMagnifierControllerImpl::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  active_user_pref_service_ = pref_service;
  InitFromUserPrefs();
}

void DockedMagnifierControllerImpl::OnSigninScreenPrefServiceInitialized(
    PrefService* prefs) {
  OnActiveUserPrefServiceChanged(prefs);
}

void DockedMagnifierControllerImpl::OnMouseEvent(ui::MouseEvent* event) {
  DCHECK(GetEnabled());
  CenterOnPoint(GetCursorScreenPoint());
}

void DockedMagnifierControllerImpl::OnScrollEvent(ui::ScrollEvent* event) {
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
    SetScale(magnifier_utils::GetScaleFromScroll(
        event->y_offset() * kScrollScaleFactor, GetScale(), kMaxMagnifierScale,
        kMinMagnifierScale));
    event->StopPropagation();
  }
}

void DockedMagnifierControllerImpl::OnTouchEvent(ui::TouchEvent* event) {
  DCHECK(GetEnabled());

  aura::Window* target = static_cast<aura::Window*>(event->target());
  aura::Window* event_root = target->GetRootWindow();
  gfx::Point event_screen_point = event->root_location();
  ::wm::ConvertPointToScreen(event_root, &event_screen_point);
  CenterOnPoint(event_screen_point);
}

void DockedMagnifierControllerImpl::OnInputContextHandlerChanged() {
  if (!GetEnabled())
    return;

  auto* new_input_method =
      magnifier_utils::GetInputMethod(current_source_root_window_);
  if (new_input_method == input_method_)
    return;

  if (input_method_)
    input_method_->RemoveObserver(this);
  input_method_ = new_input_method;
  if (input_method_)
    input_method_->AddObserver(this);
}

void DockedMagnifierControllerImpl::OnInputMethodDestroyed(
    const ui::InputMethod* input_method) {
  DCHECK_EQ(input_method, input_method_);
  input_method_->RemoveObserver(this);
  input_method_ = nullptr;
}

void DockedMagnifierControllerImpl::OnCaretBoundsChanged(
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

void DockedMagnifierControllerImpl::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(widget, viewport_widget_);

  SwitchCurrentSourceRootWindowIfNeeded(nullptr,
                                        false /* update_old_root_workarea */);
}

void DockedMagnifierControllerImpl::OnDisplayConfigurationChanged() {
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
    if (!ShouldUseLayerMirroring()) {
      // In case of layer mirroring, |viewport_magnifier_layer_| automatically
      // matches size of mirrored layer and there is no need to set its bounds
      // here.
      viewport_magnifier_layer_->SetBounds(viewport_bounds);
    }
    separator_layer_->SetBounds(
        SeparatorBoundsFromViewportBounds(viewport_bounds));
    SetViewportHeightInWorkArea(current_source_root_window_,
                                GetTotalMagnifierHeight());

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

bool DockedMagnifierControllerImpl::GetFullscreenMagnifierEnabled() const {
  return active_user_pref_service_ &&
         active_user_pref_service_->GetBoolean(
             prefs::kAccessibilityScreenMagnifierEnabled);
}

void DockedMagnifierControllerImpl::SetFullscreenMagnifierEnabled(
    bool enabled) {
  if (active_user_pref_service_) {
    active_user_pref_service_->SetBoolean(
        prefs::kAccessibilityScreenMagnifierEnabled, enabled);
  }
}

int DockedMagnifierControllerImpl::GetTotalMagnifierHeight() const {
  if (separator_layer_)
    return separator_layer_->bounds().bottom();

  return 0;
}

const views::Widget*
DockedMagnifierControllerImpl::GetViewportWidgetForTesting() const {
  return viewport_widget_;
}

const ui::Layer*
DockedMagnifierControllerImpl::GetViewportMagnifierLayerForTesting() const {
  return viewport_magnifier_layer_.get();
}

float DockedMagnifierControllerImpl::GetMinimumPointOfInterestHeightForTesting()
    const {
  return minimum_point_of_interest_height_;
}

void DockedMagnifierControllerImpl::SwitchCurrentSourceRootWindowIfNeeded(
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
    // Order here matters. We should stop observing caret bounds changes before
    // updating the work area bounds of the old root window. Otherwise, work
    // area bounds changes will lead to caret bounds changes that recurses back
    // here unnecessarily. https://crbug.com/1000903.
    if (input_method_)
      input_method_->RemoveObserver(this);
    input_method_ = nullptr;

    if (update_old_root_workarea)
      SetViewportHeightInWorkArea(old_root_window, 0);

    // Reset mouse cursor confinement to default.
    RootWindowController::ForWindow(old_root_window)
        ->ash_host()
        ->ConfineCursorToRootWindow();
  }

  // A change in the current root window means we must clear the existing
  // reflector and the viewport widget and its layers. New viewport and
  // reflector may be recreated later if |new_root_window| is not |nullptr|.
  if (reflector_) {
    aura::Env::GetInstance()->context_factory_private()->RemoveReflector(
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

  input_method_ = magnifier_utils::GetInputMethod(current_source_root_window_);
  if (input_method_)
    input_method_->AddObserver(this);

  DCHECK(aura::Env::GetInstance()->context_factory_private());
  DCHECK(viewport_widget_);
  if (ShouldUseLayerMirroring()) {
    auto* magnified_container = current_source_root_window_->GetChildById(
        kShellWindowId_MagnifiedContainer);
    viewport_magnifier_layer_->SetShowReflectedLayerSubtree(
        magnified_container->layer());
  } else {
    reflector_ =
        aura::Env::GetInstance()->context_factory_private()->CreateReflector(
            current_source_root_window_->layer()->GetCompositor(),
            viewport_magnifier_layer_.get());
  }
}

void DockedMagnifierControllerImpl::InitFromUserPrefs() {
  DCHECK(active_user_pref_service_);

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(active_user_pref_service_);
  pref_change_registrar_->Add(
      prefs::kDockedMagnifierEnabled,
      base::BindRepeating(&DockedMagnifierControllerImpl::OnEnabledPrefChanged,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kDockedMagnifierScale,
      base::BindRepeating(&DockedMagnifierControllerImpl::OnScalePrefChanged,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kAccessibilityScreenMagnifierEnabled,
      base::BindRepeating(&DockedMagnifierControllerImpl::
                              OnFullscreenMagnifierEnabledPrefChanged,
                          base::Unretained(this)));
  if (!ShouldUseLayerMirroring()) {
    // When ui::Reflector is used and high contrast is enabled, the reflected
    // texture is already inverted and will be inverted once more because the
    // root window is set to be inverted, undoing the original inversion. To
    // prevent that, observe changes to the high contrast mode and invert the
    // magnifier layer one more time.  Layer mirroring mode does not have this
    // issue.
    pref_change_registrar_->Add(
        prefs::kAccessibilityHighContrastEnabled,
        base::BindRepeating(
            &DockedMagnifierControllerImpl::OnHighContrastEnabledPrefChanged,
            base::Unretained(this)));
  }

  OnEnabledPrefChanged();
}

void DockedMagnifierControllerImpl::OnEnabledPrefChanged() {
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
  auto* overview_controller = shell->overview_controller();
  if (overview_controller->InOverviewSession()) {
    // |OverviewController::EndOverview| fails (returning false) in certain
    // cases involving tablet split view mode. We can guarantee success by
    // ensuring that tablet split view mode is not in session.
    auto* split_view_controller =
        SplitViewController::Get(Shell::GetPrimaryRootWindow());
    if (split_view_controller->InTabletSplitViewMode()) {
      split_view_controller->EndSplitView(
          SplitViewController::EndReason::kNormal);
    }
    overview_controller->EndOverview();
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

void DockedMagnifierControllerImpl::OnScalePrefChanged() {
  if (GetEnabled()) {
    // Invalidate the cached minimum height of the point of interest since the
    // change in scale changes that height.
    is_minimum_point_of_interest_height_valid_ = false;
    Refresh();
  }
}

void DockedMagnifierControllerImpl::OnFullscreenMagnifierEnabledPrefChanged() {
  // Enabling the Fullscreen Magnifier disables the Docked Magnifier.
  if (GetFullscreenMagnifierEnabled())
    SetEnabled(false);
}

void DockedMagnifierControllerImpl::OnHighContrastEnabledPrefChanged() {
  DCHECK(!ShouldUseLayerMirroring());

  if (!GetEnabled())
    return;

  viewport_magnifier_layer_->SetLayerInverted(
      Shell::Get()->accessibility_controller()->high_contrast_enabled());
}

void DockedMagnifierControllerImpl::Refresh() {
  DCHECK(GetEnabled());
  CenterOnPoint(GetCursorScreenPoint());
}

void DockedMagnifierControllerImpl::CreateMagnifierViewport() {
  DCHECK(GetEnabled());
  DCHECK(current_source_root_window_);
  DCHECK(ShouldUseLayerMirroring() || !reflector_);

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
  viewport_widget_->Init(std::move(params));

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
  // There are situations that the content rect for the magnified container gets
  // larger than its bounds (e.g. shelf stretches beyond the screen to allow it
  // being dragged up, or contents of mouse pointer might go beyond screen when
  // the pointer is at the edges of the screen). To avoid this extra content
  // becoming visible in the magnifier, magnifier layer should clip its contents
  // to its bounds.
  viewport_magnifier_layer_->SetMasksToBounds(true);
  viewport_layer->Add(viewport_magnifier_layer_.get());
  viewport_layer->SetMasksToBounds(true);

  // In case of ui::Reflector, handle high contrast mode.
  if (!ShouldUseLayerMirroring())
    OnHighContrastEnabledPrefChanged();

  // 5- Update the workarea of the current screen such that an area enough to
  //    contain the viewport and the separator is allocated at the top of the
  //    screen.
  SetViewportHeightInWorkArea(current_source_root_window_,
                              GetTotalMagnifierHeight());

  // 6- Confine the mouse cursor within the remaining part of the display.
  ConfineMouseCursorOutsideViewport();

  // 7- Show the widget, which can trigger events to request movement of the
  // viewport now that all internal state has been created.
  viewport_widget_->AddObserver(this);
  viewport_widget_->Show();
}

void DockedMagnifierControllerImpl::MaybeCachePointOfInterestMinimumHeight(
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
  if (!ShouldUseLayerMirroring()) {
    // For ui::Reflector, the reflected texture has root transform applied to
    // it. Apply the same transform to map the point to its corresponding
    // location on the reflected texture.
    host->GetRootTransform().TransformVector(
        &scaled_magnifier_bottom_in_pixels);
  }
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
  if (!ShouldUseLayerMirroring()) {
    // For ui::Reflector, apply the reverse root transform to undo the transform
    // applied earlier.
    host->GetInverseRootTransform().TransformVector(&minimum_height_vector);
  }
  minimum_point_of_interest_height_ = minimum_height_vector.Length();
  is_minimum_point_of_interest_height_valid_ = true;
}

void DockedMagnifierControllerImpl::ConfineMouseCursorOutsideViewport() {
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
