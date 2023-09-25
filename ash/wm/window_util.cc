// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_util.h"

#include <memory>
#include <tuple>

#include "ash/constants/app_types.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/multi_user/multi_user_window_manager_impl.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_overview_session.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "chromeos/ui/base/chromeos_ui_constants.h"
#include "chromeos/ui/frame/interior_resize_handler_targeter.h"
#include "chromeos/ui/wm/features.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_targeter.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/easy_resize_window_targeter.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/public/activation_client.h"

namespace ash::window_util {
namespace {

// This window targeter reserves space for the portion of the resize handles
// that extend within a top level window.
class InteriorResizeHandleTargeterAsh
    : public chromeos::InteriorResizeHandleTargeter {
 public:
  InteriorResizeHandleTargeterAsh() = default;
  InteriorResizeHandleTargeterAsh(const InteriorResizeHandleTargeterAsh&) =
      delete;
  InteriorResizeHandleTargeterAsh& operator=(
      const InteriorResizeHandleTargeterAsh&) = delete;
  ~InteriorResizeHandleTargeterAsh() override = default;

  bool ShouldUseExtendedBounds(const aura::Window* target) const override {
    // Fullscreen/maximized windows can't be drag-resized.
    const WindowState* window_state = WindowState::Get(window());
    if (window_state && window_state->IsMaximizedOrFullscreenOrPinned())
      return false;

    // The shrunken hit region only applies to children of |window()|.
    return InteriorResizeHandleTargeter::ShouldUseExtendedBounds(target);
  }
};

// Returns true if `window` has any descendant that is a system modal window or
// is itself a system modal window.
bool ContainsSystemModalWindow(const aura::Window* window) {
  if (!window) {
    return false;
  }

  if (window->GetProperty(aura::client::kModalKey) ==
      ui::ModalType::MODAL_TYPE_SYSTEM) {
    return true;
  }

  for (const auto* child : window->children()) {
    if (ContainsSystemModalWindow(child)) {
      return true;
    }
  }

  return false;
}

// Returns the lowest common parent of the given `windows` by traversing up from
// one of the windows' direct parent and check if the intermediate parent
// contains all the `windows`. If yes, it will be the lowest common parent.
aura::Window* FindLowestCommonParent(const aura::Window::Windows& windows) {
  if (windows.empty()) {
    return nullptr;
  }

  auto contains_all = [&](aura::Window* parent) {
    for (aura::Window* window : windows) {
      if (!parent->Contains(window)) {
        return false;
      }
    }

    return true;
  };

  // As a window can `Contains` itself, which is not the common parent, we start
  // traversing from its parent instead.
  for (aura::Window* parent = windows.front()->parent(); parent;
       parent = parent->parent()) {
    if (contains_all(parent)) {
      return parent;
    }
  }

  return nullptr;
}

// Uses DFS to find the topmost child of the `parent` that is included in
// `windows`. With the reverse traversing of the children, the first observed
// window found will be the topmost one.
aura::Window* FindTopMostChild(aura::Window* parent,
                               const aura::Window::Windows& windows) {
  for (aura::Window* child : base::Reversed(parent->children())) {
    for (aura::Window* window : windows) {
      if (child == window) {
        return window;
      }
      if (child->Contains(window)) {
        return FindTopMostChild(child, windows);
      }
    }
  }

  return nullptr;
}

}  // namespace

aura::Window* GetActiveWindow() {
  if (auto* activation_client =
          wm::GetActivationClient(Shell::GetPrimaryRootWindow())) {
    return activation_client->GetActiveWindow();
  }
  return nullptr;
}

aura::Window* GetFocusedWindow() {
  return aura::client::GetFocusClient(Shell::GetPrimaryRootWindow())
      ->GetFocusedWindow();
}

bool IsStackedBelow(aura::Window* win1, aura::Window* win2) {
  CHECK_NE(win1, win2);
  CHECK_EQ(win1->parent(), win2->parent());

  const auto& children = win1->parent()->children();
  auto win1_iter = base::ranges::find(children, win1);
  auto win2_iter = base::ranges::find(children, win2);
  CHECK(win1_iter != children.end());
  CHECK(win2_iter != children.end());
  return win1_iter < win2_iter;
}

aura::Window* GetTopMostWindow(const aura::Window::Windows& windows) {
  aura::Window* lowest_common_parent = FindLowestCommonParent(windows);
  CHECK(lowest_common_parent);

  return FindTopMostChild(lowest_common_parent, windows);
}

aura::Window* GetCaptureWindow() {
  return aura::client::GetCaptureWindow(Shell::GetPrimaryRootWindow());
}

void GetBlockingContainersForRoot(aura::Window* root_window,
                                  aura::Window** min_container,
                                  aura::Window** system_modal_container) {
  if (Shell::Get()->session_controller()->IsUserSessionBlocked()) {
    *min_container =
        root_window->GetChildById(kShellWindowId_LockScreenContainersContainer);
    *system_modal_container =
        root_window->GetChildById(kShellWindowId_LockSystemModalContainer);
  } else if (aura::Window* const help_bubble_container =
                 root_window->GetChildById(kShellWindowId_HelpBubbleContainer);
             ContainsSystemModalWindow(help_bubble_container)) {
    *min_container = help_bubble_container;
    *system_modal_container = nullptr;
  } else {
    *min_container = nullptr;
    *system_modal_container =
        root_window->GetChildById(kShellWindowId_SystemModalContainer);
  }
}

bool IsWindowUserPositionable(aura::Window* window) {
  return window->GetType() == aura::client::WINDOW_TYPE_NORMAL;
}

void PinWindow(aura::Window* window, bool trusted) {
  WMEvent event(trusted ? WM_EVENT_TRUSTED_PIN : WM_EVENT_PIN);
  WindowState::Get(window)->OnWMEvent(&event);
}

void SetAutoHideShelf(aura::Window* window, bool autohide) {
  WindowState::Get(window)->set_autohide_shelf_when_maximized_or_fullscreen(
      autohide);
  for (aura::Window* root_window : Shell::GetAllRootWindows())
    Shelf::ForWindow(root_window)->UpdateVisibilityState();
}

bool MoveWindowToDisplay(aura::Window* window, int64_t display_id) {
  DCHECK(window);

  aura::Window* root = Shell::GetRootWindowForDisplayId(display_id);
  if (!root || root == window->GetRootWindow()) {
    NOTREACHED();
    return false;
  }

  WindowState* window_state = WindowState::Get(window);
  if (window_state->allow_set_bounds_direct()) {
    display::Display display;
    if (!display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id,
                                                               &display))
      return false;
    gfx::Rect bounds = window->bounds();
    gfx::Rect work_area_in_display(display.size());
    work_area_in_display.Inset(display.GetWorkAreaInsets());
    AdjustBoundsToEnsureMinimumWindowVisibility(work_area_in_display, &bounds);
    SetBoundsWMEvent event(bounds, display_id);
    window_state->OnWMEvent(&event);
    return true;
  }

