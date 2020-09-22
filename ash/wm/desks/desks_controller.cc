// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_controller.h"

#include <utility>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_animation_base.h"
#include "ash/wm/desks/desk_animation_impl.h"
#include "ash/wm/desks/desks_animations.h"
#include "ash/wm/desks/desks_restore_util.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_cycle_controller.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/numerics/ranges.h"
#include "base/stl_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

constexpr char kNewDeskHistogramName[] = "Ash.Desks.NewDesk2";
constexpr char kDesksCountHistogramName[] = "Ash.Desks.DesksCount2";
constexpr char kRemoveDeskHistogramName[] = "Ash.Desks.RemoveDesk";
constexpr char kDeskSwitchHistogramName[] = "Ash.Desks.DesksSwitch";
constexpr char kMoveWindowFromActiveDeskHistogramName[] =
    "Ash.Desks.MoveWindowFromActiveDesk";
constexpr char kNumberOfWindowsOnDesk_1_HistogramName[] =
    "Ash.Desks.NumberOfWindowsOnDesk_1";
constexpr char kNumberOfWindowsOnDesk_2_HistogramName[] =
    "Ash.Desks.NumberOfWindowsOnDesk_2";
constexpr char kNumberOfWindowsOnDesk_3_HistogramName[] =
    "Ash.Desks.NumberOfWindowsOnDesk_3";
constexpr char kNumberOfWindowsOnDesk_4_HistogramName[] =
    "Ash.Desks.NumberOfWindowsOnDesk_4";

// Appends the given |windows| to the end of the currently active overview mode
// session such that the most-recently used window is added first. If
// The windows will animate to their positions in the overview grid.
void AppendWindowsToOverview(const std::vector<aura::Window*>& windows) {
  DCHECK(Shell::Get()->overview_controller()->InOverviewSession());

  auto* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  for (auto* window :
       Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk)) {
    if (!base::Contains(windows, window) ||
        window_util::ShouldExcludeForOverview(window)) {
      continue;
    }

    overview_session->AppendItem(window, /*reposition=*/true, /*animate=*/true);
  }
}

// Removes all the items that currently exist in overview.
void RemoveAllWindowsFromOverview() {
  DCHECK(Shell::Get()->overview_controller()->InOverviewSession());

  auto* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  for (const auto& grid : overview_session->grid_list()) {
    while (!grid->empty())
      overview_session->RemoveItem(grid->window_list()[0].get());
  }
}

base::string16 GetDeskDefaultName(size_t desk_index) {
  DCHECK_LT(desk_index, desks_util::kMaxNumberOfDesks);
  constexpr int kStringIds[] = {IDS_ASH_DESKS_DESK_1_MINI_VIEW_TITLE,
                                IDS_ASH_DESKS_DESK_2_MINI_VIEW_TITLE,
                                IDS_ASH_DESKS_DESK_3_MINI_VIEW_TITLE,
                                IDS_ASH_DESKS_DESK_4_MINI_VIEW_TITLE};
  static_assert(desks_util::kMaxNumberOfDesks == base::size(kStringIds),
                "Wrong default desks' names.");

  return l10n_util::GetStringUTF16(kStringIds[desk_index]);
}

