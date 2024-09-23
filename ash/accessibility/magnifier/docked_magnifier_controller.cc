// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/magnifier/docked_magnifier_controller.h"

#include <algorithm>
#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/magnifier/magnifier_utils.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/display/cursor_window_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/work_area_insets.h"
#include "base/functional/bind.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// Default, minimum, and maximum magnifier scale values (magnification levels).
constexpr float kDefaultMagnifierScale = 4.0f;
constexpr float kMinMagnifierScale = 1.0f;
constexpr float kMaxMagnifierScale = 20.0f;

// Minimum and maximum screen height divisors. These correspond to the tallest
// and shortest allowed docked magnifier viewport heights, as
// viewport_height = root_bounds.height() / screen_height_divisor.
constexpr float kMinScreenHeightDivisor = 5.0f / 4.0f;
constexpr float kMaxScreenHeightDivisor = 8.0f;

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

// Returns the separator layer bounds from the given |viewport_bounds|. The
// separator layer is to be placed right below the viewport.
inline gfx::Rect SeparatorBoundsFromViewportBounds(
    const gfx::Rect& viewport_bounds) {
  return gfx::Rect(viewport_bounds.x(), viewport_bounds.bottom(),
                   viewport_bounds.width(),
                   DockedMagnifierController::kSeparatorHeight);
}

// Returns the child container in |root| that should be used as the parent of
// viewport widget.
aura::Window* GetViewportParentContainerForRoot(aura::Window* root) {
  return root->GetChildById(kShellWindowId_DockedMagnifierContainer);
}

// Returns the child container in |root| that should be used as the parent of
// the separator layer.
aura::Window* GetViewportParentContainerForDivider(aura::Window* root) {
  return root->GetChildById(kShellWindowId_OverlayContainer);
}

}  // namespace

// static
DockedMagnifierController::DockedMagnifierController() {
  Shell::Get()->session_controller()->AddObserver(this);
}

DockedMagnifierController::~DockedMagnifierController() {
  Shell* shell = Shell::Get();
  shell->session_controller()->RemoveObserver(this);

  if (GetEnabled()) {
    shell->display_manager()->RemoveDisplayManagerObserver(this);
    shell->RemovePreTargetHandler(this);
  }
  CHECK(!views::WidgetObserver::IsInObserverList());
}

// static
void DockedMagnifierController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kDockedMagnifierEnabled, false);
  registry->RegisterDoublePref(prefs::kDockedMagnifierScale,
                               kDefaultMagnifierScale);
  registry->RegisterDoublePref(prefs::kDockedMagnifierScreenHeightDivisor,
                               kDefaultScreenHeightDivisor);
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

float DockedMagnifierController::GetScreenHeightDivisor() const {
  if (active_user_pref_service_) {
    return active_user_pref_service_->GetDouble(
        prefs::kDockedMagnifierScreenHeightDivisor);
  }

  return kDefaultScreenHeightDivisor;
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
        std::clamp(scale, kMinMagnifierScale, kMaxMagnifierScale));
  }
}

void DockedMagnifierController::SetScreenHeightDivisor(
    float screen_height_divisor) {
  if (active_user_pref_service_) {
    active_user_pref_service_->SetDouble(
        prefs::kDockedMagnifierScreenHeightDivisor,
        std::clamp(screen_height_divisor, kMinScreenHeightDivisor,
                   kMaxScreenHeightDivisor));
  }
}

void DockedMagnifierController::StepToNextScaleValue(int delta_index) {
  SetScale(magnifier_utils::GetNextMagnifierScaleValue(
      delta_index, GetScale(), kMinMagnifierScale, kMaxMagnifierScale));
}