  // Moves |window| to the given |root| window's corresponding container.
  aura::Window* container = RootWindowController::ForWindow(root)->GetContainer(
      window->parent()->GetId());
  if (!container)
    return false;

  // Update restore bounds to target root window.
  if (window_state->HasRestoreBounds()) {
    gfx::Rect restore_bounds = window_state->GetRestoreBoundsInParent();
    ::wm::ConvertRectToScreen(root, &restore_bounds);
    window_state->SetRestoreBoundsInScreen(restore_bounds);
  }

  container->AddChild(window);
  return true;
}

int GetNonClientComponent(aura::Window* window, const gfx::Point& location) {
  return window->delegate()
             ? window->delegate()->GetNonClientComponent(location)
             : HTNOWHERE;
}

void SetChildrenUseExtendedHitRegionForWindow(aura::Window* window) {
  gfx::Insets mouse_extend(-chromeos::kResizeOutsideBoundsSize);
  gfx::Insets touch_extend = gfx::ScaleToFlooredInsets(
      mouse_extend, chromeos::kResizeOutsideBoundsScaleForTouch);
  window->SetEventTargeter(std::make_unique<::wm::EasyResizeWindowTargeter>(
      mouse_extend, touch_extend));
}

void CloseWidgetForWindow(aura::Window* window) {
  views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
  DCHECK(widget);
  widget->Close();
}

void InstallResizeHandleWindowTargeterForWindow(aura::Window* window) {
  window->SetEventTargeter(std::make_unique<InteriorResizeHandleTargeterAsh>());
}

bool IsDraggingTabs(const aura::Window* window) {
  return window->GetProperty(ash::kIsDraggingTabsKey);
}