// Updates the |ShelfItem::is_on_active_desk| of the items associated with
// |windows_on_inactive_desk| and |windows_on_active_desk|. The items of the
// given windows will be updated, while the rest will remain unchanged. Either
// or both window lists can be empty.
void MaybeUpdateShelfItems(
    const std::vector<aura::Window*>& windows_on_inactive_desk,
    const std::vector<aura::Window*>& windows_on_active_desk) {
  if (!features::IsPerDeskShelfEnabled())
    return;

  auto* shelf_model = ShelfModel::Get();
  DCHECK(shelf_model);
  std::vector<ShelfModel::ItemDeskUpdate> shelf_items_updates;

  auto add_shelf_item_update = [&](aura::Window* window,
                                   bool is_on_active_desk) {
    const ShelfID shelf_id =
        ShelfID::Deserialize(window->GetProperty(kShelfIDKey));
    const int index = shelf_model->ItemIndexByID(shelf_id);

    if (index < 0)
      return;

    shelf_items_updates.push_back({index, is_on_active_desk});
  };

  for (auto* window : windows_on_inactive_desk)
    add_shelf_item_update(window, /*is_on_active_desk=*/false);
  for (auto* window : windows_on_active_desk)
    add_shelf_item_update(window, /*is_on_active_desk=*/true);

  shelf_model->UpdateItemsForDeskChange(shelf_items_updates);
}

}  // namespace

DesksController::DesksController()
    : is_enhanced_desk_animations_(features::IsEnhancedDeskAnimations()) {
  Shell::Get()->activation_client()->AddObserver(this);
  Shell::Get()->session_controller()->AddObserver(this);

  for (int id : desks_util::GetDesksContainersIds())
    available_container_ids_.push(id);

  // There's always one default desk. The DesksCreationRemovalSource used here
  // doesn't matter, since UMA reporting will be skipped for the first ever
  // default desk.
  NewDesk(DesksCreationRemovalSource::kButton);
  active_desk_ = desks_.back().get();
  active_desk_->Activate(/*update_window_activation=*/true);
}

DesksController::~DesksController() {
  Shell::Get()->session_controller()->RemoveObserver(this);
  Shell::Get()->activation_client()->RemoveObserver(this);
}

// static
DesksController* DesksController::Get() {
  return Shell::Get()->desks_controller();
}

const Desk* DesksController::GetTargetActiveDesk() const {
  if (animation_)
    return desks_[animation_->ending_desk_index()].get();
  return active_desk();
}

void DesksController::Shutdown() {
  animation_.reset();
}

void DesksController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DesksController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool DesksController::AreDesksBeingModified() const {
  return are_desks_being_modified_ || !!animation_;
}

bool DesksController::CanCreateDesks() const {
  return desks_.size() < desks_util::kMaxNumberOfDesks;
}

Desk* DesksController::GetNextDesk() const {
  int next_index = GetDeskIndex(active_desk_);
  if (++next_index >= static_cast<int>(desks_.size()))
    return nullptr;
  return desks_[next_index].get();
}

Desk* DesksController::GetPreviousDesk() const {
  int previous_index = GetDeskIndex(active_desk_);
  if (--previous_index < 0)
    return nullptr;
  return desks_[previous_index].get();
}

bool DesksController::CanRemoveDesks() const {
  return desks_.size() > 1;
}

void DesksController::NewDesk(DesksCreationRemovalSource source) {
  DCHECK(CanCreateDesks());
  DCHECK(!available_container_ids_.empty());

  base::AutoReset<bool> in_progress(&are_desks_being_modified_, true);

  // The first default desk should not overwrite any desks restore data, nor
  // should it trigger any UMA stats reports.
  const bool is_first_ever_desk = desks_.empty();

  desks_.push_back(std::make_unique<Desk>(available_container_ids_.front()));
  available_container_ids_.pop();
  Desk* new_desk = desks_.back().get();
  new_desk->SetName(GetDeskDefaultName(desks_.size() - 1),
                    /*set_by_user=*/false);

  Shell::Get()
      ->accessibility_controller()
      ->TriggerAccessibilityAlertWithMessage(l10n_util::GetStringFUTF8(
          IDS_ASH_VIRTUAL_DESKS_ALERT_NEW_DESK_CREATED,
          base::NumberToString16(desks_.size())));

  for (auto& observer : observers_)
    observer.OnDeskAdded(new_desk);

  if (!is_first_ever_desk) {
    desks_restore_util::UpdatePrimaryUserDesksPrefs();
    UMA_HISTOGRAM_ENUMERATION(kNewDeskHistogramName, source);
    ReportDesksCountHistogram();
  }
}

