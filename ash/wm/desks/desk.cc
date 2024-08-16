// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk.h"

#include <absl/cleanup/cleanup.h>

#include <utility>
#include <vector>

#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_restore_util.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/window_positioner.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/backdrop_controller.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/app_restore/full_restore_utils.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_tracker.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/wm/core/scoped_animation_disabler.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

// The name of the histogram for consecutive daily visits.
constexpr char kConsecutiveDailyVisitsHistogramName[] =
    "Ash.Desks.ConsecutiveDailyVisits";

// Prefix for the desks lifetime histograms.
constexpr char kDeskLifetimeHistogramNamePrefix[] = "Ash.Desks.DeskLifetime_";

// The amount of time a user has to stay on a recently activated desk for it to
// be considered interacted with. Used for tracking weekly active desks metric.
constexpr base::TimeDelta kDeskInteractedWithTime = base::Seconds(3);

// A counter for tracking the number of desks interacted with this week. A
// desk is considered interacted with if a window is moved to it, it is
// created, its name is changed or it is activated and stayed on for a brief
// period of time. This value can go beyond the max number of desks as it
// counts deleted desks that have been previously interacted with.
int g_weekly_active_desks = 0;

void UpdateBackdropController(aura::Window* desk_container) {
  auto* workspace_controller = GetWorkspaceController(desk_container);
  // Work might have already been cleared when the display is removed. See
  // |RootWindowController::MoveWindowsTo()|.
  if (!workspace_controller)
    return;

  WorkspaceLayoutManager* layout_manager =
      workspace_controller->layout_manager();
  BackdropController* backdrop_controller =
      layout_manager->backdrop_controller();
  backdrop_controller->OnDeskContentChanged();
}

bool IsOverviewUiWindow(aura::Window* window) {
  return window->GetProperty(kOverviewUiKey) &&
         !window->GetProperty(kIsOverviewItemKey);
}

// Returns true if `window` can be managed by the desk, and therefore can be
// moved out of the desk when the desk is removed.
bool CanMoveWindowOutOfDeskContainer(aura::Window* window) {
  // Overview Ui windows such as the desks bar and saved desk library should be
  // moved outside the desk when the desk is removed.
  if (IsOverviewUiWindow(window)) {
    return true;
  }

  // We never move transient descendants directly, this is taken care of by
  // `wm::TransientWindowManager::OnWindowHierarchyChanged()`.
  auto* transient_root = ::wm::GetTransientRoot(window);
  if (transient_root != window)
    return false;

  // Only allow app windows to move to other desks.
  return window->GetProperty(chromeos::kAppTypeKey) !=
         chromeos::AppType::NON_APP;
}

// Used to temporarily turn off the automatic window positioning while windows
// are being moved between desks.
class ScopedWindowPositionerDisabler {
 public:
  ScopedWindowPositionerDisabler() {
    window_positioner::DisableAutoPositioning(true);
  }

  ScopedWindowPositionerDisabler(const ScopedWindowPositionerDisabler&) =
      delete;
  ScopedWindowPositionerDisabler& operator=(
      const ScopedWindowPositionerDisabler&) = delete;

  ~ScopedWindowPositionerDisabler() {
    window_positioner::DisableAutoPositioning(false);
  }
};

}  // namespace

class DeskContainerObserver : public aura::WindowObserver {
 public:
  DeskContainerObserver(Desk* owner, aura::Window* container)
      : owner_(owner), container_(container) {
    DCHECK_EQ(container_->GetId(), owner_->container_id());
    container->AddObserver(this);
  }

  DeskContainerObserver(const DeskContainerObserver&) = delete;
  DeskContainerObserver& operator=(const DeskContainerObserver&) = delete;

  ~DeskContainerObserver() override { container_->RemoveObserver(this); }

  // aura::WindowObserver:
  void OnWindowAdded(aura::Window* new_window) override {
    // TODO(afakhry): Overview mode creates a new widget for each window under
    // the same parent for the OverviewItemView. We will be notified with
    // this window addition here. Consider ignoring these windows if they cause
    // problems.
    owner_->AddWindowToDesk(new_window);

    if (Shell::Get()->overview_controller()->InOverviewSession() &&
        !new_window->GetProperty(kHideInDeskMiniViewKey) &&
        desks_util::IsWindowVisibleOnAllWorkspaces(new_window)) {
      // If we're in overview and an all desks window has been added to a new
      // container, that means the user has moved the window to another display
      // so we need to refresh all the desk previews.
      Shell::Get()->desks_controller()->NotifyAllDesksForContentChanged();
    }
  }

  void OnWillRemoveWindow(aura::Window* window) override {
    owner_->WillRemoveWindowFromDesk(window);
  }