void DockedMagnifierController::MoveMagnifierToRect(
    const gfx::Rect& rect_in_screen) {
  DCHECK(GetEnabled());
  gfx::Point point_in_screen = rect_in_screen.CenterPoint();

  // If rect is too wide to fit in viewport, include as much as we can, starting
  // with the left edge.
  const int scaled_viewport_width =
      current_source_root_window_->bounds().width() / GetScale();
  if (rect_in_screen.width() > scaled_viewport_width) {
    point_in_screen.set_x(std::max(rect_in_screen.x() +
                                       scaled_viewport_width / 2 -
                                       magnifier_utils::kLeftEdgeContextPadding,
                                   0));
  }

  CenterOnPoint(point_in_screen);
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
  const float scale = GetScale();
  point_in_pixels.Scale(scale);

  // Transform steps: (Note that the transform is applied in the opposite
  // order)
  // 1- Scale the layer by |scale|.
  // 2- Translate the point of interest to the center point of the viewport
  //    widget.
  const gfx::Point viewport_center_point =
      magnifier_utils::GetViewportWidgetBoundsInRoot(
          current_source_root_window_, GetScreenHeightDivisor())
          .CenterPoint();
  gfx::Transform transform;
  transform.Translate(viewport_center_point.x() - point_in_pixels.x(),
                      viewport_center_point.y() - point_in_pixels.y());
  transform.Scale(scale, scale);

  // When updating the transform, we don't want any animation, otherwise the
  // movement of the mouse won't be very smooth. We want the magnifier layer to
  // update immediately with the movement of the mouse (or the change in the
  // point of interest due to input caret bounds changes ... etc.).
  ui::ScopedLayerAnimationSettings settings(
      viewport_magnifier_layer_->GetAnimator());
  settings.SetTransitionDuration(base::Milliseconds(0));
  settings.SetTweenType(gfx::Tween::ZERO);
  settings.SetPreemptionStrategy(ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
  viewport_magnifier_layer_->SetTransform(transform);
}

int DockedMagnifierController::GetMagnifierHeightForTesting() const {
  return GetTotalMagnifierHeight();
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
  MaybePerformViewportResizing(event);
  CenterOnPoint(GetCursorScreenPoint());
}

void DockedMagnifierController::OnScrollEvent(ui::ScrollEvent* event) {
  DCHECK(GetEnabled());
  if (!event->IsAltDown() || !event->IsControlDown())
    return;

  if (event->type() == ui::EventType::kScrollFlingStart ||
      event->type() == ui::EventType::kScrollFlingCancel) {
    event->StopPropagation();
    return;
  }

  if (event->type() == ui::EventType::kScroll) {
    // Notes: - Clamping of the new scale value happens inside SetScale().
    //        - Refreshing the viewport happens in the handler of the scale pref
    //          changes.
    SetScale(magnifier_utils::GetScaleFromScroll(
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

  // Ignore touch events on virtual Keyboard, to stabilize docked magnifier.
  if (keyboard::KeyboardUIController::Get()->IsEnabled() &&
      keyboard::KeyboardUIController::Get()
          ->GetKeyboardWindow()
          ->GetBoundsInScreen()
          .Contains(event_screen_point))
    return;

  CenterOnPoint(event_screen_point);
}

void DockedMagnifierController::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(widget, viewport_widget_);

  SwitchCurrentSourceRootWindowIfNeeded(nullptr,
                                        false /* update_old_root_workarea */);
}

void DockedMagnifierController::OnDidApplyDisplayChanges() {
  DCHECK(GetEnabled());

  // The viewport might have been on a display that just got removed, and hence
  // the viewport widget and its associated layers are already destroyed. In
  // that case we also cleared the |current_source_root_window_|.
  if (current_source_root_window_) {
    // Resolution may have changed. Update all bounds.
    const auto viewport_bounds = magnifier_utils::GetViewportWidgetBoundsInRoot(
        current_source_root_window_, GetScreenHeightDivisor());
    viewport_widget_->SetBounds(viewport_bounds);
    viewport_background_layer_->SetBounds(viewport_bounds);
    separator_layer_->SetBounds(
        SeparatorBoundsFromViewportBounds(viewport_bounds));
    SetViewportHeightInWorkArea(current_source_root_window_,
                                GetTotalMagnifierHeight());

    // Resolution changes, screen rotation, etc. can reset the host to confine
    // the mouse cursor inside the root window. We want to make sure the cursor
    // is confined properly outside the viewport. But don't confine mouse if
    // resizing.
    if (!is_resizing_)
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

int DockedMagnifierController::GetTotalMagnifierHeight() const {
  if (separator_layer_)
    return separator_layer_->bounds().bottom();

  return 0;
}

gfx::Rect DockedMagnifierController::GetTotalMagnifierBoundsForRoot(
    aura::Window* root) const {
  DCHECK(root);
  DCHECK(root->IsRootWindow());

  if (viewport_widget_ && current_source_root_window_ == root) {
    gfx::Rect bounds =
        viewport_widget_->GetNativeWindow()->GetActualBoundsInRootWindow();
    DCHECK(separator_layer_);
    bounds.set_height(separator_layer_->bounds().bottom());
    return bounds;
  }

  return gfx::Rect();
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

void DockedMagnifierController::MaybeSetCursorSize(ui::CursorSize cursor_size) {
  if (Shell::Get()->accessibility_controller()->large_cursor().enabled())
    return;
  Shell::Get()->cursor_manager()->SetCursorSize(cursor_size);
}

void DockedMagnifierController::MaybePerformViewportResizing(
    ui::MouseEvent* event) {
  DCHECK(current_source_root_window_);
  gfx::Rect root_bounds = current_source_root_window_->GetBoundsInRootWindow();
  float magnifier_height = root_bounds.height() / GetScreenHeightDivisor();
  float root_y = event->root_location_f().y();
  const int separator_top = separator_layer_->bounds().y();
  const int separator_bottom = separator_layer_->bounds().bottom();
  bool cursor_is_over_resizer =
      root_y >= separator_top - 1 && root_y <= separator_bottom;
  ::wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();
  CursorWindowController* cursor_window_controller =
      Shell::Get()->window_tree_host_manager()->cursor_window_controller();

  // If cursor is over separator, change to north/south resize, move on top.
  // Reset once the cursor is not over separator, and user isn't resizing.
  if (cursor_is_over_resizer && !is_cursor_locked_) {
    MaybeSetCursorSize(ui::CursorSize::kLarge);
    cursor_manager->SetCursor(ui::mojom::CursorType::kNorthSouthResize);
    cursor_manager->LockCursor();
    cursor_window_controller->OnDockedMagnifierResizingStateChanged(true);
    is_cursor_locked_ = true;
  } else if (!cursor_is_over_resizer && !is_resizing_) {
    MaybeResetResizingCursor();
  }

  // If user releases left mouse button, or any other mouse button is pressed,
  // ignore and stop resizing.
  if (!event->IsOnlyLeftMouseButton() ||
      event->type() == ui::EventType::kMouseReleased) {
    if (is_resizing_) {
      is_resizing_ = false;
      ConfineMouseCursorOutsideViewport();
    }
    return;
  }
  float new_screen_height_divisor =
      root_bounds.height() / std::max(1.0f, root_y + resize_offset_);

  switch (event->type()) {
    case ui::EventType::kMousePressed:
      // User clicks within separator to start resizing Docked Magnifier.
      // Subtracting one is needed to capture when mouse is at the very top.
      if (!is_resizing_ && cursor_is_over_resizer) {
        resize_offset_ = magnifier_height - root_y;
        is_resizing_ = true;
        RootWindowController::ForWindow(current_source_root_window_)
            ->ash_host()
            ->ConfineCursorToRootWindow();
      }
      break;
    case ui::EventType::kMouseDragged:
      // User continues holding and drags separator to resize Docked Magnifier.
      if (is_resizing_) {
        SetScreenHeightDivisor(std::clamp(new_screen_height_divisor,
                                          kMinScreenHeightDivisor,
                                          kMaxScreenHeightDivisor));
        OnDidApplyDisplayChanges();
      }
      break;
    default:
      break;
  }
}

void DockedMagnifierController::MaybeResetResizingCursor() {
  if (!is_cursor_locked_) {
    return;
  }

  MaybeSetCursorSize(ui::CursorSize::kNormal);
  Shell::Get()->cursor_manager()->UnlockCursor();
  Shell::Get()
      ->window_tree_host_manager()
      ->cursor_window_controller()
      ->OnDockedMagnifierResizingStateChanged(false);
  is_cursor_locked_ = false;
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
    if (update_old_root_workarea)
      SetViewportHeightInWorkArea(old_root_window, 0);

    // Reset mouse cursor confinement to default.
    RootWindowController::ForWindow(old_root_window)
        ->ash_host()
        ->ConfineCursorToRootWindow();
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

  auto* magnified_container = current_source_root_window_->GetChildById(
      kShellWindowId_MagnifiedContainer);
  viewport_magnifier_layer_->SetShowReflectedLayerSubtree(
      magnified_container->layer());
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
    overview_controller->EndOverview(
        OverviewEndAction::kEnabledDockedMagnifier);
  }

  if (new_enabled) {
    // Enabling the Docked Magnifier disables the Fullscreen Magnifier.
    SetFullscreenMagnifierEnabled(false);
    // Calling refresh will result in the creation of the magnifier viewport and
    // its associated layers.
    Refresh();
    // Make sure we are in front of the fullscreen magnifier which also handles
    // scroll events.
    shell->AddAccessibilityEventHandler(
        this, AccessibilityEventHandlerManager::HandlerType::kDockedMagnifier);
    shell->display_manager()->AddDisplayManagerObserver(this);
  } else {
    shell->display_manager()->RemoveDisplayManagerObserver(this);
    shell->RemoveAccessibilityEventHandler(this);
    MaybeResetResizingCursor();

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

void DockedMagnifierController::Refresh() {
  DCHECK(GetEnabled());
  CenterOnPoint(GetCursorScreenPoint());
}

void DockedMagnifierController::CreateMagnifierViewport() {
  DCHECK(GetEnabled());
  DCHECK(current_source_root_window_);

  const auto viewport_bounds = magnifier_utils::GetViewportWidgetBoundsInRoot(
      current_source_root_window_, GetScreenHeightDivisor());

  // 1- Create the viewport widget.
  viewport_widget_ = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.accept_events = false;
  params.bounds = viewport_bounds;
  params.opacity = views::Widget::InitParams::WindowOpacity::kOpaque;
  aura::Window* const viewport_parent =
      GetViewportParentContainerForRoot(current_source_root_window_);
  params.parent = viewport_parent;
  params.name = kDockedMagnifierViewportWindowName;
  viewport_widget_->Init(std::move(params));

  // 2- Create the separator layer right below the viwport widget, parented to
  //    the layer of the root window.
  separator_layer_ = std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
  separator_layer_->SetColor(SK_ColorBLACK);
  separator_layer_->SetBounds(
      SeparatorBoundsFromViewportBounds(viewport_bounds));
  aura::Window* const separator_parent =
      GetViewportParentContainerForDivider(current_source_root_window_);
  separator_parent->layer()->Add(separator_layer_.get());

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
      magnifier_utils::GetViewportWidgetBoundsInRoot(
          current_source_root_window_, GetScreenHeightDivisor());

  // 1- Point (A)'s height.
  // Note we use a Vector3dF to actually represent a 2D point. The reason is
  // Vector3dF provides an API to get the Length() of the vector without
  // converting the object to another temporary object. We need to get the
  // Length() rather than y() because screen rotation transform can make the
  // height we are interested in either x() or y() depending on the rotation
  // angle, so we just simply use Length().
  // Note: Why transform the point to the magnified scale and back? The reason
  // is that we need to go through the root window transform to go to the pixel
  // space. This will account for device scale factors, screen rotations, and
  // any other transforms that we cannot anticipate ourselves.
  gfx::Vector3dF scaled_magnifier_bottom_in_pixels(
      0.0f, viewport_bounds.bottom() + kSeparatorHeight, 0.0f);
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
  minimum_point_of_interest_height_ = minimum_height_vector.Length();
  is_minimum_point_of_interest_height_valid_ = true;
}

void DockedMagnifierController::ConfineMouseCursorOutsideViewport() {
  DCHECK(current_source_root_window_);

  gfx::Rect confine_bounds =
      current_source_root_window_->GetBoundsInRootWindow();
  const auto viewport_bounds = magnifier_utils::GetViewportWidgetBoundsInRoot(
      current_source_root_window_, GetScreenHeightDivisor());
  const int docked_height = viewport_bounds.height();
  confine_bounds.Offset(0, docked_height);
  confine_bounds.set_height(confine_bounds.height() - docked_height);
  RootWindowController::ForWindow(current_source_root_window_)
      ->ash_host()
      ->ConfineCursorToBoundsInRoot(confine_bounds);
}

}  // namespace ash