void DesksController::RemoveDesk(const Desk* desk,
                                 DesksCreationRemovalSource source) {
  DCHECK(CanRemoveDesks());
  DCHECK(HasDesk(desk));

  base::AutoReset<bool> in_progress(&are_desks_being_modified_, true);

  auto* overview_controller = Shell::Get()->overview_controller();
  const bool in_overview = overview_controller->InOverviewSession();
  if (!in_overview && active_desk_ == desk) {
    // When removing the active desk outside of overview, we trigger the remove
    // desk animation. We will activate the desk to its left if any, otherwise,
    // we activate one on the right.
    const int current_desk_index = GetDeskIndex(active_desk_);
    const int target_desk_index =
        current_desk_index + ((current_desk_index > 0) ? -1 : 1);
    DCHECK_GE(target_desk_index, 0);
    DCHECK_LT(target_desk_index, static_cast<int>(desks_.size()));
    animation_ = std::make_unique<DeskRemovalAnimation>(
        this, current_desk_index, target_desk_index, source);
    animation_->Launch();
    return;
  }

  RemoveDeskInternal(desk, source);
}

void DesksController::ActivateDesk(const Desk* desk, DesksSwitchSource source) {
  DCHECK(HasDesk(desk));
  DCHECK(!animation_);

  OverviewController* overview_controller = Shell::Get()->overview_controller();
  const bool in_overview = overview_controller->InOverviewSession();
  if (desk == active_desk_) {
    if (in_overview) {
      // Selecting the active desk's mini_view in overview mode is allowed and
      // should just exit overview mode normally.
      overview_controller->EndOverview();
    }
    return;
  }

  UMA_HISTOGRAM_ENUMERATION(kDeskSwitchHistogramName, source);

  const int target_desk_index = GetDeskIndex(desk);
  if (source != DesksSwitchSource::kDeskRemoved) {
    // Desk removal has its own a11y alert.
    Shell::Get()
        ->accessibility_controller()
        ->TriggerAccessibilityAlertWithMessage(l10n_util::GetStringFUTF8(
            IDS_ASH_VIRTUAL_DESKS_ALERT_DESK_ACTIVATED, desk->name()));
  }

  if (source == DesksSwitchSource::kDeskRemoved ||
      source == DesksSwitchSource::kUserSwitch) {
    // Desk switches due to desks removal or user switches in a multi-profile
    // session result in immediate desk activation without animation.
    ActivateDeskInternal(desk, /*update_window_activation=*/!in_overview);
    return;
  }

  const int starting_desk_index = GetDeskIndex(active_desk());
  animation_ = std::make_unique<DeskActivationAnimation>(
      this, starting_desk_index, target_desk_index, source);
  animation_->Launch();
}

bool DesksController::ActivateAdjacentDesk(bool going_left,
                                           DesksSwitchSource source) {
  // An on-going desk switch animation might be in progress. Skip this
  // accelerator or touchpad event if enhanced desk animations are not enabled.
  if (!is_enhanced_desk_animations_ && AreDesksBeingModified())
    return false;

  if (Shell::Get()->session_controller()->IsUserSessionBlocked())
    return false;

  // Try replacing an ongoing desk animation of the same source.
  if (is_enhanced_desk_animations_ && animation_ &&
      animation_->Replace(going_left, source)) {
    return true;
  }

  const Desk* desk_to_activate = going_left ? GetPreviousDesk() : GetNextDesk();
  if (desk_to_activate) {
    ActivateDesk(desk_to_activate, source);
  } else {
    for (auto* root : Shell::GetAllRootWindows())
      desks_animations::PerformHitTheWallAnimation(root, going_left);
  }

  return true;
}