  void OnWindowRemoved(aura::Window* removed_window) override {
    // We listen to `OnWindowRemoved()` as opposed to `OnWillRemoveWindow()`
    // since we want to refresh the mini_views only after the window has been
    // removed from the window tree hierarchy.
    owner_->RemoveWindowFromDesk(removed_window);
  }

  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    // We need this for saved desks, where new app windows can be created while
    // in overview. The window may not be visible when `OnWindowAdded` is called
    // so updating the previews then wouldn't show the new window preview.

    if (!Shell::Get()->overview_controller()->InOverviewSession())
      return;

    // `OnWindowVisibilityChanged()` will be run for all windows in the tree of
    // `container_`. We are only interested in direct children.
    if (!window->parent() || window->parent() != container_)
      return;

    // No need to update transient children as the update will handle them.
    if (wm::GetTransientRoot(window) != window)
      return;

    // Minimized windows may be force shown to be mirrored. They won't be
    // visible on the desk preview however, so no need to update.
    if (!WindowState::Get(window) || WindowState::Get(window)->IsMinimized())
      return;

    // Do not update windows shown or hidden for overview as they will not be
    // shown in the desk previews anyways.
    if (window->GetProperty(kHideInDeskMiniViewKey))
      return;

    owner_->NotifyContentChanged();
  }

  void OnWindowDestroyed(aura::Window* window) override {
    // We should never get here. We should be notified in
    // `OnRootWindowClosing()` before the child containers of the root window
    // are destroyed, and this object should have already been destroyed.
    NOTREACHED();
  }

 private:
  const raw_ptr<Desk> owner_;
  const raw_ptr<aura::Window> container_;
};

// -----------------------------------------------------------------------------
// Desk::ScopedContentUpdateNotificationDisabler:

Desk::ScopedContentUpdateNotificationDisabler::
    ScopedContentUpdateNotificationDisabler(
        const std::vector<std::unique_ptr<Desk>>& desks,
        bool notify_when_destroyed)
    : notify_when_destroyed_(notify_when_destroyed) {
  DCHECK(!desks.empty());

  for (auto& desk : desks) {
    desks_.push_back(desk.get());
    desks_.back()->SuspendContentUpdateNotification();
  }
}

Desk::ScopedContentUpdateNotificationDisabler::
    ScopedContentUpdateNotificationDisabler(const std::vector<Desk*>& desks,
                                            bool notify_when_destroyed)
    : notify_when_destroyed_(notify_when_destroyed) {
  DCHECK(!desks.empty());

  for (auto* desk : desks) {
    desks_.push_back(desk);
    desks_.back()->SuspendContentUpdateNotification();
  }
}

Desk::ScopedContentUpdateNotificationDisabler::
    ~ScopedContentUpdateNotificationDisabler() {
  for (ash::Desk* desk : desks_) {
    desk->ResumeContentUpdateNotification(notify_when_destroyed_);
  }
}

// -----------------------------------------------------------------------------
// Desk:

Desk::Desk(int associated_container_id, bool desk_being_restored)
    : uuid_(base::Uuid::GenerateRandomV4()),
      container_id_(associated_container_id),
      creation_time_(base::Time::Now()) {
  // For the very first default desk added during initialization, there won't be
  // any root windows yet. That's OK, OnRootWindowAdded() will be called
  // explicitly by the RootWindowController when they're initialized.
  for (aura::Window* root : Shell::GetAllRootWindows())
    OnRootWindowAdded(root);

  if (!desk_being_restored)
    MaybeIncrementWeeklyActiveDesks();
}

Desk::~Desk() {
#if DCHECK_IS_ON()
  for (aura::Window* window : windows_) {
    DCHECK(!CanMoveWindowOutOfDeskContainer(window))
        << "DesksController should remove this desk's application windows "
           "first.";
  }
#endif

  for (auto& observer : observers_) {
    observers_.RemoveObserver(&observer);
    observer.OnDeskDestroyed(this);
  }
}

// static
void Desk::SetWeeklyActiveDesks(int weekly_active_desks) {
  g_weekly_active_desks = weekly_active_desks;
}

// static
int Desk::GetWeeklyActiveDesks() {
  return g_weekly_active_desks;
}

void Desk::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void Desk::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void Desk::OnRootWindowAdded(aura::Window* root) {
  DCHECK(!roots_to_containers_observers_.count(root));

  // No windows should be added to the desk container on |root| prior to
  // tracking it by the desk.
  aura::Window* desk_container = root->GetChildById(container_id_);
  DCHECK(desk_container->children().empty());
  auto container_observer =
      std::make_unique<DeskContainerObserver>(this, desk_container);
  roots_to_containers_observers_.emplace(root, std::move(container_observer));
}