bool ShouldExcludeForCycleList(const aura::Window* window) {
  // Exclude windows:
  // - non user positionable windows, such as extension popups.
  // - windows being dragged
  // - pip windows
  const WindowState* state = WindowState::Get(window);
  if (!state->IsUserPositionable() || state->is_dragged() || state->IsPip())
    return true;

  // Exclude the AppList window, which will hide as soon as cycling starts
  // anyway. It doesn't make sense to count it as a "switchable" window, yet
  // a lot of code relies on the MRU list returning the app window. If we
  // don't manually remove it, the window cycling UI won't crash or misbehave,
  // but there will be a flicker as the target window changes. Also exclude
  // unselectable windows such as extension popups.
  for (auto* parent = window->parent(); parent; parent = parent->parent()) {
    if (parent->GetId() == kShellWindowId_AppListContainer)
      return true;
  }

  return window->GetProperty(kHideInOverviewKey);
}

bool ShouldExcludeForOverview(const aura::Window* window) {
  // If we're currently in tablet splitview or in clamshell mode with
  // `IsArm1AutomaticallyLockEnabled()` (see SnapGroupController for more
  // details), remove the default snapped window from the window list. The
  // default snapped window occupies one side of the screen, while the other
  // windows occupy the other side of the screen in overview mode. The default
  // snap position is the position where the window was first snapped. See
  // `default_snap_position_` in SplitViewController for more details.
  auto* split_view_controller =
      SplitViewController::Get(window->GetRootWindow());

  // A window should be excluded from being shown in overview when:
  // 1. In tablet split view mode on one window snapped;
  // 2. During split view overview session in clamshell mode,
  // 3. If the window is not the mru window in snap group i.e. the corresponding
  // overview item representation for the snap group has been created.
  auto should_exclude_in_clamshell = [&]() -> bool {
    if (auto* snap_group_controller = SnapGroupController::Get()) {
      if (auto* split_view_overview_session =
              RootWindowController::ForWindow(window)
                  ->split_view_overview_session();
          snap_group_controller->IsArm1AutomaticallyLockEnabled() &&
          split_view_overview_session &&
          split_view_overview_session->window() == window) {
        return true;
      }

      if (SnapGroup* snap_group =
              snap_group_controller->GetSnapGroupForGivenWindow(window)) {
        return window != snap_group->GetTopMostWindowInGroup();
      }
    }

    return false;
  };

  if (ShouldExcludeForCycleList(window)) {
    return true;
  }

  return Shell::Get()->tablet_mode_controller()->InTabletMode()
             ? (window == split_view_controller->GetDefaultSnappedWindow())
             : should_exclude_in_clamshell();
}

void EnsureTransientRoots(std::vector<aura::Window*>* out_window_list) {
  for (auto it = out_window_list->begin(); it != out_window_list->end();) {
    aura::Window* transient_root = ::wm::GetTransientRoot(*it);
    if (*it != transient_root) {
      if (base::Contains(*out_window_list, transient_root)) {
        it = out_window_list->erase(it);
      } else {
        *it = transient_root;
        ++it;
      }
    } else {
      ++it;
    }
  }
}

void MinimizeAndHideWithoutAnimation(
    const std::vector<aura::Window*>& windows) {
  for (auto* window : windows) {
    ScopedAnimationDisabler disable(window);

    // ARC windows are minimized asynchronously, so we hide them after
    // minimization. We minimize ARC windows first so they receive occlusion
    // updates before losing focus from being hidden. See crbug.com/910304.
    // TODO(oshima): Investigate better way to handle ARC apps immediately.

    // Suspect some callsites may use this on a window without a window state
    // (`aura::client::WINDOW_TYPE_CONTROL`) or windows that cannot be
    // minimized. See https://crbug.com/1200596.
    auto* window_state = WindowState::Get(window);
    if (window_state && window_state->CanMinimize())
      window_state->Minimize();

    window->Hide();
  }

  if (windows.size()) {
    // Disabling the animations using `ScopedAnimationDisabler` will skip
    // detaching the resources associated with the layer. So we have to trick
    // the compositor into releasing the resources.
    // crbug.com/924802.
    auto* compositor = windows[0]->layer()->GetCompositor();
    bool was_visible = compositor->IsVisible();
    compositor->SetVisible(false);
    compositor->SetVisible(was_visible);
  }
}