bool DesksController::StartSwipeAnimation(bool move_left) {
  DCHECK(is_enhanced_desk_animations_);

  // Activate an adjacent desk. It will replace an ongoing touchpad animation if
  // one exists.
  return ActivateAdjacentDesk(move_left,
                              DesksSwitchSource::kDeskSwitchTouchpad);
}

void DesksController::UpdateSwipeAnimation(float scroll_delta_x) {
  DCHECK(is_enhanced_desk_animations_);
  if (animation_)
    animation_->UpdateSwipeAnimation(scroll_delta_x);
}

void DesksController::EndSwipeAnimation() {
  DCHECK(is_enhanced_desk_animations_);
  if (animation_)
    animation_->EndSwipeAnimation();
}

bool DesksController::MoveWindowFromActiveDeskTo(
    aura::Window* window,
    Desk* target_desk,
    aura::Window* target_root,
    DesksMoveWindowFromActiveDeskSource source) {
  DCHECK_NE(active_desk_, target_desk);

  // An active window might be an always-on-top or pip which doesn't belong to
  // the active desk, and hence cannot be removed.
  if (!base::Contains(active_desk_->windows(), window))
    return false;

  base::AutoReset<bool> in_progress(&are_desks_being_modified_, true);

  auto* overview_controller = Shell::Get()->overview_controller();
  const bool in_overview = overview_controller->InOverviewSession();

  // The below order matters:
  // If in overview, remove the item from overview first, before calling
  // MoveWindowToDesk(), since MoveWindowToDesk() unminimizes the window (if it
  // was minimized) before updating the mini views. We shouldn't change the
  // window's minimized state before removing it from overview, since overview
  // handles minimized windows differently.
  if (in_overview) {
    auto* overview_session = overview_controller->overview_session();
    auto* item = overview_session->GetOverviewItemForWindow(window);
    DCHECK(item);
    item->OnMovingWindowToAnotherDesk();
    // The item no longer needs to be in the overview grid.
    overview_session->RemoveItem(item);
  }

  active_desk_->MoveWindowToDesk(window, target_desk, target_root);

  MaybeUpdateShelfItems(/*windows_on_inactive_desk=*/{window},
                        /*windows_on_active_desk=*/{});

  Shell::Get()
      ->accessibility_controller()
      ->TriggerAccessibilityAlertWithMessage(l10n_util::GetStringFUTF8(
          IDS_ASH_VIRTUAL_DESKS_ALERT_WINDOW_MOVED_FROM_ACTIVE_DESK,
          window->GetTitle(), active_desk_->name(), target_desk->name()));

  UMA_HISTOGRAM_ENUMERATION(kMoveWindowFromActiveDeskHistogramName, source);
  ReportNumberOfWindowsPerDeskHistogram();

  // A window moving out of the active desk cannot be active.
  // If we are in overview, we should not change the window activation as we do
  // below, since the dummy "OverviewModeFocusedWidget" should remain active
  // while overview mode is active.
  if (!in_overview)
    wm::DeactivateWindow(window);
  return true;
}

void DesksController::RevertDeskNameToDefault(Desk* desk) {
  DCHECK(HasDesk(desk));
  desk->SetName(GetDeskDefaultName(GetDeskIndex(desk)), /*set_by_user=*/false);
}

void DesksController::RestoreNameOfDeskAtIndex(base::string16 name,
                                               size_t index) {
  DCHECK(!name.empty());
  DCHECK_LT(index, desks_.size());

  desks_[index]->SetName(std::move(name), /*set_by_user=*/true);
}

void DesksController::OnRootWindowAdded(aura::Window* root_window) {
  for (auto& desk : desks_)
    desk->OnRootWindowAdded(root_window);
}

void DesksController::OnRootWindowClosing(aura::Window* root_window) {
  for (auto& desk : desks_)
    desk->OnRootWindowClosing(root_window);
}

bool DesksController::BelongsToActiveDesk(aura::Window* window) {
  return desks_util::BelongsToActiveDesk(window);
}