void Desk::OnRootWindowClosing(aura::Window* root) {
  const size_t count = roots_to_containers_observers_.erase(root);
  DCHECK(count);

  // The windows on this root are about to be destroyed. We already stopped
  // observing the container above, so we won't get a call to
  // DeskContainerObserver::OnWindowRemoved(). Therefore, we must remove those
  // windows manually. If this is part of shutdown (i.e. when the
  // RootWindowController is being destroyed), then we're done with those
  // windows. If this is due to a display being removed, then the
  // WindowTreeHostManager will move those windows to another host/root, and
  // they will be added again to the desk container on the new root.
  const auto windows = windows_;
  for (aura::Window* window : windows) {
    if (window->GetRootWindow() == root)
      std::erase(windows_, window);
  }

  if (last_active_root_ == root) {
    last_active_root_ = nullptr;
  }

  all_desk_window_stacking_.erase(root);
}

void Desk::AddWindowToDesk(aura::Window* window) {
  DCHECK(!base::Contains(windows_, window));

  // Maybe update stacking data for all-desk windows when a window is added. If
  // `window` itself is an all-desk window, it will be handled by
  // `AddAllDeskWindow`.
  if (ShouldUpdateAllDeskStackingData() &&
      !desks_util::IsWindowVisibleOnAllWorkspaces(window)) {
    aura::Window* root = window->GetRootWindow();
    auto& adw_data = all_desk_window_stacking_[root];

    // Update `last_active_root_` in case it has changed.
    if (!is_active_ && last_active_root_ != root) {
      last_active_root_ = root;
    }

    // Find z-order of the added window.
    auto* container = GetDeskContainerForRoot(root);
    if (auto order =
            desks_util::GetWindowZOrder(container->children(), window)) {
      for (auto& adw : adw_data) {
        // All desk windows that are below the added window will have their
        // order updated (since they are now farther from the top).
        if (adw.order >= order)
          ++adw.order;
      }
    }
  } else if (HasAllDeskWindowDataOnOtherRoot(window)) {
    // This indicates that the window has moved to a new root. We will notify
    // the controller about this and it will in turn notify all desks.
    DesksController::Get()->NotifyAllDeskWindowMovedToNewRoot(window);
  }

  windows_.push_back(window);
  // No need to refresh the mini_views if the destroyed window doesn't show up
  // there in the first place. Also don't refresh for visible on all desks
  // windows since they're already refreshed in OnWindowAdded().
  if (!window->GetProperty(kHideInDeskMiniViewKey) &&
      !desks_util::IsWindowVisibleOnAllWorkspaces(window)) {
    NotifyContentChanged();
  }

  // Update the window's workspace to this parent desk.
  auto* desks_controller = DesksController::Get();
  if (!is_desk_being_removed_ &&
      !desks_util::IsWindowVisibleOnAllWorkspaces(window)) {
    // Setting the property for `kWindowWorkspaceKey` or
    // `kDeskUuidKey` will trigger a save for the window state. To
    // avoid doing this twice, we tell the window state to hold off on saving
    // until we save the `kDeskUuidKey` value.
    // TODO(b/265490703): We should eventually clean up this and
    // `GetScopedIgnorePropertyChange` when unit tests no longer need this
    // scoping to prevent double saves.
    {
      auto scoped_ignore_property_changes =
          WindowState::Get(window)->GetScopedIgnorePropertyChange();
      window->SetProperty(aura::client::kWindowWorkspaceKey,
                          desks_controller->GetDeskIndex(this));
    }

    window->SetProperty(aura::client::kDeskUuidKey, uuid_.AsLowercaseString());
  }

  MaybeIncrementWeeklyActiveDesks();
}

void Desk::RemoveWindowFromDesk(aura::Window* window) {
  DCHECK(base::Contains(windows_, window));

  std::erase(windows_, window);
  // No need to refresh the mini_views if the destroyed window doesn't show up
  // there in the first place. Also don't refresh for visible on all desks
  // windows since they're already refreshed in OnWindowRemoved().
  if (!window->GetProperty(kHideInDeskMiniViewKey) &&
      !desks_util::IsWindowVisibleOnAllWorkspaces(window)) {
    NotifyContentChanged();
  }
}

void Desk::WillRemoveWindowFromDesk(aura::Window* window) {
  // Maybe update stacking data for all-desk windows when a window is removed.
  // If `window` itself is an all-desk window, it will be handled by
  // `RemoveAllDeskWindow`.
  if (!ShouldUpdateAllDeskStackingData() ||
      desks_util::IsWindowVisibleOnAllWorkspaces(window)) {
    return;
  }

  aura::Window* root = window->GetRootWindow();
  auto& adw_data = all_desk_window_stacking_[root];

  // Nothing to update.
  if (adw_data.empty())
    return;

  aura::Window* container = GetDeskContainerForRoot(root);
  if (auto order = desks_util::GetWindowZOrder(container->children(), window)) {
    for (auto& info : adw_data) {
      // All-desk windows that are below the removed window will have their
      // order updated (since they are now closer to the top).
      if (info.order > order)
        --info.order;
    }
  }
}