aura::Window* GetRootWindowAt(const gfx::Point& point_in_screen) {
  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayNearestPoint(point_in_screen);
  DCHECK(display.is_valid());
  RootWindowController* root_window_controller =
      Shell::GetRootWindowControllerWithDisplayId(display.id());
  return root_window_controller ? root_window_controller->GetRootWindow()
                                : nullptr;
}

aura::Window* GetRootWindowMatching(const gfx::Rect& rect_in_screen) {
  const display::Display& display =
      display::Screen::GetScreen()->GetDisplayMatching(rect_in_screen);
  RootWindowController* root_window_controller =
      Shell::GetRootWindowControllerWithDisplayId(display.id());
  return root_window_controller ? root_window_controller->GetRootWindow()
                                : nullptr;
}

bool IsArcPipWindow(const aura::Window* window) {
  return IsArcWindow(window) && WindowState::Get(window)->IsPip();
}

void ExpandArcPipWindow() {
  auto* pip_container = Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                                            kShellWindowId_PipContainer);
  if (!pip_container)
    return;

  auto pip_window_iter =
      base::ranges::find_if(pip_container->children(), IsArcPipWindow);
  if (pip_window_iter == pip_container->children().end())
    return;

  auto* window_state = WindowState::Get(*pip_window_iter);
  window_state->Restore();
}

bool IsAnyWindowDragged() {
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (overview_controller->InOverviewSession() &&
      overview_controller->overview_session()
          ->GetCurrentDraggedOverviewItem()) {
    return true;
  }

  for (aura::Window* window :
       Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk)) {
    if (WindowState::Get(window)->is_dragged())
      return true;
  }
  return false;
}

aura::Window* GetTopWindow() {
  MruWindowTracker::WindowList windows =
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kActiveDesk);

  return windows.empty() ? nullptr : windows[0];
}

aura::Window* GetTopNonFloatedWindow() {
  MruWindowTracker::WindowList windows =
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kActiveDesk);
  for (aura::Window* window : windows) {
    if (!WindowState::Get(window)->IsFloated())
      return window;
  }
  return nullptr;
}

aura::Window* GetFloatedWindowForActiveDesk() {
  if (!chromeos::wm::features::IsWindowLayoutMenuEnabled()) {
    return nullptr;
  }

  auto* float_controller = Shell::Get()->float_controller();
  DCHECK(float_controller);
  return float_controller->FindFloatedWindowOfDesk(
      DesksController::Get()->GetTargetActiveDesk());
}

bool ShouldMinimizeTopWindowOnBack() {
  Shell* shell = Shell::Get();
  // We never want to minimize the main app window in the Kiosk session.
  if (shell->session_controller()->IsRunningInAppMode())
    return false;

  if (!shell->tablet_mode_controller()->InTabletMode())
    return false;

  aura::Window* window = GetTopWindow();
  if (!window)
    return false;

  // Do not minimize the window if it is in overview. This can avoid unnecessary
  // window minimize animation.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (overview_controller->InOverviewSession() &&
      overview_controller->overview_session()->IsWindowInOverview(window)) {
    return false;
  }

  // ARC and crostini apps will handle the back event that follows on the client
  // side and will minimize/close the window there.
  const int app_type = window->GetProperty(aura::client::kAppType);
  if (app_type == static_cast<int>(AppType::ARC_APP) ||
      app_type == static_cast<int>(AppType::CROSTINI_APP)) {
    return false;
  }

  // Use the value of |kMinimizeOnBackKey| if it is provided. It can be provided
  // by windows with custom web contents.
  bool* can_minimize_on_back_key = window->GetProperty(kMinimizeOnBackKey);
  if (can_minimize_on_back_key)
    return *can_minimize_on_back_key;

  // Minimize the window if it is at the bottom page.
  return !shell->shell_delegate()->CanGoBack(window);
}

bool IsMinimizedOrTucked(aura::Window* window) {
  DCHECK(window->parent());

  WindowState* window_state = WindowState::Get(window);
  if (!window_state) {
    return false;
  }
  if (window_state->IsFloated()) {
    return !window->is_destroying() &&
           Shell::Get()->float_controller()->IsFloatedWindowTuckedForTablet(
               window);
  }
  return window_state->IsMinimized();
}