void DesksController::OnWindowActivating(ActivationReason reason,
                                         aura::Window* gaining_active,
                                         aura::Window* losing_active) {
  if (AreDesksBeingModified())
    return;

  if (!gaining_active)
    return;

  const Desk* window_desk = FindDeskOfWindow(gaining_active);
  if (!window_desk || window_desk == active_desk_)
    return;

  ActivateDesk(window_desk, DesksSwitchSource::kWindowActivated);
}

void DesksController::OnWindowActivated(ActivationReason reason,
                                        aura::Window* gained_active,
                                        aura::Window* lost_active) {}

void DesksController::OnActiveUserSessionChanged(const AccountId& account_id) {
  // TODO(afakhry): Remove this when multi-profile support goes away.
  if (!current_account_id_.is_valid()) {
    // This is the login of the first primary user. No need to switch any desks.
    current_account_id_ = account_id;
    return;
  }

  user_to_active_desk_index_[current_account_id_] = GetDeskIndex(active_desk_);
  current_account_id_ = account_id;

  // Note the following constraints:
  // - Simultaneously logged-in users share the same number of desks.
  // - We don't sync and restore the number of desks nor the active desk
  //   position from previous login sessions.
  //
  // Given the above, we do the following for simplicity:
  // - If this user has never been seen before, we activate their first desk.
  // - If one of the simultaneously logged-in users remove desks, that other
  //   users' active-desk indices may become invalid. We won't create extra
  //   desks for this user, but rather we will simply activate their last desk
  //   on the right. Future user switches will update the pref for this user to
  //   the correct value.
  int new_user_active_desk_index =
      /* This is a default initialized index to 0 if the id doesn't exist. */
      user_to_active_desk_index_[current_account_id_];
  new_user_active_desk_index = base::ClampToRange(
      new_user_active_desk_index, 0, static_cast<int>(desks_.size()) - 1);

  ActivateDesk(desks_[new_user_active_desk_index].get(),
               DesksSwitchSource::kUserSwitch);
}

void DesksController::OnFirstSessionStarted() {
  desks_restore_util::RestorePrimaryUserDesks();
}

void DesksController::OnAnimationFinished(DeskAnimationBase* animation) {
  DCHECK_EQ(animation_.get(), animation);
  animation_.reset();
}

bool DesksController::HasDesk(const Desk* desk) const {
  auto iter = std::find_if(
      desks_.begin(), desks_.end(),
      [desk](const std::unique_ptr<Desk>& d) { return d.get() == desk; });
  return iter != desks_.end();
}

int DesksController::GetDeskIndex(const Desk* desk) const {
  for (size_t i = 0; i < desks_.size(); ++i) {
    if (desk == desks_[i].get())
      return i;
  }

  NOTREACHED();
  return -1;
}

void DesksController::ActivateDeskInternal(const Desk* desk,
                                           bool update_window_activation) {
  DCHECK(HasDesk(desk));

  if (desk == active_desk_)
    return;

  base::AutoReset<bool> in_progress(&are_desks_being_modified_, true);

  // Mark the new desk as active first, so that deactivating windows on the
  // `old_active` desk do not activate other windows on the same desk. See
  // `ash::AshFocusRules::GetNextActivatableWindow()`.
  Desk* old_active = active_desk_;
  active_desk_ = const_cast<Desk*>(desk);

  // There should always be an active desk at any time.
  DCHECK(old_active);
  old_active->Deactivate(update_window_activation);
  active_desk_->Activate(update_window_activation);

  MaybeUpdateShelfItems(old_active->windows(), active_desk_->windows());

  // If in the middle of a window cycle gesture, reset the window cycle list
  // contents so it contains the new active desk's windows.
  if (features::IsAltTabLimitedToActiveDesk()) {
    auto* window_cycle_controller = Shell::Get()->window_cycle_controller();
    window_cycle_controller->MaybeResetCycleList();
  }

  for (auto& observer : observers_)
    observer.OnDeskActivationChanged(active_desk_, old_active);
}