bool Desk::ContainsAppWindows() const {
  return !GetAllAppWindows().empty();
}

void Desk::SetName(std::u16string new_name, bool set_by_user) {
  // Even if the user focuses the DeskNameView for the first time and hits enter
  // without changing the desk's name (i.e. |new_name| is the same,
  // |is_name_set_by_user_| is false, and |set_by_user| is true), we don't
  // change |is_name_set_by_user_| and keep considering the name as a default
  // name.
  if (name_ == new_name)
    return;

  name_ = std::move(new_name);
  is_name_set_by_user_ = set_by_user;

  if (set_by_user)
    MaybeIncrementWeeklyActiveDesks();

  for (auto& observer : observers_)
    observer.OnDeskNameChanged(name_);

  DesksController::Get()->NotifyDeskNameChanged(this, name_);
}

void Desk::SetGuid(base::Uuid new_guid) {
  if (new_guid.is_valid()) {
    uuid_ = std::move(new_guid);
  }
}

void Desk::SetLacrosProfileId(
    uint64_t lacros_profile_id,
    std::optional<DeskProfilesSelectProfileSource> source,
    bool skip_prefs_update) {
  if (lacros_profile_id == lacros_profile_id_) {
    return;
  }

  if (source) {
    base::UmaHistogramEnumeration(kDeskProfilesSelectProfileHistogramName,
                                  *source);
  }

  lacros_profile_id_ = lacros_profile_id;
  if (!skip_prefs_update) {
    desks_restore_util::UpdatePrimaryUserDeskLacrosProfileIdPrefs();
  }

  for (auto& observer : observers_) {
    observer.OnDeskProfileChanged(lacros_profile_id_);
  }
}

void Desk::PrepareForActivationAnimation() {
  DCHECK(!is_active_);

  // Floated window doesn't belong to desk container and needed to be handled
  // separately.
  if (aura::Window* floated_window =
          Shell::Get()->float_controller()->FindFloatedWindowOfDesk(this)) {
    // Ensure the floated window remain hidden during activation animation.
    // The floated window will be shown when desk is activated.
    wm::ScopedAnimationDisabler disabler(floated_window);
    floated_window->Hide();
  }

  for (aura::Window* root : Shell::GetAllRootWindows()) {
    auto* container = root->GetChildById(container_id_);
    container->layer()->SetOpacity(0);
    container->Show();
  }
  started_activation_animation_ = true;
}

void Desk::Activate(bool update_window_activation) {
  DCHECK(!is_active_);

  absl::Cleanup last_active_root_reset = [this] {
    last_active_root_ = nullptr;
  };

  if (!MaybeResetContainersOpacities()) {
    for (aura::Window* root : Shell::GetAllRootWindows())
      root->GetChildById(container_id_)->Show();
  }

  is_active_ = true;

  if (!IsConsecutiveDailyVisit())
    RecordAndResetConsecutiveDailyVisits(/*being_removed=*/false);

  int current_date = desks_restore_util::GetDaysFromLocalEpoch();
  if (current_date < last_day_visited_ || first_day_visited_ == -1) {
    // If |current_date| < |last_day_visited_| then the user has moved timezones
    // or the stored data has been corrupted so reset |first_day_visited_|.
    first_day_visited_ = current_date;
  }
  last_day_visited_ = current_date;

  active_desk_timer_.Start(
      FROM_HERE, kDeskInteractedWithTime,
      base::BindOnce(&Desk::MaybeIncrementWeeklyActiveDesks,
                     base::Unretained(this)));

  if (!update_window_activation || windows_.empty())
    return;

  auto mru_window_list =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);

  // If there's an adw window that has order=0 (should be on top) and was on the
  // last active root window, then we'll find it first and activate it. We use
  // the MRU list here so that in the case that there are multiple roots that
  // each have a topmost adw window, we'll activate the one most recently used.
  for (aura::Window* window : mru_window_list) {
    aura::Window* root = window->GetRootWindow();

    if (last_active_root_ != nullptr && last_active_root_ != root) {
      continue;
    }

    auto& adw_data = all_desk_window_stacking_[root];

    if (!adw_data.empty() && adw_data.front().window == window &&
        adw_data.front().order == 0 &&
        !WindowState::Get(window)->IsMinimized()) {
      wm::ActivateWindow(window);
      return;
    }
  }

  // Activate the window on this desk that was most recently used right before
  // the user switched to another desk, so as not to break the user's workflow.
  for (aura::Window* window : mru_window_list) {
    const auto* window_state = WindowState::Get(window);
    // Floated window should be activated with the desk window, but it doesn't
    // belong to `windows_`.
    if (!base::Contains(windows_, window) && !window_state->IsFloated()) {
      continue;
    }

    // Do not activate minimized windows, otherwise they will unminimize.
    if (window_state->IsMinimized())
      continue;

    if (desks_util::IsWindowVisibleOnAllWorkspaces(window)) {
      // Ignore an adw window that is not topmost.
      continue;
    }

    wm::ActivateWindow(window);
    return;
  }
}