void SendBackKeyEvent(aura::Window* root_window) {
  // Send up event as well as down event as ARC++ clients expect this
  // sequence.
  // TODO: Investigate if we should be using the current modifiers.
  ui::KeyEvent press_key_event(ui::ET_KEY_PRESSED, ui::VKEY_BROWSER_BACK,
                               ui::EF_NONE);
  std::ignore = root_window->GetHost()->SendEventToSink(&press_key_event);
  ui::KeyEvent release_key_event(ui::ET_KEY_RELEASED, ui::VKEY_BROWSER_BACK,
                                 ui::EF_NONE);
  std::ignore = root_window->GetHost()->SendEventToSink(&release_key_event);
}

WindowTransientDescendantIteratorRange GetVisibleTransientTreeIterator(
    aura::Window* window) {
  auto hide_predicate = [](aura::Window* window) {
    return window->GetProperty(kHideInOverviewKey);
  };
  return GetTransientTreeIterator(window, base::BindRepeating(hide_predicate));
}

gfx::RectF GetTransformedBounds(aura::Window* transformed_window,
                                int top_inset) {
  gfx::RectF bounds;
  for (auto* window : GetVisibleTransientTreeIterator(transformed_window)) {
    // Ignore other window types when computing bounding box of overview target
    // item.
    if (window != transformed_window &&
        window->GetType() != aura::client::WINDOW_TYPE_NORMAL) {
      continue;
    }
    gfx::RectF window_bounds(window->GetTargetBounds());
    const gfx::Transform new_transform = TransformAboutPivot(
        window_bounds.origin(), window->layer()->GetTargetTransform());
    window_bounds = new_transform.MapRect(window_bounds);

    // The preview title is shown above the preview window. Hide the window
    // header for apps or browser windows with no tabs (web apps) to avoid
    // showing both the window header and the preview title.
    if (top_inset > 0) {
      gfx::RectF header_bounds = window_bounds;
      header_bounds.set_height(top_inset);
      header_bounds = new_transform.MapRect(header_bounds);
      window_bounds.Inset(gfx::InsetsF::TLBR(header_bounds.height(), 0, 0, 0));
    }
    ::wm::TranslateRectToScreen(window->parent(), &window_bounds);
    bounds.Union(window_bounds);
  }
  return bounds;
}

bool ShouldShowForCurrentUser(aura::Window* window) {
  MultiUserWindowManager* multi_user_window_manager =
      MultiUserWindowManagerImpl::Get();
  if (!multi_user_window_manager)
    return true;

  const AccountId account_id =
      multi_user_window_manager->GetUserPresentingWindow(window);
  // An empty account ID is returned if the window is presented for all users.
  if (!account_id.is_valid())
    return true;

  return account_id == multi_user_window_manager->CurrentAccountId();
}

aura::Window* GetEventHandlerForEvent(const ui::LocatedEvent& event) {
  gfx::Point location_in_screen = event.location();
  ::wm::ConvertPointToScreen(static_cast<aura::Window*>(event.target()),
                             &location_in_screen);
  aura::Window* root_window_at_point = GetRootWindowAt(location_in_screen);
  gfx::Point location_in_root = location_in_screen;
  ::wm::ConvertPointFromScreen(root_window_at_point, &location_in_root);
  return root_window_at_point->GetEventHandlerForPoint(location_in_root);
}

bool IsNaturalScrollOn() {
  PrefService* pref =
      Shell::Get()->session_controller()->GetActivePrefService();
  return pref->GetBoolean(prefs::kTouchpadEnabled) &&
         pref->GetBoolean(prefs::kNaturalScroll);
}

bool ShouldRoundThumbnailWindow(views::View* backdrop_view,
                                const gfx::RectF& thumbnail_bounds_in_screen) {
  // If the backdrop is not created or not visible, round the thumbnail.
  if (!backdrop_view || !backdrop_view->GetVisible()) {
    return true;
  }

  CHECK(backdrop_view->layer());
  // Get the bounds of the backdrop as a rounded rect object. This will allow us
  // to use `gfx::RRectF::Contains` to check if `thumbnail_bounds_in_screen` is
  // inside the rounding. For example, if the x,y,w,h all match and the rounding
  // is non-zero, this will return false as the thumbnails corners will be
  // considered out of bounds.
  const gfx::RRectF backdrop_bounds_in_screen(
      gfx::RRectF(gfx::RectF(backdrop_view->GetBoundsInScreen()),
                  backdrop_view->layer()->rounded_corner_radii()));
  return !backdrop_bounds_in_screen.Contains(thumbnail_bounds_in_screen);
}

}  // namespace ash::window_util