void DesksController::RemoveDeskInternal(const Desk* desk,
                                         DesksCreationRemovalSource source) {
  DCHECK(CanRemoveDesks());

  base::AutoReset<bool> in_progress(&are_desks_being_modified_, true);

  auto iter = std::find_if(
      desks_.begin(), desks_.end(),
      [desk](const std::unique_ptr<Desk>& d) { return d.get() == desk; });
  DCHECK(iter != desks_.end());

  // Used by accessibility to indicate the desk that has been removed.
  const int removed_desk_number = std::distance(desks_.begin(), iter) + 1;

  // Keep the removed desk alive until the end of this function.
  std::unique_ptr<Desk> removed_desk = std::move(*iter);
  DCHECK_EQ(removed_desk.get(), desk);
  auto iter_after = desks_.erase(iter);

  DCHECK(!desks_.empty());

  auto* overview_controller = Shell::Get()->overview_controller();
  const bool in_overview = overview_controller->InOverviewSession();
  const std::vector<aura::Window*> removed_desk_windows =
      removed_desk->windows();

  // No need to spend time refreshing the mini_views of the removed desk.
  auto removed_desk_mini_views_pauser =
      removed_desk->GetScopedNotifyContentChangedDisabler();

  // - Move windows in removed desk (if any) to the currently active desk.
  // - If the active desk is the one being removed, activate the desk to its
  //   left, if no desk to the left, activate one on the right.
  const bool will_switch_desks = (removed_desk.get() == active_desk_);
  if (!will_switch_desks) {
    // We will refresh the mini_views of the active desk only once at the end.
    auto active_desk_mini_view_pauser =
        active_desk_->GetScopedNotifyContentChangedDisabler();

    removed_desk->MoveWindowsToDesk(active_desk_);

    MaybeUpdateShelfItems({}, removed_desk_windows);

    // If overview mode is active, we add the windows of the removed desk to the
    // overview grid in the order of the new MRU (which changes after removing a
    // desk by making the windows of the removed desk as the least recently used
    // across all desks). Note that this can only be done after the windows have
    // moved to the active desk in `MoveWindowsToDesk()` above, so that building
    // the window MRU list should contain those windows.
    if (in_overview)
      AppendWindowsToOverview(removed_desk_windows);
  } else {
    Desk* target_desk = nullptr;
    if (iter_after == desks_.begin()) {
      // Nothing before this desk.
      target_desk = (*iter_after).get();
    } else {
      // Back up to select the desk on the left.
      target_desk = (*(--iter_after)).get();
    }

    DCHECK(target_desk);

    // The target desk, which is about to become active, will have its
    // mini_views refreshed at the end.
    auto target_desk_mini_view_pauser =
        target_desk->GetScopedNotifyContentChangedDisabler();

    // Exit split view if active, before activating the new desk. We will
    // restore the split view state of the newly activated desk at the end.
    for (aura::Window* root_window : Shell::GetAllRootWindows()) {
      SplitViewController::Get(root_window)
          ->EndSplitView(SplitViewController::EndReason::kDesksChange);
    }

    // The removed desk is still the active desk, so temporarily remove its
    // windows from the overview grid which will result in removing the
    // "OverviewModeLabel" widgets created by overview mode for these windows.
    // This way the removed desk tracks only real windows, which are now ready
    // to be moved to the target desk.
    if (in_overview)
      RemoveAllWindowsFromOverview();

    // If overview mode is active, change desk activation without changing
    // window activation. Activation should remain on the dummy
    // "OverviewModeFocusedWidget" while overview mode is active.
    removed_desk->MoveWindowsToDesk(target_desk);
    ActivateDesk(target_desk, DesksSwitchSource::kDeskRemoved);

    // Desk activation should not change overview mode state.
    DCHECK_EQ(in_overview, overview_controller->InOverviewSession());

    // Now that the windows from the removed and target desks merged, add them
    // all to the grid in the order of the new MRU.
    if (in_overview)
      AppendWindowsToOverview(target_desk->windows());
  }

  // It's OK now to refresh the mini_views of *only* the active desk, and only
  // if windows from the removed desk moved to it.
  DCHECK(active_desk_->should_notify_content_changed());
  if (!removed_desk_windows.empty())
    active_desk_->NotifyContentChanged();

  UpdateDesksDefaultNames();

  for (auto& observer : observers_)
    observer.OnDeskRemoved(removed_desk.get());

  available_container_ids_.push(removed_desk->container_id());

  // Avoid having stale backdrop state as a desk is removed while in overview
  // mode, since the backdrop controller won't update the backdrop window as
  // the removed desk's windows move out from the container. Therefore, we need
  // to update it manually.
  if (in_overview)
    removed_desk->UpdateDeskBackdrops();

  // Restoring split view may start or end overview mode, therefore do this at
  // the end to avoid getting into a bad state.
  if (will_switch_desks)
    MaybeRestoreSplitView(/*refresh_snapped_windows=*/true);

  UMA_HISTOGRAM_ENUMERATION(kRemoveDeskHistogramName, source);
  ReportDesksCountHistogram();
  ReportNumberOfWindowsPerDeskHistogram();

  int active_desk_number = GetDeskIndex(active_desk_) + 1;
  if (active_desk_number == removed_desk_number)
    active_desk_number++;
  Shell::Get()
      ->accessibility_controller()
      ->TriggerAccessibilityAlertWithMessage(l10n_util::GetStringFUTF8(
          IDS_ASH_VIRTUAL_DESKS_ALERT_DESK_REMOVED, removed_desk->name(),
          active_desk_->name()));

  desks_restore_util::UpdatePrimaryUserDesksPrefs();

  DCHECK_LE(available_container_ids_.size(), desks_util::kMaxNumberOfDesks);
}