void Desk::Deactivate(bool update_window_activation) {
  DCHECK(is_active_);

  auto* active_window = window_util::GetActiveWindow();

  // Hide the associated containers on all roots.
  for (aura::Window* root : Shell::GetAllRootWindows())
    root->GetChildById(container_id_)->Hide();

  is_active_ = false;
  last_day_visited_ = desks_restore_util::GetDaysFromLocalEpoch();

  active_desk_timer_.Stop();

  if (!update_window_activation)
    return;

  // Deactivate the active window (if it belongs to this desk; active window may
  // be on a different container, or one of the widgets created by overview mode
  // which are not considered desk windows) after this desk's associated
  // containers have been hidden. This is to prevent the focus controller from
  // activating another window on the same desk when the active window loses
  // focus.
  if (active_window && base::Contains(windows_, active_window))
    wm::DeactivateWindow(active_window);
}

void Desk::MoveNonAppOverviewWindowsToDesk(Desk* target_desk) {
  DCHECK(Shell::Get()->overview_controller()->InOverviewSession());

  // Wait until the end to allow notifying the observers of either desk.
  auto this_desk_throttled = ScopedContentUpdateNotificationDisabler(
      /*desks=*/{this}, /*notify_when_destroyed=*/false);
  auto target_desk_throttled = ScopedContentUpdateNotificationDisabler(
      /*desks=*/{target_desk}, /*notify_when_destroyed=*/true);

  // Create a `aura::WindowTracker` to hold `windows_`'s windows so that we do
  // not edit `windows_` in place.
  aura::WindowTracker window_tracker(windows_);

  // Move only the non-app overview windows.
  while (!window_tracker.windows().empty()) {
    auto* window = window_tracker.Pop();
    if (IsOverviewUiWindow(window)) {
      MoveWindowToDeskInternal(window, target_desk, window->GetRootWindow());
    }
  }
}

void Desk::MoveWindowsToDesk(Desk* target_desk) {
  DCHECK(target_desk);

  ScopedWindowPositionerDisabler window_positioner_disabler;

  // Throttle notifying the observers, while we move those windows and notify
  // them only once when done.
  auto this_and_target_desk_throttled = ScopedContentUpdateNotificationDisabler(
      /*desks=*/{this, target_desk}, /*notify_when_destroyed=*/true);

  // There are 2 cases in moving floated window during desk removal.
  // Case 1: If there's no floated window on the "moved-to" desk, then the
  // floated window on the current desk should remain floated. Case 2: If
  // there's a floating window on the "moved-to" desk too, unfloat the one on
  // the closed desk and retain the one on the "moved-to" desk.
  // Special Note:
  // Because of Case 2, below operation needs to be done before calling
  // `MoveWindowToDeskInternal` on `windows_to_move`. We want to re-parent
  // floated window back to desk container before the removal, so all windows
  // under the to-be-removed desk's container can be collected in
  // `windows_to_move` to move to target desk.
  Shell::Get()->float_controller()->OnMovingAllWindowsOutToDesk(this,
                                                                target_desk);

  // Moving windows will change the hierarchy and hence `windows_`, and has to
  // be done without changing the relative z-order. So we make a copy of all the
  // top-level windows on all the containers of this desk, such that windows in
  // each container are copied from top-most (z-order) to bottom-most. Note that
  // moving windows out of the container and restacking them differently may
  // trigger events that lead to destroying a window on the list. For example
  // moving the top-most window which has a backdrop will cause the backdrop to
  // be destroyed. Therefore observe such events using an `aura::WindowTracker`.
  aura::WindowTracker windows_to_move;
  for (aura::Window* root : Shell::GetAllRootWindows()) {
    const aura::Window* container = GetDeskContainerForRoot(root);
    for (aura::Window* window : base::Reversed(container->children())) {
      windows_to_move.Add(window);
    }
  }

  auto* mru_tracker = Shell::Get()->mru_window_tracker();
  while (!windows_to_move.windows().empty()) {
    auto* window = windows_to_move.Pop();
    if (!CanMoveWindowOutOfDeskContainer(window)) {
      continue;
    }

    // It's possible the `window` was already moved to the `target_desk`
    // indirectly, such as when one window in a Snap Group moves and the other
    // will follow. If this is the case, skip the explicit window move.
    if (base::Contains(target_desk->windows(), window)) {
      continue;
    }

    // Note that windows that belong to the same container in `windows_to_move`
    // are sorted from top-most to bottom-most, hence calling
    // `StackChildAtBottom()` on each in this order will maintain that same
    // order in the target_desk's container.
    MoveWindowToDeskInternal(window, target_desk, window->GetRootWindow());
    window->parent()->StackChildAtBottom(window);
    mru_tracker->OnWindowMovedOutFromRemovingDesk(window);
  }
}

void Desk::MoveWindowToDesk(aura::Window* window,
                            Desk* target_desk,
                            aura::Window* target_root,
                            bool unminimize) {
  DCHECK(window);
  DCHECK(target_desk);
  DCHECK(target_root);
  DCHECK(base::Contains(windows_, window));
  DCHECK(this != target_desk);

  ScopedWindowPositionerDisabler window_positioner_disabler;

  // Throttling here is necessary even though we're attempting to move a
  // single window. This is because that window might exist in a transient
  // window tree, which will result in actually moving multiple windows if the
  // transient children used to be on the same container.
  // See `wm::TransientWindowManager::OnWindowHierarchyChanged()`.
  auto this_and_target_desk_throttled = ScopedContentUpdateNotificationDisabler(
      /*desks=*/{this, target_desk}, /*notify_when_destroyed=*/true);

  // Always move the root of the transient window tree. We should never move a
  // transient child and leave its parent behind. Moving the transient
  // descendants that exist on the same desk container will be taken care of by
  // `wm::TransientWindowManager::OnWindowHierarchyChanged()`.
  aura::Window* transient_root = ::wm::GetTransientRoot(window);
  MoveWindowToDeskInternal(transient_root, target_desk, target_root);

  if (!desks_util::IsWindowVisibleOnAllWorkspaces(window)) {
    window_util::FixWindowStackingAccordingToGlobalMru(transient_root);
  }

  // Unminimize the window so that it shows up in the mini_view after it had
  // been dragged and moved to another desk. Don't unminimize if the window is
  // visible on all desks since it's being moved during desk activation.
  auto* window_state = WindowState::Get(transient_root);
  if (unminimize && window_state->IsMinimized() &&
      !desks_util::IsWindowVisibleOnAllWorkspaces(window)) {
    window_state->Unminimize();
  }
}

aura::Window* Desk::GetDeskContainerForRoot(aura::Window* root) const {
  DCHECK(root);

  return root->GetChildById(container_id_);
}

void Desk::NotifyContentChanged() {
  if (ContentUpdateNotificationSuspended()) {
    return;
  }

  // Updating the backdrops below may lead to the removal or creation of
  // backdrop windows in this desk, which can cause us to recurse back here.
  // Disable this.
  auto disable_recursion = ScopedContentUpdateNotificationDisabler(
      /*desks=*/{this}, /*notify_when_destroyed=*/false);

  // The availability and visibility of backdrops of all containers associated
  // with this desk will be updated *before* notifying observer, so that the
  // mini_views update *after* the backdrops do.
  // This is *only* needed if the WorkspaceLayoutManager won't take care of this
  // for us while overview is active.
  if (Shell::Get()->overview_controller()->InOverviewSession())
    UpdateDeskBackdrops();

  for (auto& observer : observers_)
    observer.OnContentChanged();
}

void Desk::UpdateDeskBackdrops() {
  for (aura::Window* root : Shell::GetAllRootWindows()) {
    UpdateBackdropController(GetDeskContainerForRoot(root));
  }
}

void Desk::RecordLifetimeHistogram(int index) {
  // Desk index is 1-indexed in histograms. The histogram is only defined for
  // the first 8 desks.
  if (const int desk_index = index + 1; desk_index <= 8) {
    std::string histogram;
    if (lacros_profile_id() != 0) {
      histogram = base::StringPrintf(
          "%sProfile_%i", kDeskLifetimeHistogramNamePrefix, desk_index);
    } else {
      histogram = base::StringPrintf("%s%i", kDeskLifetimeHistogramNamePrefix,
                                     desk_index);
    }

    base::UmaHistogramCounts1000(
        histogram, (base::Time::Now() - creation_time_).InHours());
  }
}

bool Desk::IsConsecutiveDailyVisit() const {
  if (last_day_visited_ == -1)
    return true;

  const int days_since_last_visit =
      desks_restore_util::GetDaysFromLocalEpoch() - last_day_visited_;
  return days_since_last_visit <= 1;
}

void Desk::RecordAndResetConsecutiveDailyVisits(bool being_removed) {
  if (being_removed && is_active_) {
    // When the user removes the active desk, update |last_day_visited_| to the
    // current day to account for the time they spent on this desk.
    last_day_visited_ = desks_restore_util::GetDaysFromLocalEpoch();
  }

  const int consecutive_daily_visits =
      last_day_visited_ - first_day_visited_ + 1;
  DCHECK_GE(consecutive_daily_visits, 1);
  base::UmaHistogramCounts1000(kConsecutiveDailyVisitsHistogramName,
                               consecutive_daily_visits);

  last_day_visited_ = -1;
  first_day_visited_ = -1;
}