const Desk* DesksController::FindDeskOfWindow(aura::Window* window) const {
  DCHECK(window);

  for (const auto& desk : desks_) {
    if (base::Contains(desk->windows(), window))
      return desk.get();
  }

  return nullptr;
}

void DesksController::ReportNumberOfWindowsPerDeskHistogram() const {
  for (size_t i = 0; i < desks_.size(); ++i) {
    const size_t windows_count = desks_[i]->windows().size();
    switch (i) {
      case 0:
        UMA_HISTOGRAM_COUNTS_100(kNumberOfWindowsOnDesk_1_HistogramName,
                                 windows_count);
        break;

      case 1:
        UMA_HISTOGRAM_COUNTS_100(kNumberOfWindowsOnDesk_2_HistogramName,
                                 windows_count);
        break;

      case 2:
        UMA_HISTOGRAM_COUNTS_100(kNumberOfWindowsOnDesk_3_HistogramName,
                                 windows_count);
        break;

      case 3:
        UMA_HISTOGRAM_COUNTS_100(kNumberOfWindowsOnDesk_4_HistogramName,
                                 windows_count);
        break;

      default:
        NOTREACHED();
        break;
    }
  }
}

void DesksController::ReportDesksCountHistogram() const {
  DCHECK_LE(desks_.size(), desks_util::kMaxNumberOfDesks);
  UMA_HISTOGRAM_EXACT_LINEAR(kDesksCountHistogramName, desks_.size(),
                             desks_util::kMaxNumberOfDesks);
}

void DesksController::UpdateDesksDefaultNames() {
  size_t i = 0;
  for (auto& desk : desks_) {
    // Do not overwrite user-modified desks' names.
    if (!desk->is_name_set_by_user())
      desk->SetName(GetDeskDefaultName(i), /*set_by_user=*/false);
    i++;
  }
}

}  // namespace ash