std::vector<raw_ptr<aura::Window, VectorExperimental>> Desk::GetAllAppWindows()
    const {
  // We need to copy the app windows from `windows_` into `app_windows` so
  // that we do not modify `windows_` in place. This also gives us a filtered
  // list with all of the app windows that we need to remove.
  std::vector<raw_ptr<aura::Window, VectorExperimental>> app_windows;
  base::ranges::copy_if(windows_, std::back_inserter(app_windows),
                        [](aura::Window* window) {
                          return window->GetProperty(chromeos::kAppTypeKey) !=
                                 chromeos::AppType::NON_APP;
                        });
  // Note that floated window is also app window but needs to be handled
  // separately since it doesn't store in desk container.
  if (aura::Window* floated_window =
          Shell::Get()->float_controller()->FindFloatedWindowOfDesk(this)) {
    app_windows.push_back(floated_window);
  }

  return app_windows;
}

std::vector<raw_ptr<aura::Window, VectorExperimental>>
Desk::GetAllAssociatedWindows() const {
  // Note that floated window needs to be handled separately since it doesn't
  // store in desk container.
  if (auto* floated_window =
          Shell::Get()->float_controller()->FindFloatedWindowOfDesk(this)) {
    std::vector<raw_ptr<aura::Window, VectorExperimental>> all_windows;
    base::ranges::copy(windows_, std::back_inserter(all_windows));
    all_windows.push_back(floated_window);
    return all_windows;
  }
  return windows_;
}

void Desk::BuildAllDeskStackingData() {
  auto* active_window = window_util::GetActiveWindow();
  last_active_root_ = active_window ? active_window->GetRootWindow() : nullptr;

  for (aura::Window* root : Shell::GetAllRootWindows()) {
    auto& adw_data = all_desk_window_stacking_[root];
    aura::Window* container = GetDeskContainerForRoot(root);

    const auto& desk_windows = container->children();

    adw_data.clear();
    size_t order = 0;
    for (aura::Window* window : base::Reversed(desk_windows)) {
      if (desks_util::IsZOrderTracked(window)) {
        if (desks_util::IsWindowVisibleOnAllWorkspaces(window))
          adw_data.push_back({.window = window, .order = order});
        ++order;
      }
    }
  }
}

void Desk::RestackAllDeskWindows() {
  for (aura::Window* root : Shell::GetAllRootWindows()) {
    auto& adw_data = all_desk_window_stacking_[root];
    if (adw_data.empty()) {
      continue;
    }

    aura::Window* container = GetDeskContainerForRoot(root);

    // At this point, all desk windows have been moved to the container and
    // should be at the end of the list of children.
    const size_t count = container->children().size();
    DCHECK_LE(adw_data.size(), count);

    // Keeps track of which ADW windows have been stacked in the code below.
    base::flat_set<aura::Window*> already_stacked;

    // Find the place to insert, counting only windows that are Z-order tracked.
    auto find_window_to_stack_below = [&](size_t order) -> aura::Window* {
      size_t index = 0;
      for (aura::Window* w : base::Reversed(container->children())) {
        if (desks_util::IsZOrderTracked(w) &&
            (!desks_util::IsWindowVisibleOnAllWorkspaces(w) ||
             already_stacked.contains(w))) {
          ++index;
        }
        if (order == index) {
          return w;
        }
      }
      return nullptr;
    };

    for (auto& adw : adw_data) {
      DCHECK(adw.window);

      if (adw.window->parent() != container) {
        // TODO(b/295371112): Clean this up when the root cause has been
        // resolved. When this function is called, `this` is going to be the
        // active desk and it is expected that all all-desk windows have been
        // moved to this desk. If this branch is taken, we have an ADW that is
        // *not* on the current desk and we must not try to stack it.
        SCOPED_CRASH_KEY_NUMBER(
            "Restack", "adw_app_type",
            static_cast<int>(adw.window->GetProperty(chromeos::kAppTypeKey)));
        SCOPED_CRASH_KEY_STRING32("Restack", "adw_app_id",
                                  full_restore::GetAppId(adw.window));

        base::debug::DumpWithoutCrashing();
        continue;
      }

      if (adw.order == 0) {
        container->StackChildAtTop(adw.window);
      } else if (aura::Window* stack_below =
                     find_window_to_stack_below(adw.order)) {
        if (adw.window != stack_below) {
          container->StackChildBelow(adw.window, stack_below);
        }
      }
      already_stacked.insert(adw.window);
    }
  }
}

void Desk::TrackAllDeskWindow(aura::Window* window) {
  // Floated windows are always on top, so we should not track their stacking
  // data.
  if (WindowState::Get(window)->IsFloated()) {
    return;
  }
  aura::Window* root = window->GetRootWindow();
  auto& adw_data = all_desk_window_stacking_[root];

  // Update `last_active_root_` in case it has changed.
  if (!is_active_ && last_active_root_ != root) {
    last_active_root_ = root;
  }

  // Assume this window is going to be on top and bump remaining windows down.
  adw_data.insert(adw_data.begin(), {.window = window, .order = 0});
  for (size_t i = 1; i != adw_data.size(); ++i) {
    ++adw_data[i].order;
  }
}

void Desk::UntrackAllDeskWindow(aura::Window* window,
                                aura::Window* recent_root) {
  CHECK(recent_root);

  auto& adw_data = all_desk_window_stacking_[recent_root];
  auto it =
      base::ranges::find(adw_data, window, &AllDeskWindowStackingData::window);
  if (it == adw_data.end()) {
    // This will happen when the desk was created after the window was made into
    // an all desk window. In this case, there's nothing to do since this desk
    // doesn't have any stacking info for this window. This will also happen
    // when the window is floated, as floated windows are always on top and
    // shouldn't have stacking data.
    return;
  }

  // Reset `last_active_root_` if the adw being removed was the mru window.
  if (!is_active_ && recent_root == last_active_root_ && it->order == 0) {
    last_active_root_ = nullptr;
  }

  it = adw_data.erase(it);
  // Raise all remaining windows up.
  for (; it != adw_data.end(); ++it) {
    --it->order;
  }
}

void Desk::AllDeskWindowMovedToNewRoot(aura::Window* window) {
  // `window` has been moved to a new root, so `all_desk_window_stacking_` will
  // need to be updated.
  for (const auto& [root, adw_vec] : all_desk_window_stacking_) {
    for (const auto& adw : adw_vec) {
      if (adw.window == window) {
        UntrackAllDeskWindow(window, /*recent_root=*/root);
        TrackAllDeskWindow(window);

        // Note: Both functions above will manipulate ADW data, so it is
        // imperative that we end the iteration here.
        return;
      }
    }
  }

  NOTREACHED();
}

bool Desk::ContentUpdateNotificationSuspended() const {
  return content_update_notification_suspend_count_ != 0;
}

void Desk::MoveWindowToDeskInternal(aura::Window* window,
                                    Desk* target_desk,
                                    aura::Window* target_root) {
  DCHECK(base::Contains(windows_, window));
  DCHECK(CanMoveWindowOutOfDeskContainer(window))
      << "Non-desk windows are not allowed to move out of the container.";

  // When |target_root| is different than the current window's |root|, this can
  // only happen when dragging and dropping a window on mini desk view on
  // another display. Therefore |target_desk| is an inactive desk (i.e.
  // invisible). The order doesn't really matter, but we move the window to the
  // target desk's container first (so that it becomes hidden), then move it to
  // the target display (while it's hidden).
  aura::Window* root = window->GetRootWindow();
  aura::Window* source_container = GetDeskContainerForRoot(root);
  aura::Window* target_container = target_desk->GetDeskContainerForRoot(root);
  DCHECK(window->parent() == source_container);
  target_container->AddChild(window);

  if (root != target_root) {
    // Move the window to the container with the same ID on the target display's
    // root (i.e. container that belongs to the same desk), and adjust its
    // bounds to fit in the new display's work area.
    window_util::MoveWindowToDisplay(window,
                                     display::Screen::GetScreen()
                                         ->GetDisplayNearestWindow(target_root)
                                         .id());
    DCHECK_EQ(target_desk->container_id_, window->parent()->GetId());
  }
}

bool Desk::ShouldUpdateAllDeskStackingData() {
  return !is_active_;
}

bool Desk::MaybeResetContainersOpacities() {
  if (!started_activation_animation_)
    return false;

  for (aura::Window* root : Shell::GetAllRootWindows()) {
    auto* container = root->GetChildById(container_id_);
    container->layer()->SetOpacity(1);
  }
  started_activation_animation_ = false;
  return true;
}

void Desk::MaybeIncrementWeeklyActiveDesks() {
  if (interacted_with_this_week_)
    return;
  interacted_with_this_week_ = true;
  ++g_weekly_active_desks;
}

void Desk::SuspendContentUpdateNotification() {
  ++content_update_notification_suspend_count_;
}

void Desk::ResumeContentUpdateNotification(bool notify_when_fully_resumed) {
  --content_update_notification_suspend_count_;
  DCHECK_GE(content_update_notification_suspend_count_, 0);

  if (!content_update_notification_suspend_count_ &&
      notify_when_fully_resumed) {
    NotifyContentChanged();
  }
}

bool Desk::HasAllDeskWindowDataOnOtherRoot(aura::Window* window) const {
  if (!desks_util::IsWindowVisibleOnAllWorkspaces(window)) {
    return false;
  }

  aura::Window* current_root = window->GetRootWindow();
  for (const auto& [root, adw_vec] : all_desk_window_stacking_) {
    if (root != current_root) {
      for (const auto& adw : adw_vec) {
        if (adw.window == window) {
          return true;
        }
      }
    }
  }

  return false;
}

}  // namespace ash
