// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/wm/desks/desks_controller.h"

#include <algorithm>
#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/system/toast_data.h"
#include "ash/public/cpp/system/toast_manager.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_animation_base.h"
#include "ash/wm/desks/desk_animation_impl.h"
#include "ash/wm/desks/desk_bar_controller.h"
#include "ash/wm/desks/desks_animations.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/desks/desks_restore_util.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/templates/saved_desk_dialog_controller.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/scoped_overview_hide_windows.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/switchable_windows.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_restore/window_restore_controller.h"
#include "ash/wm/window_restore/window_restore_util.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "base/auto_reset.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/utils/haptics_util.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/app_restore/window_properties.h"
#include "components/user_manager/user_manager.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// Used to initialize toasts to undo desk removal with different IDs.
unsigned int g_close_desk_toast_counter = 0;

constexpr char kDesksCountHistogramName[] = "Ash.Desks.DesksCount3";
constexpr char kWeeklyActiveDesksHistogramName[] =
    "Ash.Desks.WeeklyActiveDesks";
constexpr char kMoveWindowFromActiveDeskHistogramName[] =
    "Ash.Desks.MoveWindowFromActiveDesk";
constexpr char kCloseAllUndoHistogramName[] = "Ash.Desks.CloseAllUndo";
constexpr char kCloseAllTotalHistogramName[] = "Ash.Desks.CloseAllTotal";
constexpr char kRemoveDeskTypeHistogramName[] = "Ash.Desks.RemoveDeskType";
constexpr char kDeskApiRemoveDeskTypeHistogramName[] =
    "Ash.DeskApi.RemoveDeskType";
constexpr char kDeskApiCloseAllUndoHistogramName[] = "Ash.DeskApi.CloseAllUndo";
constexpr char kNumberOfWindowsClosed[] = "Ash.Desks.NumberOfWindowsClosed2";
constexpr char kNumberOfWindowsClosedByButton[] =
    "Ash.Desks.NumberOfWindowsClosed2.Button";
constexpr char kNumberOfWindowsClosedByKeyboard[] =
    "Ash.Desks.NumberOfWindowsClosed2.Keyboard";
constexpr char kNumberOfWindowsClosedByApi[] =
    "Ash.Desks.NumberOfWindowsClosed2.Api";
// Used for histograms from "Ash.Desks.NumberOfWindowsOnDesk_1" up to 16.
constexpr char kNumberOfWindowsOnDeskHistogramPrefix[] =
    "Ash.Desks.NumberOfWindowsOnDesk_";

constexpr char kNumberOfDeskTraversalsHistogramName[] =
    "Ash.Desks.NumberOfDeskTraversals";
constexpr int kDeskTraversalsMaxValue = 20;

// After an desk activation animation starts,
// |kNumberOfDeskTraversalsHistogramName| will be recorded after this time
// interval.
constexpr base::TimeDelta kDeskTraversalsTimeout = base::Seconds(5);

// The timeout duration that we allow an app window on a closed desk to run
// its "close" hooks before being forcefully closed.
base::TimeDelta g_close_all_window_close_timeout = base::Seconds(1);

constexpr int kDeskDefaultNameIds[] = {
    IDS_ASH_DESKS_DESK_1_MINI_VIEW_TITLE,
    IDS_ASH_DESKS_DESK_2_MINI_VIEW_TITLE,
    IDS_ASH_DESKS_DESK_3_MINI_VIEW_TITLE,
    IDS_ASH_DESKS_DESK_4_MINI_VIEW_TITLE,
    IDS_ASH_DESKS_DESK_5_MINI_VIEW_TITLE,
    IDS_ASH_DESKS_DESK_6_MINI_VIEW_TITLE,
    IDS_ASH_DESKS_DESK_7_MINI_VIEW_TITLE,
    IDS_ASH_DESKS_DESK_8_MINI_VIEW_TITLE,
    IDS_ASH_DESKS_DESK_9_MINI_VIEW_TITLE,
    IDS_ASH_DESKS_DESK_10_MINI_VIEW_TITLE,
    IDS_ASH_DESKS_DESK_11_MINI_VIEW_TITLE,
    IDS_ASH_DESKS_DESK_12_MINI_VIEW_TITLE,
    IDS_ASH_DESKS_DESK_13_MINI_VIEW_TITLE,
    IDS_ASH_DESKS_DESK_14_MINI_VIEW_TITLE,
    IDS_ASH_DESKS_DESK_15_MINI_VIEW_TITLE,
    IDS_ASH_DESKS_DESK_16_MINI_VIEW_TITLE,
};

// Appends the given |windows| to the end of the currently active overview mode
// session such that the most-recently used window is added first. If
// The windows will animate to their positions in the overview grid.
void AppendWindowsToOverview(
    const std::vector<raw_ptr<aura::Window, VectorExperimental>>& windows) {
  DCHECK(Shell::Get()->overview_controller()->InOverviewSession());

  // TODO(dandersson): See if we can remove this code and just let
  // OverviewSession do its thing when the windows are moved to the new desk.
  auto* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  overview_session->set_auto_add_windows_enabled(false);
  for (aura::Window* window :
       Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk)) {
    if (!base::Contains(windows, window) ||
        window_util::ShouldExcludeForOverview(window)) {
      continue;
    }

    overview_session->AppendItem(window, /*reposition=*/true, /*animate=*/true);
  }
  overview_session->set_auto_add_windows_enabled(true);
}

// Removes all the items that currently exist in overview.
void RemoveAllWindowsFromOverview() {
  DCHECK(Shell::Get()->overview_controller()->InOverviewSession());

  auto* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  for (const auto& grid : overview_session->grid_list()) {
    while (!grid->empty()) {
      OverviewItemBase* overview_item = grid->item_list()[0].get();

      // We want to restore the window here primarily because when we are
      // undoing the removal of an active desk outside of overview, we do not
      // want those windows to still be in its overview transformation, so we
      // need to restore the transformation of the desk's windows before we
      // remove the respective overview items from the grid. We can also do this
      // in the case of combining desks, as in that case we will be applying the
      // overview transformation to the windows again when appending those
      // windows back to the overview grid regardless of whether those windows
      // were already in that transformation.
      overview_item->RestoreWindow(/*reset_transform=*/true, /*animate=*/false);
      overview_session->RemoveItem(overview_item);
    }
  }

  // Clear the hidden windows for saved desks grid so that when we reach the
  // new desk, the hidden windows will be handled correctly.
  ScopedOverviewHideWindows* hide_windows =
      overview_session->hide_windows_for_saved_desks_grid();
  if (hide_windows)
    hide_windows->RemoveAllWindows();
}

// Updates the |ShelfItem::is_on_active_desk| of the items associated with
// |windows_on_inactive_desk| and |windows_on_active_desk|. The items of the
// given windows will be updated, while the rest will remain unchanged. Either
// or both window lists can be empty.
void MaybeUpdateShelfItems(
    const std::vector<raw_ptr<aura::Window, VectorExperimental>>&
        windows_on_inactive_desk,
    const std::vector<raw_ptr<aura::Window, VectorExperimental>>&
        windows_on_active_desk) {
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

  for (aura::Window* window : windows_on_inactive_desk) {
    add_shelf_item_update(window, /*is_on_active_desk=*/false);
  }
  for (aura::Window* window : windows_on_active_desk) {
    add_shelf_item_update(window, /*is_on_active_desk=*/true);
  }

  shelf_model->UpdateItemsForDeskChange(shelf_items_updates);
}

bool IsParentSwitchableContainer(const aura::Window* window) {
  DCHECK(window);
  return window->parent() && IsSwitchableContainer(window->parent());
}

bool IsApplistActiveInTabletMode(const aura::Window* active_window) {
  DCHECK(active_window);
  if (!display::Screen::GetScreen()->InTabletMode()) {
    return false;
  }

  auto* app_list_controller = Shell::Get()->app_list_controller();
  return active_window == app_list_controller->GetWindow();
}

void ShowDeskRemovalUndoToast(const std::string& toast_id,
                              base::RepeatingClosure dismiss_callback,
                              base::OnceClosure expired_callback,
                              bool use_persistent_toast) {
  // If ChromeVox is enabled, then we want the toast to be infinite duration.
  ToastData undo_toast_data(
      toast_id, ash::ToastCatalogName::kUndoCloseAll,
      l10n_util::GetStringUTF16(IDS_ASH_DESKS_CLOSE_ALL_TOAST_TEXT),
      use_persistent_toast ? ToastData::kInfiniteDuration
                           : ToastData::kDefaultToastDuration,
      /*visible_on_lock_screen=*/false,
      /*has_dismiss_button=*/true,
      l10n_util::GetStringUTF16(IDS_ASH_DESKS_CLOSE_ALL_UNDO_TEXT));
  undo_toast_data.persist_on_hover = true;
  undo_toast_data.show_on_all_root_windows = true;
  undo_toast_data.dismiss_callback = std::move(dismiss_callback);
  undo_toast_data.expired_callback = std::move(expired_callback);
  undo_toast_data.activatable = use_persistent_toast;
  ToastManager::Get()->Show(std::move(undo_toast_data));
}

AccountId GetPrimaryUserAccountId() {
  return Shell::Get()
      ->session_controller()
      ->GetPrimaryUserSession()
      ->user_info.account_id;
}

}  // namespace

// Class that can hold the data for a removed desk while it waits for a user
// to confirm its deletion.
class DesksController::RemovedDeskData {
 public:
  RemovedDeskData(std::unique_ptr<Desk> desk,
                  bool was_active,
                  int index,
                  DesksCreationRemovalSource source,
                  DeskCloseType type)
      : toast_id_(base::StringPrintf("UndoCloseAllToast_%d",
                                     ++g_close_desk_toast_counter)),
        was_active_(was_active),
        desk_(std::move(desk)),
        index_(index),
        is_toast_persistent_(Shell::Get()
                                 ->accessibility_controller()
                                 ->spoken_feedback()
                                 .enabled()),
        source_(source),
        desk_close_type_(type) {
    ::full_restore::SaveRemovingDeskGuid(desk_->uuid());
    desk_->set_is_desk_being_removed(true);
  }

  RemovedDeskData(const RemovedDeskData&) = delete;
  RemovedDeskData& operator=(const RemovedDeskData&) = delete;

  ~RemovedDeskData() {
    // `toast_manager` gets destroyed before `DesksController` so we want to
    // make sure it still exists before we try to call `Cancel` on it.
    auto* toast_manager = ToastManager::Get();
    if (toast_manager && desk_) {
      toast_manager->Cancel(toast_id_);
      DesksController::Get()->FinalizeDeskRemoval(this);
    }
    ::full_restore::ResetRemovingDeskGuid();
  }

  const std::string& toast_id() const { return toast_id_; }
  bool was_active() const { return was_active_; }
  Desk* desk() { return desk_.get(); }
  int index() const { return index_; }
  bool is_toast_persistent() const { return is_toast_persistent_; }
  DesksCreationRemovalSource desk_removal_source() const { return source_; }
  DeskCloseType desk_close_type() const { return desk_close_type_; }

  std::unique_ptr<Desk> AcquireDesk() { return std::move(desk_); }

 private:
  const std::string toast_id_;
  const bool was_active_;
  std::unique_ptr<Desk> desk_;
  const int index_;

  // If this was created in overview with ChromeVox enabled and then ChromeVox
  // is exited before this is destroyed, then we still need to know to destroy
  // it when overview closes.
  const bool is_toast_persistent_;

  const DesksCreationRemovalSource source_;

  const DeskCloseType desk_close_type_;
};

// Helper class which wraps around a OneShotTimer and used for recording how
// many times the user has traversed desks. Here traversal means the amount of
// times the user has seen a visual desk change. This differs from desk
// activation as a desk is only activated as needed for a screenshot during an
// animation. The user may bounce back and forth on two desks that already
// have screenshots, and each bounce is recorded as a traversal. For touchpad
// swipes, the amount of traversals in one animation depends on the amount of
// changes in the most visible desk have been seen. For other desk changes,
// the amount of traversals in one animation is 1 + number of Replace() calls.
// Multiple animations may be recorded before the timer stops.
class DesksController::DeskTraversalsMetricsHelper {
 public:
  explicit DeskTraversalsMetricsHelper(DesksController* controller)
      : controller_(controller) {}
  DeskTraversalsMetricsHelper(const DeskTraversalsMetricsHelper&) = delete;
  DeskTraversalsMetricsHelper& operator=(const DeskTraversalsMetricsHelper&) =
      delete;
  ~DeskTraversalsMetricsHelper() = default;

  // Starts |timer_| unless it is already running.
  void MaybeStart() {
    if (timer_.IsRunning())
      return;

    count_ = 0;
    timer_.Start(FROM_HERE, kDeskTraversalsTimeout,
                 base::BindOnce(&DeskTraversalsMetricsHelper::OnTimerStop,
                                base::Unretained(this)));
  }

  // Called when a desk animation is finished. Adds all observed
  // |visible_desk_changes| to |count_|.
  void OnAnimationFinished(int visible_desk_changes) {
    if (timer_.IsRunning())
      count_ += visible_desk_changes;
  }

  // Fires |timer_| immediately.
  void FireTimerForTesting() {
    if (timer_.IsRunning())
      timer_.FireNow();
  }

 private:
  void OnTimerStop() {
    // If an animation is still running, add its current visible desk change
    // count to |count_|.
    DeskAnimationBase* current_animation = controller_->animation();
    if (current_animation)
      count_ += current_animation->visible_desk_changes();

    base::UmaHistogramExactLinear(kNumberOfDeskTraversalsHistogramName, count_,
                                  kDeskTraversalsMaxValue);
  }

  // Pointer to the DesksController that owns this. Guaranteed to be not
  // nullptr for the lifetime of |this|.
  const raw_ptr<DesksController> controller_;

  base::OneShotTimer timer_;

  // Tracks the amount of traversals that have happened since |timer_| has
  // started.
  int count_ = 0;
};

DesksController::DesksController()
    : metrics_helper_(std::make_unique<DeskTraversalsMetricsHelper>(this)) {
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

  weekly_active_desks_scheduler_.Start(
      FROM_HERE, base::Days(7), this,
      &DesksController::RecordAndResetNumberOfWeeklyActiveDesks);

  if (ash::features::IsDeskButtonEnabled()) {
    desk_bar_controller_ = std::make_unique<DeskBarController>();
  }
}

DesksController::~DesksController() {
  Shell::Get()->session_controller()->RemoveObserver(this);
  Shell::Get()->activation_client()->RemoveObserver(this);
}

// static
DesksController* DesksController::Get() {
  // Sometimes it's necessary to get the instance even before the
  // constructor is done. For example,
  // |DesksController::NotifyDeskNameChanged())| could be called
  // during the construction of |DesksController|, and at this point
  // |Shell::desks_controller_| has not been assigned yet.
  return static_cast<DesksController*>(chromeos::DesksHelper::Get(nullptr));
}

// static
std::u16string DesksController::GetDeskDefaultName(size_t desk_index) {
  DCHECK_LT(desk_index, desks_util::GetMaxNumberOfDesks());
  return l10n_util::GetStringUTF16(kDeskDefaultNameIds[desk_index]);
}

const std::u16string& DesksController::GetCombineDesksTargetName(
    const Desk* desk) const {
  if (desk == active_desk_ && desks_.size() > 1) {
    Desk* target = GetPreviousDesk();

    if (!target)
      target = GetNextDesk();

    DCHECK(target);
    return target->name();
  }

  return active_desk_->name();
}

const Desk* DesksController::GetTargetActiveDesk() const {
  const Desk* target_desk = nullptr;
  if (animation_) {
    // If there is ongoing animation, return the target of the animation.
    target_desk = desks_[animation_->ending_desk_index()].get();
  } else if (desk_to_activate_) {
    // Even if there is no ongoing animation, it's still possible to be in the
    // middle of a desk switch.
    // Please refer to b/266147233.
    target_desk = desk_to_activate_;
  } else {
    target_desk = active_desk();
  }

  DCHECK(HasDesk(target_desk));
  return target_desk;
}

base::flat_set<aura::Window*>
DesksController::GetVisibleOnAllDesksWindowsOnRoot(
    aura::Window* root_window) const {
  DCHECK(root_window->IsRootWindow());
  base::flat_set<aura::Window*> filtered_visible_on_all_desks_windows;
  for (aura::Window* visible_on_all_desks_window :
       visible_on_all_desks_windows_) {
    if (visible_on_all_desks_window->GetRootWindow() == root_window)
      filtered_visible_on_all_desks_windows.insert(visible_on_all_desks_window);
  }
  return filtered_visible_on_all_desks_windows;
}

void DesksController::RestorePrimaryUserActiveDeskIndex(int active_desk_index) {
  DCHECK_GE(active_desk_index, 0);
  DCHECK_LT(active_desk_index, static_cast<int>(desks_.size()));
  user_to_active_desk_index_[GetPrimaryUserAccountId()] = active_desk_index;
  ActivateDesk(desks_[active_desk_index].get(),
               DesksSwitchSource::kDeskRestored);
}

void DesksController::OnNewUserShown() {
  RestackVisibleOnAllDesksWindowsOnActiveDesk();
  NotifyAllDesksForContentChanged();
}

void DesksController::Shutdown() {
  desk_bar_controller_.reset();
  animation_.reset();
  desks_restore_util::UpdatePrimaryUserDeskMetricsPrefs();
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
  return desks_.size() < desks_util::GetMaxNumberOfDesks();
}

Desk* DesksController::GetNextDesk(bool use_target_active_desk) const {
  int next_index = use_target_active_desk ? GetDeskIndex(GetTargetActiveDesk())
                                          : GetActiveDeskIndex();
  if (++next_index >= static_cast<int>(desks_.size()))
    return nullptr;
  return desks_[next_index].get();
}

Desk* DesksController::GetPreviousDesk(bool use_target_active_desk) const {
  int previous_index = use_target_active_desk
                           ? GetDeskIndex(GetTargetActiveDesk())
                           : GetActiveDeskIndex();
  if (--previous_index < 0)
    return nullptr;
  return desks_[previous_index].get();
}

Desk* DesksController::GetDeskByUuid(const base::Uuid& desk_uuid) const {
  auto it = base::ranges::find(desks_, desk_uuid, &Desk::uuid);
  return it != desks_.end() ? it->get() : nullptr;
}

int DesksController::GetDeskIndexByUuid(const base::Uuid& desk_uuid) const {
  for (size_t i = 0; i < desks_.size(); ++i) {
    if (desk_uuid == desks_[i]->uuid()) {
      return i;
    }
  }
  return -1;
}

bool DesksController::CanRemoveDesks() const {
  return desks_.size() > 1;
}

void DesksController::NewDesk(DesksCreationRemovalSource source) {
  NewDesk(source, std::u16string());
}

void DesksController::NewDesk(DesksCreationRemovalSource source,
                              std::u16string name) {
  // We want to destroy the `temporary_removed_desk_` first in this
  // function because we want to ensure that the removing desk's container is
  // available for use if we need it for the new desk.
  MaybeCommitPendingDeskRemoval();

  DCHECK(CanCreateDesks());
  DCHECK(!available_container_ids_.empty());

  base::AutoReset<bool> in_progress(&are_desks_being_modified_, true);

  // The first default desk should not overwrite any desks restore data, nor
  // should it trigger any UMA stats reports.
  const bool is_first_ever_desk = desks_.empty();

  desks_.push_back(std::make_unique<Desk>(
      available_container_ids_.front(),
      source == DesksCreationRemovalSource::kDesksRestore));
  available_container_ids_.pop();
  Desk* new_desk = desks_.back().get();

  // We should notify observers that the desk is added before possibly
  // notifying observers that the name is set.
  for (auto& observer : observers_) {
    observer.OnDeskAdded(new_desk, /*from_undo=*/false);
  }

  if (name.empty()) {
    // The new desk should have an empty name when the user creates a desk with
    // a button. This is done to encourage them to rename their desks.
    const bool empty_name =
        (source == DesksCreationRemovalSource::kButton ||
         source == DesksCreationRemovalSource::kDeskButtonDeskBarButton) &&
        desks_.size() > 1;
    if (!empty_name) {
      new_desk->SetName(GetDeskDefaultName(desks_.size() - 1),
                        /*set_by_user=*/false);
    }
  } else {
    new_desk->SetName(std::move(name), /*set_by_user=*/true);
  }

  // Don't trigger an a11y alert when the source is `kLaunchTemplate` because
  // `CreateNewDeskForSavedDesk` will trigger an alert instead.
  // Dont trigger when the source is `kSaveAndRecall` because the
  // `DESK_TEMPLATES_MODE_ENTERED` alert triggered in
  // `OverviewSession::ShowSavedDeskLibrary` should be shown instead.
  if (source != DesksCreationRemovalSource::kLaunchTemplate &&
      source != DesksCreationRemovalSource::kSaveAndRecall &&
      source != DesksCreationRemovalSource::kFloatingWorkspace) {
    auto* shell = Shell::Get();
    shell->accessibility_controller()->TriggerAccessibilityAlertWithMessage(
        l10n_util::GetStringFUTF8(IDS_ASH_VIRTUAL_DESKS_ALERT_NEW_DESK_CREATED,
                                  base::NumberToString16(desks_.size())));
  }

  if (!is_first_ever_desk) {
    if (features::IsDeskButtonEnabled()) {
      PrefService* prefs =
          Shell::Get()->session_controller()->GetLastActiveUserPrefService();
      // Record that virtual desks have been used on this device so we can show
      // the desk button from now on, if the user doesn't and hasn't elected to
      // hide it from the shelf context menu.
      SetDeviceUsesDesksPref(prefs, true);
    }
    desks_restore_util::UpdatePrimaryUserDeskGuidsPrefs();
    desks_restore_util::UpdatePrimaryUserDeskNamesPrefs();
    desks_restore_util::UpdatePrimaryUserDeskLacrosProfileIdPrefs();
    desks_restore_util::UpdatePrimaryUserDeskMetricsPrefs();
    UMA_HISTOGRAM_ENUMERATION(kNewDeskHistogramName, source);
    ReportDesksCountHistogram();
  }
}

bool DesksController::HasDesk(const Desk* desk) const {
  return base::Contains(desks_, desk, &std::unique_ptr<Desk>::get);
}

Desk* DesksController::GetDeskAtIndex(size_t index) const {
  CHECK_LT(index, desks_.size());
  return desks_[index].get();
}

void DesksController::RemoveDesk(const Desk* desk,
                                 DesksCreationRemovalSource source,
                                 DeskCloseType close_type) {
  DCHECK(CanRemoveDesks());
  DCHECK(HasDesk(desk));

  base::AutoReset<bool> in_progress(&are_desks_being_modified_, true);

  auto* overview_controller = Shell::Get()->overview_controller();
  const bool in_overview = overview_controller->InOverviewSession();
  if (!in_overview && active_desk_ == desk &&
      source != DesksCreationRemovalSource::kSaveAndRecall) {
    // When removing the active desk outside of overview (and the source is not
    // Save & Recall), we trigger the remove desk animation. We will activate
    // the desk to its left if any, otherwise, we activate one on the right.
    const int current_desk_index = GetDeskIndex(active_desk_);
    const int target_desk_index =
        current_desk_index + ((current_desk_index > 0) ? -1 : 1);
    DCHECK_GE(target_desk_index, 0);
    DCHECK_LT(target_desk_index, static_cast<int>(desks_.size()));

    // Collect metrics now because the `DeskRemovalAnimation` will skip the
    // logging in `ActivateDesk` and go straight to `ActivateDeskInternal`.
    base::UmaHistogramEnumeration(
        kDeskSwitchHistogramName,
        source == DesksCreationRemovalSource::kDeskButtonDeskBarButton
            ? DesksSwitchSource::kDeskButtonDeskRemoved
            : DesksSwitchSource::kDeskRemoved);

    animation_ = std::make_unique<DeskRemovalAnimation>(
        this, current_desk_index, target_desk_index, source, close_type);
    animation_->Launch();
    return;
  }

  RemoveDeskInternal(desk, source, close_type, /*desk_switched=*/false);
}

void DesksController::ReorderDesk(int old_index, int new_index) {
  DCHECK_NE(old_index, new_index);
  DCHECK_GE(old_index, 0);
  DCHECK_GE(new_index, 0);
  DCHECK_LT(old_index, static_cast<int>(desks_.size()));
  DCHECK_LT(new_index, static_cast<int>(desks_.size()));
  desks_util::ReorderItem(desks_, old_index, new_index);

  for (auto& observer : observers_)
    observer.OnDeskReordered(old_index, new_index);

  // Since multi-profile users share the same desks, the active user needs to
  // update the desk name list to maintain the right desk order for restore
  // and update workspaces of windows in all affected desks across all profiles.
  // Meanwhile, only the primary user needs to update the active desk, which is
  // independent across profiles but only recoverable for the primary user.

  // 1. Update desk name and metrics lists in the user prefs to maintain the
  // right order.
  desks_restore_util::UpdatePrimaryUserDeskNamesPrefs();
  desks_restore_util::UpdatePrimaryUserDeskGuidsPrefs();
  desks_restore_util::UpdatePrimaryUserDeskLacrosProfileIdPrefs();
  desks_restore_util::UpdatePrimaryUserDeskMetricsPrefs();

  // 2. For multi-profile switching, update all affected active desk index in
  // |user_to_active_desk_index_|.
  const int starting_affected_index = std::min(old_index, new_index);
  const int ending_affected_index = std::max(old_index, new_index);
  // If the user move a desk to the back, other affected desks in between the
  // two positions shift left (-1), otherwiser shift right (+1).
  const int offset = new_index > old_index ? -1 : 1;

  for (auto& [account_id, desk_index] : user_to_active_desk_index_) {
    const int old_active_index = desk_index;
    if (old_active_index < starting_affected_index ||
        old_active_index > ending_affected_index) {
      // Skip unaffected desk index.
      continue;
    }
    // The moving desk changes from old_index to new_index, while other desks
    // between the two positions shift by one position.
    desk_index =
        old_active_index == old_index ? new_index : old_active_index + offset;
  }

  // 3. For primary user's active desks restore, update the active desk index.
  desks_restore_util::UpdatePrimaryUserActiveDeskPrefs(
      user_to_active_desk_index_[GetPrimaryUserAccountId()]);

  // 4. For restoring windows to the right desks, update workspaces of all
  // windows in the affected desks for all simultaneously logged-in users.
  for (int i = starting_affected_index; i <= ending_affected_index; i++) {
    for (aura::Window* window : desks_[i]->windows()) {
      if (desks_util::IsWindowVisibleOnAllWorkspaces(window))
        continue;
      window->SetProperty(aura::client::kWindowWorkspaceKey, i);
    }
  }
}

void DesksController::ActivateDesk(const Desk* desk, DesksSwitchSource source) {
  DCHECK(HasDesk(desk));
  DCHECK(!animation_);

  // If we are switching users, we don't want to notify desks of content changes
  // until the user switch animation has shown the new user's windows.
  const bool is_user_switch = source == DesksSwitchSource::kUserSwitch ||
                              source == DesksSwitchSource::kDeskRestored;
  std::optional<Desk::ScopedContentUpdateNotificationDisabler>
      desks_scoped_notify_disabler;
  if (is_user_switch) {
    desks_scoped_notify_disabler.emplace(/*desks=*/desks_,
                                         /*notify_when_destroyed=*/false);
  }

  OverviewController* overview_controller = Shell::Get()->overview_controller();
  const bool in_overview = overview_controller->InOverviewSession();
  if (desk == active_desk_) {
    if (in_overview) {
      // Selecting the active desk's mini_view in overview mode is allowed and
      // should just exit overview mode normally. Immediately exit overview if
      // switching to a new user, otherwise the multi user switch animation will
      // animate the same windows that overview watches to determine if the
      // overview shutdown animation is complete. See https://crbug.com/1001586.
      overview_controller->EndOverview(
          OverviewEndAction::kDeskActivation,
          is_user_switch ? OverviewEnterExitType::kImmediateExit
                         : OverviewEnterExitType::kNormal);
    }
    // Selecting the active desk in desk button desk bar is allowed, and
    // should just close all desk bars.
    if (desk_bar_controller_) {
      desk_bar_controller_->CloseAllDeskBars();
    }
    return;
  }

  UMA_HISTOGRAM_ENUMERATION(kDeskSwitchHistogramName, source);

  const int target_desk_index = GetDeskIndex(desk);
  if (source != DesksSwitchSource::kDeskRemoved &&
      source != DesksSwitchSource::kDeskButtonDeskRemoved) {
    // Desk removal has its own a11y alert.
    Shell::Get()
        ->accessibility_controller()
        ->TriggerAccessibilityAlertWithMessage(l10n_util::GetStringFUTF8(
            IDS_ASH_VIRTUAL_DESKS_ALERT_DESK_ACTIVATED, desk->name()));
  }

  if (source == DesksSwitchSource::kDeskRemoved ||
      source == DesksSwitchSource::kDeskButtonDeskRemoved ||
      (source == DesksSwitchSource::kRemovalUndone && in_overview) ||
      is_user_switch) {
    // Desk switches due to desks removal, undoing the removal of an active desk
    // in overview mode, or user switches in a multi-profile session result in
    // immediate desk activation without animation.
    ActivateDeskInternal(desk, /*update_window_activation=*/!in_overview);
    return;
  }

  if (source == DesksSwitchSource::kLaunchTemplate) {
    // Desk switch due to launching a template will immediately activate the new
    // desk without animation.
    ActivateDeskInternal(desk, /*update_window_activation=*/true);
    return;
  }

  // When switching desks we want to update window activation when leaving
  // overview or if nothing was active prior to switching desks. This will
  // ensure that after switching desks, we will try to focus a candidate window.
  // We will also update window activation if the currently active window is one
  // in a switchable container. Otherwise, do not update the window activation.
  // This will prevent some ephemeral system UI surfaces such as the app list
  // and system tray from closing when switching desks. An exception is the app
  // list in tablet mode, which should gain activation when there are no
  // windows, as it is treated like a bottom stacked window.
  aura::Window* active_window = window_util::GetActiveWindow();
  const bool update_window_activation =
      in_overview || !active_window ||
      IsParentSwitchableContainer(active_window) ||
      IsApplistActiveInTabletMode(active_window);

  const int starting_desk_index = GetDeskIndex(active_desk());
  animation_ = std::make_unique<DeskActivationAnimation>(
      this, starting_desk_index, target_desk_index, source,
      update_window_activation);
  animation_->Launch();

  metrics_helper_->MaybeStart();
}

bool DesksController::ActivateAdjacentDesk(bool going_left,
                                           DesksSwitchSource source) {
  if (Shell::Get()->session_controller()->IsUserSessionBlocked())
    return false;

  // Try replacing an ongoing desk animation of the same source.
  if (animation_) {
    if (animation_->Replace(going_left, source))
      return true;

    // We arrive here if `DeskActivationAnimation::Replace()` fails
    // due to trying to replace an animation before the original animation has
    // finished taking their screenshots. We can continue with creating a new
    // animation in `ActivateDesk()`, but we need to clean up some desk state.
    ActivateDeskInternal(desks()[animation_->ending_desk_index()].get(),
                         /*update_window_activation=*/false);
    animation_.reset();
  }

  const Desk* desk_to_activate = going_left ? GetPreviousDesk() : GetNextDesk();
  if (desk_to_activate) {
    ActivateDesk(desk_to_activate, source);
  } else {
    // Fire a haptic event if necessary.
    if (source == DesksSwitchSource::kDeskSwitchTouchpad) {
      chromeos::haptics_util::PlayHapticTouchpadEffect(
          ui::HapticTouchpadEffect::kKnock,
          ui::HapticTouchpadEffectStrength::kMedium);
    }
    for (aura::Window* root : Shell::GetAllRootWindows()) {
      desks_animations::PerformHitTheWallAnimation(root, going_left);
    }
  }

  return true;
}

bool DesksController::StartSwipeAnimation(bool move_left) {
  // Activate an adjacent desk. It will replace an ongoing touchpad animation if
  // one exists.
  return ActivateAdjacentDesk(move_left,
                              DesksSwitchSource::kDeskSwitchTouchpad);
}

void DesksController::UpdateSwipeAnimation(float scroll_delta_x) {
  if (animation_)
    animation_->UpdateSwipeAnimation(scroll_delta_x);
}

void DesksController::EndSwipeAnimation() {
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
  // the active desk, and cannot be removed. Except floated window, which is
  // handled by `FloatController::OnMovingFloatedWindowToDesk`.
  const bool is_floated = WindowState::Get(window)->IsFloated();
  if (!base::Contains(active_desk_->windows(), window) && !is_floated)
    return false;

  const bool visible_on_all_desks =
      desks_util::IsWindowVisibleOnAllWorkspaces(window);
  if (visible_on_all_desks) {
    if (source == DesksMoveWindowFromActiveDeskSource::kDragAndDrop) {
      // Since a visible on all desks window is on all desks, prevent users from
      // moving them manually in overview.
      return false;
    } else if (source !=
               DesksMoveWindowFromActiveDeskSource::kVisibleOnAllDesks) {
      window->SetProperty(aura::client::kWindowWorkspaceKey,
                          aura::client::kWindowWorkspaceUnassignedWorkspace);
    }
  }

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
    // `item` can be null when we are switching users.
    if (auto* item = overview_session->GetOverviewItemForWindow(window)) {
      item->OnMovingItemToAnotherDesk();
      // The item no longer needs to be in the overview grid.
      overview_session->RemoveItem(item);
    } else if (visible_on_all_desks) {
      // Create an item for a visible on all desks window if it doesn't have one
      // already. This can happen when launching a template. When we are in the
      // saved desk grid, there are no items.
      overview_session->AppendItem(window,
                                   /*reposition=*/true, /*animate=*/true);
    }
  }

  // Floated window doesn't belong to the desk container, float controller
  // handles its desk-window relationship.
  if (is_floated) {
    Shell::Get()->float_controller()->OnMovingFloatedWindowToDesk(
        window, active_desk_, target_desk, target_root);
  } else {
    active_desk_->MoveWindowToDesk(window, target_desk, target_root,
                                   /*unminimize=*/true);
  }

  // We don't update shelf items if we're moving an all-desk window, since we
  // are in that case about to switch to `target_desk` and the app will be on
  // that desk as well.
  if (source != DesksMoveWindowFromActiveDeskSource::kVisibleOnAllDesks) {
    MaybeUpdateShelfItems(/*windows_on_inactive_desk=*/{window},
                          /*windows_on_active_desk=*/{});
  }

  Shell::Get()
      ->accessibility_controller()
      ->TriggerAccessibilityAlertWithMessage(l10n_util::GetStringFUTF8(
          IDS_ASH_VIRTUAL_DESKS_ALERT_WINDOW_MOVED_FROM_ACTIVE_DESK,
          window->GetTitle(), active_desk_->name(), target_desk->name()));

  if (source != DesksMoveWindowFromActiveDeskSource::kVisibleOnAllDesks) {
    // A visible on all desks window is moved from the active desk to the next
    // active desk during desk switch so we log its usage (when a user pins a
    // window to all desks) in other locations.
    UMA_HISTOGRAM_ENUMERATION(kMoveWindowFromActiveDeskHistogramName, source);
  }
  ReportNumberOfWindowsPerDeskHistogram();

  // A window moving out of the active desk cannot be active.
  // If we are in overview, we should not change the window activation as we do
  // below, since the dummy "OverviewModeFocusedWidget" should remain active
  // while overview mode is active.
  if (!in_overview)
    wm::DeactivateWindow(window);
  return true;
}

void DesksController::AddVisibleOnAllDesksWindow(aura::Window* window) {
  // Now that WorkspaceLayoutManager requests
  // Add/MaybeRemoveVisibleOnAllDesksWindow when a child window is added in
  // OnWindowAddedToLayout, the window could be the one that has already been
  // added.
  if (!visible_on_all_desks_windows_.emplace(window).second)
    return;

  if (SnapGroupController* snap_group_controller = SnapGroupController::Get()) {
    if (SnapGroup* snap_group =
            snap_group_controller->GetSnapGroupForGivenWindow(window)) {
      snap_group_controller->RemoveSnapGroup(
          snap_group, SnapGroupExitPoint::kVisibleOnAllDesks);
    }
  }

  // A window is made visible on all desks by always keeping it on the active
  // desk. If `window` isn't already on the active desk, then we need to move it
  // there. We will also skip the bounce animation.
  bool do_window_bound_animation = true;
  if (const Desk* window_desk = desks_util::GetDeskForContext(window);
      window_desk != active_desk_) {
    do_window_bound_animation = false;

    // TODO(b/295371112): It is unexpected that we get here. Dump a call stack
    // to help figure out why it happens.
    base::debug::DumpWithoutCrashing();

    CHECK(window_desk);
    const_cast<Desk*>(window_desk)
        ->MoveWindowToDesk(window, active_desk_, window->GetRootWindow(),
                           /*unminimize=*/false);
  }

  TrackWindowOnAllDesks(window);

  if (do_window_bound_animation) {
    wm::AnimateWindow(window, wm::WINDOW_ANIMATION_TYPE_BOUNCE);
  }

  NotifyAllDesksForContentChanged();
  Shell::Get()
      ->accessibility_controller()
      ->TriggerAccessibilityAlertWithMessage(l10n_util::GetStringFUTF8(
          IDS_ASH_VIRTUAL_DESKS_ASSIGNED_TO_ALL_DESKS, window->GetTitle()));
}

void DesksController::MaybeRemoveVisibleOnAllDesksWindow(aura::Window* window) {
  if (visible_on_all_desks_windows_.erase(window)) {
    UntrackWindowFromAllDesks(window);

    wm::AnimateWindow(window, wm::WINDOW_ANIMATION_TYPE_BOUNCE);
    NotifyAllDesksForContentChanged();
    Shell::Get()
        ->accessibility_controller()
        ->TriggerAccessibilityAlertWithMessage(l10n_util::GetStringFUTF8(
            IDS_ASH_VIRTUAL_DESKS_UNASSIGNED_FROM_ALL_DESKS, window->GetTitle(),
            active_desk_->name()));
  }
}

void DesksController::NotifyAllDeskWindowMovedToNewRoot(aura::Window* window) {
  for (auto& desk : desks_) {
    desk->AllDeskWindowMovedToNewRoot(window);
  }
}

void DesksController::NotifyAllDesksForContentChanged() {
  for (const auto& desk : desks_)
    desk->NotifyContentChanged();
}

void DesksController::NotifyDeskNameChanged(const Desk* desk,
                                            const std::u16string& new_name) {
  // We only want metrics for users with two or more desks.
  if (desks_.size() > 1) {
    ReportCustomDeskNames();
    base::UmaHistogramBoolean(kCustomNameCreatedHistogramName,
                              desk->is_name_set_by_user());
  }

  for (auto& observer : observers_)
    observer.OnDeskNameChanged(desk, new_name);
}

void DesksController::RevertDeskNameToDefault(Desk* desk) {
  DCHECK(HasDesk(desk));
  desk->SetName(GetDeskDefaultName(GetDeskIndex(desk)), /*set_by_user=*/false);
}

void DesksController::RestoreNameOfDeskAtIndex(std::u16string name,
                                               size_t index) {
  DCHECK(!name.empty());
  DCHECK_LT(index, desks_.size());

  desks_[index]->SetName(std::move(name), /*set_by_user=*/true);
}

void DesksController::RestoreGuidOfDeskAtIndex(base::Uuid guid, size_t index) {
  DCHECK(guid.is_valid());
  DCHECK_LT(index, desks_.size());
  desks_[index]->SetGuid(std::move(guid));
}

void DesksController::RestoreCreationTimeOfDeskAtIndex(base::Time creation_time,
                                                       size_t index) {
  DCHECK_LT(index, desks_.size());

  desks_[index]->set_creation_time(creation_time);
}

void DesksController::RestoreVisitedMetricsOfDeskAtIndex(int first_day_visited,
                                                         int last_day_visited,
                                                         size_t index) {
  DCHECK_LT(index, desks_.size());
  DCHECK_GE(last_day_visited, first_day_visited);

  const auto& target_desk = desks_[index];
  target_desk->set_first_day_visited(first_day_visited);
  target_desk->set_last_day_visited(last_day_visited);
  if (!target_desk->IsConsecutiveDailyVisit())
    target_desk->RecordAndResetConsecutiveDailyVisits(/*being_removed=*/false);
}

void DesksController::RestoreWeeklyInteractionMetricOfDeskAtIndex(
    bool interacted_with_this_week,
    size_t index) {
  DCHECK_LT(index, desks_.size());

  desks_[index]->set_interacted_with_this_week(interacted_with_this_week);
}

void DesksController::RestoreWeeklyActiveDesksMetrics(int weekly_active_desks,
                                                      base::Time report_time) {
  DCHECK_GE(weekly_active_desks, 0);

  Desk::SetWeeklyActiveDesks(weekly_active_desks);

  base::TimeDelta report_time_delta(report_time - base::Time::Now());
  if (report_time_delta.InMinutes() < 0) {
    // The scheduled report time has passed so log the restored metrics and
    // reset related metrics.
    RecordAndResetNumberOfWeeklyActiveDesks();
  } else {
    // The scheduled report time has not passed so reset the existing timer to
    // go off at the scheduled report time.
    weekly_active_desks_scheduler_.Start(
        FROM_HERE, report_time_delta, this,
        &DesksController::RecordAndResetNumberOfWeeklyActiveDesks);
  }
}

base::Time DesksController::GetWeeklyActiveReportTime() const {
  return base::Time::Now() + weekly_active_desks_scheduler_.GetCurrentDelay();
}

void DesksController::OnRootWindowAdded(aura::Window* root_window) {
  for (auto& desk : desks_)
    desk->OnRootWindowAdded(root_window);

  if (temporary_removed_desk_)
    temporary_removed_desk_->desk()->OnRootWindowAdded(root_window);
}

void DesksController::OnRootWindowClosing(aura::Window* root_window) {
  for (auto& desk : desks_)
    desk->OnRootWindowClosing(root_window);

  if (temporary_removed_desk_)
    temporary_removed_desk_->desk()->OnRootWindowClosing(root_window);
}

int DesksController::GetDeskIndex(const Desk* desk) const {
  for (size_t i = 0; i < desks_.size(); ++i) {
    if (desk == desks_[i].get())
      return i;
  }

  return -1;
}

aura::Window* DesksController::GetDeskContainer(aura::Window* target_root,
                                                int desk_index) {
  if (desk_index < 0 || desk_index >= static_cast<int>(desks_.size()))
    return nullptr;
  return desks_[desk_index]->GetDeskContainerForRoot(target_root);
}

bool DesksController::BelongsToActiveDesk(aura::Window* window) {
  return desks_util::BelongsToActiveDesk(window);
}

int DesksController::GetActiveDeskIndex() const {
  return GetDeskIndex(active_desk_);
}

std::u16string DesksController::GetDeskName(int index) const {
  return index < static_cast<int>(desks_.size()) ? desks_[index]->name()
                                                 : std::u16string();
}

int DesksController::GetNumberOfDesks() const {
  return static_cast<int>(desks_.size());
}

void DesksController::SendToDeskAtIndex(aura::Window* window, int desk_index) {
  if (desk_index < 0 || desk_index >= static_cast<int>(desks_.size()))
    return;

  // If |window| is assigned to all desk, clear it since the user is manually
  // moving it to a desk.
  if (desks_util::IsWindowVisibleOnAllWorkspaces(window)) {
    window->SetProperty(aura::client::kWindowWorkspaceKey,
                        aura::client::kWindowWorkspaceUnassignedWorkspace);
  }

  const int active_desk_index = GetDeskIndex(active_desk_);
  if (desk_index == active_desk_index)
    return;

  DCHECK(desks_.at(desk_index));

  desks_animations::PerformWindowMoveToDeskAnimation(
      window, /*going_left=*/desk_index < active_desk_index);
  MoveWindowFromActiveDeskTo(window, desks_[desk_index].get(),
                             window->GetRootWindow(),
                             DesksMoveWindowFromActiveDeskSource::kSendToDesk);
}

void DesksController::CaptureActiveDeskAsSavedDesk(
    GetDeskTemplateCallback callback,
    DeskTemplateType template_type,
    aura::Window* root_window_to_show) const {
  DCHECK(current_account_id_.is_valid());

  restore_data_collector_.CaptureActiveDeskAsSavedDesk(
      std::move(callback), template_type,
      base::UTF16ToUTF8(active_desk_->name()), root_window_to_show,
      current_account_id_);
}

Desk* DesksController::CreateNewDeskForSavedDesk(
    DeskTemplateType template_type,
    const std::u16string& customized_desk_name) {
  DCHECK(CanCreateDesks());

  // If there is an ongoing animation, we should stop it before creating and
  // activating the new desk, which triggers its own animation.
  if (animation_)
    animation_.reset();

  // Call `HideSavedDeskLibrary` before the new desk is created to update the
  // state of the library button, otherwise the library button will be laid out
  // with the wrong state when the new desk is created.
  if (template_type == DeskTemplateType::kTemplate ||
      template_type == DeskTemplateType::kFloatingWorkspace) {
    if (auto* session =
            Shell::Get()->overview_controller()->overview_session()) {
      session->HideSavedDeskLibrary();
      for (auto& grid : session->grid_list()) {
        grid->RemoveAllItemsForSavedDeskLaunch();
      }
    }
  }

  switch (template_type) {
    case DeskTemplateType::kTemplate:
      NewDesk(DesksCreationRemovalSource::kLaunchTemplate);
      break;
    case DeskTemplateType::kSaveAndRecall:
      NewDesk(DesksCreationRemovalSource::kSaveAndRecall);
      break;
    case DeskTemplateType::kFloatingWorkspace:
      NewDesk(DesksCreationRemovalSource::kFloatingWorkspace);
      break;
    case DeskTemplateType::kUnknown:
      return nullptr;
  }

  Desk* desk = desks().back().get();

  // Desk name was set to a default name upon creation. If
  // `customized_desk_name` is not empty, override desk name to be
  // `customized_desk_name` or `customized_desk_name ({counter})` to resolve
  // naming conflicts.
  std::u16string desk_name = CreateUniqueDeskName(customized_desk_name);

  if (!desk_name.empty()) {
    desk->SetName(desk_name, /*set_by_user=*/true);
    Shell::Get()
        ->accessibility_controller()
        ->TriggerAccessibilityAlertWithMessage(l10n_util::GetStringFUTF8(
            IDS_ASH_VIRTUAL_DESKS_ALERT_NEW_DESK_CREATED, desk_name));
  }
  // Force update user prefs because `SetName()` does not trigger it.
  desks_restore_util::UpdatePrimaryUserDeskNamesPrefs();

  if (template_type == DeskTemplateType::kTemplate ||
      template_type == DeskTemplateType::kFloatingWorkspace) {
    // We're staying in overview mode, so move desks bar window and the
    // save desk buttons to the new desk. They would otherwise disappear
    // when the new desk is activated.
    DCHECK(active_desk_);

    // Since we're going to move certain windows from the currently active desk,
    // this is going to implicitly modify that list. We therefore grab a copy of
    // it to avoid issues with concurrent iteration and modification of the
    // list.
    auto active_desk_windows = active_desk_->windows();
    for (aura::Window* window : active_desk_windows) {
      if (window->GetProperty(kOverviewUiKey)) {
        aura::Window* destination_container =
            desk->GetDeskContainerForRoot(window->GetRootWindow());
        destination_container->AddChild(window);
      }
    }

    ActivateDesk(desk, DesksSwitchSource::kLaunchTemplate);
    DCHECK(!animation_);
  }

  return desk;
}

bool DesksController::OnSingleInstanceAppLaunchingFromSavedDesk(
    const std::string& app_id,
    const app_restore::RestoreData::LaunchList& launch_list) {
  // Iterate through the windows on each desk to see if there is an existing app
  // window instance.
  aura::Window* existing_app_instance_window = nullptr;
  Desk* src_desk = nullptr;
  for (auto& desk : desks()) {
    for (aura::Window* window : desk->GetAllAssociatedWindows()) {
      const std::string* const app_id_ptr = window->GetProperty(kAppIDKey);
      if (app_id_ptr && *app_id_ptr == app_id) {
        existing_app_instance_window = window;
        src_desk = desk.get();
        break;
      }
    }

    // We can break the first loop once we found an existing app window
    // instance.
    if (existing_app_instance_window)
      break;
  }

  if (!existing_app_instance_window)
    return true;

  // We have a window that we are going to move to the right desk and then apply
  // properties to. In order to do this, we need the restore data. If we are in
  // this function, then we are dealing with a single instance app and there
  // should be at most one entry in the launch list.
  DCHECK_LE(launch_list.size(), 1u);
  if (launch_list.empty() || !launch_list.begin()->second)
    return false;

  auto& app_restore_data = *launch_list.begin()->second;

  // No need to shift a window that is visible on all desks.
  // TODO(sammiequon): Remove this property if the window on the new desk should
  // not be visible on all desks.
  if (!desks_util::IsWindowVisibleOnAllWorkspaces(
          existing_app_instance_window)) {
    // The uuid of the target desk is found in `app_restore_data`. If it isn't
    // set, or is invalid, then we default to the rightmost desk.
    Desk* target_desk = GetDeskByUuid(app_restore_data.window_info.desk_guid);
    if (!target_desk) {
      target_desk = desks_.back().get();
    }

    DCHECK(src_desk);
    if (src_desk != target_desk) {
      base::AutoReset<bool> in_progress(&are_desks_being_modified_, true);
      src_desk->MoveWindowToDesk(existing_app_instance_window, target_desk,
                                 existing_app_instance_window->GetRootWindow(),
                                 /*unminimize=*/false);
      // If the floated window is the single instance window, we need to let
      // float controller handle move window to desk, as floated window
      // doesn't belong to desk container.
      if (WindowState::Get(existing_app_instance_window)->IsFloated()) {
        Shell::Get()->float_controller()->OnMovingFloatedWindowToDesk(
            existing_app_instance_window, src_desk, target_desk,
            existing_app_instance_window->GetRootWindow());
      }

      MaybeUpdateShelfItems(
          /*windows_on_inactive_desk=*/{},
          /*windows_on_active_desk=*/{existing_app_instance_window});
      ReportNumberOfWindowsPerDeskHistogram();
    }
  }

  // Now that the window is on the correct desk, we can apply window properties.
  const app_restore::WindowInfo& window_info = app_restore_data.window_info;
  if (window_info.current_bounds) {
    existing_app_instance_window->SetBounds(*window_info.current_bounds);
  }

  // Handle window state and window bounds.
  if (window_info.window_state_type) {
    chromeos::WindowStateType target_state = *window_info.window_state_type;

    // Not all window states are supported.
    const bool restoreable_state =
        chromeos::IsNormalWindowStateType(target_state) ||
        chromeos::IsSnappedWindowStateType(target_state) ||
        target_state == chromeos::WindowStateType::kMinimized ||
        target_state == chromeos::WindowStateType::kMaximized;

    if (restoreable_state) {
      WindowState* window_state =
          WindowState::Get(existing_app_instance_window);
      DCHECK(window_state);

      if (target_state != window_state->GetStateType()) {
        switch (target_state) {
          case chromeos::WindowStateType::kDefault:
          case chromeos::WindowStateType::kNormal: {
            const WMEvent event(WM_EVENT_NORMAL);
            window_state->OnWMEvent(&event);
            break;
          }
          case chromeos::WindowStateType::kMinimized:
            if (window_state->CanMinimize()) {
              window_state->Minimize();
              window_state->set_unminimize_to_restore_bounds(true);
            }
            break;
          case chromeos::WindowStateType::kMaximized:
            if (window_state->CanMaximize())
              window_state->Maximize();
            break;
          case chromeos::WindowStateType::kPrimarySnapped:
          case chromeos::WindowStateType::kSecondarySnapped:
            if (window_state->CanSnap()) {
              const WindowSnapWMEvent event(
                  target_state == chromeos::WindowStateType::kPrimarySnapped
                      ? WM_EVENT_SNAP_PRIMARY
                      : WM_EVENT_SNAP_SECONDARY,
                  WindowSnapActionSource::
                      kSnapByFullRestoreOrDeskTemplateOrSavedDesk);
              window_state->OnWMEvent(&event);
            }
            break;
          case chromeos::WindowStateType::kFloated: {
            const WindowFloatWMEvent event(
                chromeos::FloatStartLocation::kBottomRight);
            window_state->OnWMEvent(&event);
            break;
          }
          case chromeos::WindowStateType::kInactive:
          case chromeos::WindowStateType::kFullscreen:
          case chromeos::WindowStateType::kPinned:
          case chromeos::WindowStateType::kTrustedPinned:
          case chromeos::WindowStateType::kPip:
            NOTREACHED();
        }
      }

      // For states with restore bounds (maximized, snapped, minimized), the
      // restore bounds are stored in `current_bounds`.
      const gfx::Rect restore_bounds =
          window_info.current_bounds.value_or(gfx::Rect());
      if (!restore_bounds.IsEmpty())
        window_state->SetRestoreBoundsInScreen(restore_bounds);
    }
  }

  if (window_info.activation_index) {
    existing_app_instance_window->SetProperty(app_restore::kActivationIndexKey,
                                              *window_info.activation_index);
  }

  WindowRestoreController::Get()->StackWindow(existing_app_instance_window);

  // TODO(sammiequon): Read something for chromevox, either here or when the
  // whole template launches.
  return false;
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

void DesksController::MaybeCancelDeskRemoval() {
  if (!temporary_removed_desk_)
    return;

  // `UndoDeskRemoval()` will take ownership of `temporary_removed_desk` so we
  // need to get the toast id beforehand. It also needs to come before
  // cancelling the toast as if animations are disabled, cancelling will call
  // `MaybeCommitPendingDeskRemoval()` right away, which would delete
  // `temporary_removed_desk`.
  const std::string toast_id = temporary_removed_desk_->toast_id();
  UndoDeskRemoval();
  ToastManager::Get()->Cancel(toast_id);
}

void DesksController::MaybeDismissPersistentDeskRemovalToast() {
  if (temporary_removed_desk_ &&
      temporary_removed_desk_->is_toast_persistent()) {
    ToastManager::Get()->Cancel(temporary_removed_desk_->toast_id());
  }
}

bool DesksController::RequestFocusOnUndoDeskRemovalToast() {
  if (!Shell::Get()->accessibility_controller()->spoken_feedback().enabled() ||
      !temporary_removed_desk_ ||
      !ToastManager::Get()->IsToastShown(temporary_removed_desk_->toast_id())) {
    return false;
  }

  return ToastManager::Get()->RequestFocusOnActiveToastDismissButton(
      temporary_removed_desk_->toast_id());
}

bool DesksController::CanEnterOverview() const {
  // Prevent entering overview if a desk animation is underway and we didn't
  // start the animation in overview. The overview animation would be completely
  // covered anyway, and doing so could put us in a strange state (unless we are
  // doing an immediate enter).
  if (animation_ && !animation_->CanEnterOverview()) {
    // The one exception to this rule is in tablet mode, having a window snapped
    // to one side. Moving to this desk, we will want to open overview on the
    // other side. For clamshell we don't need to enter overview as having a
    // window snapped to one side and showing the wallpaper on the other is
    // fine.
    auto* split_view_controller =
        SplitViewController::Get(Shell::GetPrimaryRootWindow());
    if (!split_view_controller->InTabletSplitViewMode() ||
        split_view_controller->state() ==
            SplitViewController::State::kBothSnapped) {
      return false;
    }
  }
  return true;
}

bool DesksController::CanEndOverview() const {
  // During an ongoing desk animation, we take screenshots of the starting
  // active desk and the new active desk and animate between them. If overview
  // desk navigation is enabled, we keep the user in overview for both the
  // original and new active desks so it appears as if the user never left
  // overview, and this is reflected in the screenshots displayed. If an
  // overview exit is attempted during this ongoing animation (i.e. a user
  // presses the overview button), we want to ensure that the displayed
  // screenshot is still reflective of the user's actual ending state (which can
  // be jarring if the screenshot is different from the appearance of the new
  // desk), so we don't want to allow overview to be exited before the animation
  // ends.
  return !features::IsOverviewDeskNavigationEnabled() || !animation_ ||
         animation_->CanEndOverview();
}

void DesksController::OnWindowActivating(ActivationReason reason,
                                         aura::Window* gaining_active,
                                         aura::Window* losing_active) {
  if (AreDesksBeingModified())
    return;

  // When there is no `current_account_id_`, it means no user has finished sign
  // in (either no user, or a user is signing in). Do not change desks in this
  // case.
  if (!current_account_id_.is_valid()) {
    return;
  }

  // Browser session restore opens all restored windows, so it activates
  // every single window and activates the parent desk. Therefore, this check
  // prevents repetitive desk activation. Moreover, when Bento desks restore is
  // enabled, it avoid switching desk back and forth when windows are restored
  // to different desks.
  if (Shell::Get()->shell_delegate()->IsSessionRestoreInProgress())
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
  // TODO(b/284482035): Remove this when multi-profile support goes away.
  DCHECK(current_account_id_.is_valid());
  if (current_account_id_ == account_id) {
    return;
  }

  user_to_active_desk_index_[current_account_id_] = GetDeskIndex(active_desk_);
  current_account_id_ = account_id;

  // Note the following constraints for secondary users:
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
  new_user_active_desk_index = std::clamp(new_user_active_desk_index, 0,
                                          static_cast<int>(desks_.size()) - 1);

  ActivateDesk(desks_[new_user_active_desk_index].get(),
               DesksSwitchSource::kUserSwitch);
}

void DesksController::OnFirstSessionStarted() {
  current_account_id_ =
      Shell::Get()->session_controller()->GetActiveAccountId();
  desks_restore_util::RestorePrimaryUserDesks();

  // The DeskProfilesDelegate will be available if lacros and desk profiles are
  // both enabled.
  desk_profiles_observer_.Reset();
  if (auto* delegate = Shell::Get()->GetDeskProfilesDelegate()) {
    desk_profiles_observer_.Observe(delegate);
  }
}

void DesksController::OnProfileRemoved(uint64_t profile_id) {
  auto* delegate = Shell::Get()->GetDeskProfilesDelegate();
  CHECK(delegate);

  uint64_t primary_profile_id = delegate->GetPrimaryProfileId();
  for (auto& desk : desks_) {
    // If this desk's profile has been removed, revert it to the primary user's
    // profile (which cannot be deleted).
    if (desk->lacros_profile_id() == profile_id) {
      desk->SetLacrosProfileId(primary_profile_id, /*source=*/std::nullopt);
    }
  }
}

void DesksController::FireMetricsTimerForTesting() {
  metrics_helper_->FireTimerForTesting();
}

void DesksController::ResetAnimation() {
  animation_.reset();
}

std::u16string DesksController::CreateUniqueDeskName(
    const std::u16string& base) const {
  std::u16string desk_name;
  if (!base.empty()) {
    int count = 1;
    desk_name = base;
    while (HasDeskWithName(desk_name)) {
      desk_name =
          std::u16string(base).append(u" (" + base::FormatNumber(count) + u")");
      count++;
    }
  }
  return desk_name;
}

void DesksController::OnAnimationFinished(DeskAnimationBase* animation) {
  DCHECK_EQ(animation_.get(), animation);
  metrics_helper_->OnAnimationFinished(animation->visible_desk_changes());
  animation_.reset();

  // If we just switched desks due to removing the active desk, we immediately
  // focus the undo button.
  if (Shell::Get()->accessibility_controller()->spoken_feedback().enabled()) {
    RequestFocusOnUndoDeskRemovalToast();
  }
}

bool DesksController::HasDeskWithName(const std::u16string& desk_name) const {
  return base::Contains(desks_, desk_name, &Desk::name);
}

void DesksController::ActivateDeskInternal(const Desk* desk,
                                           bool update_window_activation) {
  DCHECK(HasDesk(desk));

  if (desk == active_desk_)
    return;

  base::AutoReset<bool> in_progress(&are_desks_being_modified_, true);
  base::AutoReset<raw_ptr<Desk>> activate_desk(&desk_to_activate_,
                                               const_cast<Desk*>(desk));

  // Mark the new desk as active first, so that deactivating windows on the
  // `old_active` desk do not activate other windows on the same desk. See
  // `ash::AshFocusRules::GetNextActivatableWindow()`.
  Desk* old_active = active_desk_;
  old_active->BuildAllDeskStackingData();

  auto* shell = Shell::Get();
  auto* overview_controller = shell->overview_controller();
  const bool was_in_overview = overview_controller->InOverviewSession();
  // The order here matters. Overview must end before ending tablet split view
  // before switching desks. (If clamshell split view is active on one or more
  // displays, then it simply will end when we end overview.) That's because
  // we don't want `TabletModeWindowManager` maximizing all windows because we
  // cleared the snapped ones in `SplitViewController` first. See
  // `TabletModeWindowManager::OnOverviewModeEndingAnimationComplete`.
  // See also test coverage for this case in
  // `TabletModeDesksTest.SnappedStateRetainedOnSwitchingDesksFromOverview`.
  if (animation_ && was_in_overview) {
    // Exit overview mode immediately without any animations before taking the
    // ending desk screenshot. This makes sure that the ending desk screenshot
    // will only show the windows in that desk, not overview stuff.
    overview_controller->EndOverview(OverviewEndAction::kDeskActivation,
                                     OverviewEnterExitType::kImmediateExit);
  }

  // We should always end split view during a desk change in order to update the
  // divider widget.
  for (aura::Window* root_window : Shell::GetAllRootWindows()) {
    SplitViewController::Get(root_window)
        ->EndSplitView(SplitViewController::EndReason::kDesksChange);
  }

  MoveVisibleOnAllDesksWindowsFromActiveDeskTo(const_cast<Desk*>(desk));
  active_desk_ = const_cast<Desk*>(desk);
  RestackVisibleOnAllDesksWindowsOnActiveDesk();

  // There should always be an active desk at any time.
  DCHECK(old_active);
  old_active->Deactivate(update_window_activation);
  active_desk_->Activate(update_window_activation);
  if (features::IsOverviewDeskNavigationEnabled() && animation_ &&
      was_in_overview) {
    overview_controller->StartOverview(OverviewStartAction::kOverviewDeskSwitch,
                                       OverviewEnterExitType::kImmediateEnter);
  }

  // Content is normally updated in
  // `MoveVisibleOnAllDesksWindowsFromActiveDeskTo`. However, the layers for a
  // visible on all desk window aren't mirrored for the active desk. When
  // `MoveVisibleOnAllDesksWindowFromActiveDeskTo` is called, `desk` is not
  // considered the active desk, so we force an update here to apply the changes
  // for a visible on all desk window.
  if (!visible_on_all_desks_windows_.empty())
    old_active->NotifyContentChanged();

  MaybeUpdateShelfItems(old_active->windows(), active_desk_->windows());

  // If in the middle of a window cycle gesture, reset the window cycle list
  // contents so it contains the new active desk's windows.
  if (auto* window_cycle_controller = shell->window_cycle_controller();
      window_cycle_controller->IsAltTabPerActiveDesk()) {
    window_cycle_controller->MaybeResetCycleList();
  }

  for (auto& observer : observers_)
    observer.OnDeskActivationChanged(active_desk_, old_active);

  NotifyFullScreenStateChangedAcrossDesksIfNeeded(old_active);

  const int active_desk_index = GetActiveDeskIndex();
  user_to_active_desk_index_[current_account_id_] = active_desk_index;

  // Only update active desk prefs when a primary user switches a desk.
  if (shell->session_controller()->IsUserPrimary()) {
    desks_restore_util::UpdatePrimaryUserActiveDeskPrefs(active_desk_index);
  }
}

void DesksController::RemoveDeskInternal(const Desk* desk,
                                         DesksCreationRemovalSource source,
                                         DeskCloseType close_type,
                                         bool desk_switched) {
  // Removing a desk can cause transient raster scale updates during overview
  // mode, if desks are combined. Pause raster scale updates until windows are
  // in their final state.
  ScopedPauseRasterScaleUpdates scoped_pause;

  MaybeCommitPendingDeskRemoval();

  DCHECK(CanRemoveDesks());

  base::AutoReset<bool> in_progress(&are_desks_being_modified_, true);

  auto iter = base::ranges::find(desks_, desk, &std::unique_ptr<Desk>::get);
  DCHECK(iter != desks_.end());

  const int removed_desk_index = std::distance(desks_.begin(), iter);

  // Update workspaces of windows in desks that have higher indices than the
  // removed desk since indices of those desks shift by one.
  for (int i = removed_desk_index + 1; i < static_cast<int>(desks_.size());
       i++) {
    for (aura::Window* window : desks_[i]->windows()) {
      if (desks_util::IsWindowVisibleOnAllWorkspaces(window))
        continue;
      window->SetProperty(aura::client::kWindowWorkspaceKey, i - 1);
    }
  }

  // If the removed desk is before the active desk (or the active desk) of any
  // logged in user, it needs to be adjusted.
  for (auto& [account_id, desk_index] : user_to_active_desk_index_) {
    if (removed_desk_index <= desk_index && desk_index > 0) {
      --desk_index;
    }
  }
  // Update the prefs for the primary user, since it may have been affected by
  // the desk removal.
  desks_restore_util::UpdatePrimaryUserActiveDeskPrefs(
      user_to_active_desk_index_[GetPrimaryUserAccountId()]);

  // Keep the removed desk's data alive until at least the end of this function.
  // `MaybeCommitPendingDeskRemoval` at this point should have cleared
  // `temporary_removed_desk_`. Otherwise, we may be resetting the wrong
  // removing desk GUID in restore data.
  CHECK(!temporary_removed_desk_);
  const bool desk_was_active = (*iter)->is_active() || desk_switched;
  auto temporary_removed_desk =
      std::make_unique<RemovedDeskData>(std::move(*iter), desk_was_active,
                                        removed_desk_index, source, close_type);
  auto* temporary_removed_desk_ptr = temporary_removed_desk.get();
  Desk* removed_desk = temporary_removed_desk_ptr->desk();

  // If we're going to wait to destroy the desk, we move the unique pointer for
  // the desk data into the `temporary_removed_desk_` member variable.
  if (close_type == DeskCloseType::kCloseAllWindowsAndWait)
    temporary_removed_desk_ = std::move(temporary_removed_desk);

  DCHECK_EQ(removed_desk, desk);
  auto iter_after = desks_.erase(iter);

  DCHECK(!desks_.empty());

  auto* shell = Shell::Get();
  auto* overview_controller = shell->overview_controller();
  const bool in_overview = overview_controller->InOverviewSession();

  // Windows that are associated with the desk that is being removed. If we are
  // combining desks, then they are going to be moved into the active desk.
  // They are placed in a window tracker, in case an operation below closes any
  // of them.
  aura::WindowTracker windows_from_combined_desk;
  if (close_type == DeskCloseType::kCombineDesks) {
    for (aura::Window* window : removed_desk->GetAllAssociatedWindows()) {
      windows_from_combined_desk.Add(window);
    }
  }

  // No need to spend time refreshing the mini_views of the removed desk.
  auto removed_desk_mini_views_pauser =
      Desk::ScopedContentUpdateNotificationDisabler(
          /*desks=*/{removed_desk},
          /*notify_when_destroyed=*/false);

  // - If the active desk is the one being removed, activate the desk to its
  //   left, if no desk to the left, activate one on the right.
  // - If we are not closing the windows, move the windows in `removed_desk`
  //   (if any) to the currently active desk.
  // - We don't need to do anything here for the case that we are closing the
  //   windows and the closing desk is not active because we don't have to move
  //   anything.
  const bool will_switch_desks = temporary_removed_desk_ptr->was_active();
  if (will_switch_desks) {
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
        Desk::ScopedContentUpdateNotificationDisabler(
            /*desks=*/{target_desk},
            /*notify_when_destroyed=*/false);

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
    // to be moved to the target desk if we are combining desks.
    if (in_overview)
      RemoveAllWindowsFromOverview();

    // If overview mode is active, change desk activation without changing
    // window activation. Activation should remain on the dummy
    // "OverviewModeFocusedWidget" while overview mode is active.
    if (close_type == DeskCloseType::kCombineDesks) {
      // If we are not closing the app windows, then we move all windows to
      // `target_desk`.
      removed_desk->MoveWindowsToDesk(target_desk);
    } else if (in_overview) {
      // If we are closing the app windows, then we only need to move non-app
      // overview windows to `target_desk` if we are in overview.
      DCHECK(close_type == DeskCloseType::kCloseAllWindows ||
             close_type == DeskCloseType::kCloseAllWindowsAndWait);
      removed_desk->MoveNonAppOverviewWindowsToDesk(target_desk);
    }

    // If the desk has already switched due to the `DeskRemovalAnimation` being
    // run in `RemoveDesk`, we should not try to activate the desk again.
    if (!desk_switched) {
      ActivateDesk(
          target_desk,
          source == DesksCreationRemovalSource::kDeskButtonDeskBarButton
              ? DesksSwitchSource::kDeskButtonDeskRemoved
              : DesksSwitchSource::kDeskRemoved);
    }

    // Desk activation should not change overview mode state.
    DCHECK_EQ(in_overview, overview_controller->InOverviewSession());

    // Now that |target_desk| is activated, we can restack the visible on all
    // desks windows that were moved from the old active desk.
    RestackVisibleOnAllDesksWindowsOnActiveDesk();

    // Now that the windows from the removed and target desks merged, add them
    // all to the grid in the order of the new MRU.
    if (in_overview)
      AppendWindowsToOverview(target_desk->GetAllAssociatedWindows());

  } else if (close_type == DeskCloseType::kCombineDesks) {
    // We will refresh the mini_views of the active desk only once at the end.
    auto active_desk_mini_view_pauser =
        Desk::ScopedContentUpdateNotificationDisabler(
            /*desks=*/{active_desk_},
            /*notify_when_destroyed=*/false);

    removed_desk->MoveWindowsToDesk(active_desk_);

    // If overview mode is active, we add the windows of the removed desk to the
    // overview grid in the order of the new MRU (which changes after removing a
    // desk by making the windows of the removed desk as the least recently used
    // across all desks). Note that this can only be done after the windows have
    // moved to the active desk in `MoveWindowsToDesk()` above, so that building
    // the window MRU list should contain those windows.
    if (in_overview) {
      AppendWindowsToOverview(windows_from_combined_desk.windows());
    }
  }

  // It's OK now to refresh the mini_views of *only* the active desk, and only
  // if windows from the removed desk moved to it.
  DCHECK(!active_desk_->ContentUpdateNotificationSuspended());
  if (!windows_from_combined_desk.windows().empty()) {
    active_desk_->NotifyContentChanged();
  }

  UpdateDesksDefaultNames();

  for (auto& observer : observers_)
    observer.OnDeskRemoved(removed_desk);

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

  // The current active desk may have gained some windows.
  MaybeUpdateShelfItems({}, windows_from_combined_desk.windows());

  UMA_HISTOGRAM_ENUMERATION(kRemoveDeskHistogramName, source);
  UMA_HISTOGRAM_ENUMERATION(kRemoveDeskTypeHistogramName, close_type);
  if (source == DesksCreationRemovalSource::kApi) {
    base::UmaHistogramEnumeration(kDeskApiRemoveDeskTypeHistogramName,
                                  close_type);
  }

  // We should only announce desks are being merged if we are combining desks.
  // Otherwise, we tell the user that the desk has closed with its windows.
  AccessibilityController* accessibility_controller =
      shell->accessibility_controller();
  if (close_type == DeskCloseType::kCombineDesks) {
    accessibility_controller->TriggerAccessibilityAlertWithMessage(
        l10n_util::GetStringFUTF8(IDS_ASH_VIRTUAL_DESKS_ALERT_DESK_REMOVED,
                                  removed_desk->name(), active_desk_->name()));
  } else if (close_type == DeskCloseType::kCloseAllWindowsAndWait) {
    accessibility_controller->TriggerAccessibilityAlertWithMessage(
        l10n_util::GetStringUTF8(
            IDS_ASH_VIRTUAL_DESKS_ALERT_DESK_CLOSED_WITH_WINDOWS));
  }

  desks_restore_util::UpdatePrimaryUserDeskNamesPrefs();
  desks_restore_util::UpdatePrimaryUserDeskGuidsPrefs();
  desks_restore_util::UpdatePrimaryUserDeskLacrosProfileIdPrefs();
  desks_restore_util::UpdatePrimaryUserDeskMetricsPrefs();

  DCHECK_LE(available_container_ids_.size(), desks_util::GetMaxNumberOfDesks());

  if (close_type == DeskCloseType::kCloseAllWindowsAndWait) {
    ShowDeskRemovalUndoToast(
        temporary_removed_desk_->toast_id(),
        /*dismiss_callback=*/
        base::BindRepeating(&DesksController::UndoDeskRemoval,
                            base::Unretained(this)),
        /*expired_callback=*/
        base::BindOnce(&DesksController::MaybeCommitPendingDeskRemoval,
                       base::Unretained(this),
                       temporary_removed_desk_->toast_id()),
        temporary_removed_desk_->is_toast_persistent());

    // This method will be invoked on both undo and expired toast.
    base::UmaHistogramBoolean(kCloseAllTotalHistogramName, true);
  }
}

void DesksController::UndoDeskRemoval() {
  DCHECK(temporary_removed_desk_);
  base::UmaHistogramBoolean(kCloseAllUndoHistogramName, true);
  if (temporary_removed_desk_->desk_removal_source() ==
      DesksCreationRemovalSource::kApi) {
    base::UmaHistogramBoolean(kDeskApiCloseAllUndoHistogramName, true);
  }
  Desk* readded_desk_ptr = temporary_removed_desk_->desk();
  auto readded_desk_data = std::move(temporary_removed_desk_);
  const int readded_desk_index = readded_desk_data->index();
  desks_.insert(desks_.begin() + readded_desk_index,
                readded_desk_data->AcquireDesk());
  readded_desk_ptr->set_is_desk_being_removed(false);

  // If the re-added desk is before the active desk of any logged in user, it
  // needs to be adjusted.
  for (auto& [account_id, desk_index] : user_to_active_desk_index_) {
    if (readded_desk_index <= desk_index) {
      ++desk_index;
    }
  }
  desks_restore_util::UpdatePrimaryUserActiveDeskPrefs(
      user_to_active_desk_index_[GetPrimaryUserAccountId()]);

  for (auto& observer : observers_) {
    observer.OnDeskAdded(readded_desk_ptr, /*from_undo=*/true);
  }

  // If the desk was active, we reactivate it.
  if (readded_desk_data->was_active()) {
    auto* overview_controller = Shell::Get()->overview_controller();
    const bool in_overview = overview_controller->InOverviewSession();

    if (in_overview) {
      RemoveAllWindowsFromOverview();
      active_desk_->MoveNonAppOverviewWindowsToDesk(readded_desk_ptr);
    }

    ActivateDesk(readded_desk_ptr, DesksSwitchSource::kRemovalUndone);
    RestackVisibleOnAllDesksWindowsOnActiveDesk();

    if (in_overview)
      AppendWindowsToOverview(readded_desk_ptr->GetAllAssociatedWindows());
  }

  Shell::Get()
      ->accessibility_controller()
      ->TriggerAccessibilityAlertWithMessage(l10n_util::GetStringUTF8(
          IDS_ASH_DESKS_CLOSE_ALL_UNDONE_NOTIFICATION));

  UpdateDesksDefaultNames();
}

void DesksController::FinalizeDeskRemoval(RemovedDeskData* removed_desk_data) {
  Desk* removed_desk = removed_desk_data->desk();
  available_container_ids_.push(removed_desk->container_id());

  // Record histograms for the desk before closing.
  auto* non_const_desk = const_cast<Desk*>(removed_desk);
  non_const_desk->RecordLifetimeHistogram(removed_desk_data->index());
  non_const_desk->RecordAndResetConsecutiveDailyVisits(
      /*being_removed=*/true);

  // Record number of windows being closed by close-all desk removal.
  // Only record metric for close-all, not for combine desk action.
  if (removed_desk_data->desk_close_type() == DeskCloseType::kCloseAllWindows ||
      removed_desk_data->desk_close_type() ==
          DeskCloseType::kCloseAllWindowsAndWait) {
    base::UmaHistogramCounts100(kNumberOfWindowsClosed,
                                removed_desk->windows().size());
    ReportClosedWindowsCountPerSourceHistogram(
        removed_desk_data->desk_removal_source(),
        removed_desk->windows().size());
  }

  for (auto& observer : observers_) {
    observer.OnDeskRemovalFinalized(removed_desk->uuid());
  }

  ReportDesksCountHistogram();
  ReportNumberOfWindowsPerDeskHistogram();

  // Record the number and percentage of desks with custom names.
  ReportCustomDeskNames();

  // We need to ensure there are no app windows in the desk before destruction.
  // During a combine desks operation, the windows would have already been
  // moved, so in that case this would be a no-op.

  // Content changed notifications for this desk should be disabled when
  // we are destroying the windows.
  auto throttle_desk_notifications =
      Desk::ScopedContentUpdateNotificationDisabler(
          /*desks=*/{removed_desk}, /*notify_when_destroyed=*/false);

  std::vector<raw_ptr<aura::Window, VectorExperimental>> app_windows =
      removed_desk->GetAllAppWindows();

  // We use `closing_window_tracker` to track all app windows that should be
  // closed from the removed desk, `WindowTracker` will handle windows that may
  // have already been indirectly closed due to the closure of other windows, as
  // the `WindowTracker` automatically removes windows when they are closed.
  std::unique_ptr<aura::WindowTracker> closing_window_tracker =
      std::make_unique<aura::WindowTracker>(app_windows);

  // We create a new tracker other than `closing_window_tracker`, since we want
  // to leave it unaffected as we will pass it later to a delayed callback to
  // force close the windows that haven't been closed yet. The new tracker
  // `unclosed_windows_tracker` will be empty by the end of the below
  // while-loop.
  aura::WindowTracker unclosed_windows_tracker(app_windows);

  aura::Window* floated_window =
      Shell::Get()->float_controller()->FindFloatedWindowOfDesk(removed_desk);
  while (!unclosed_windows_tracker.windows().empty()) {
    aura::Window* window = unclosed_windows_tracker.Pop();
    views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
    DCHECK(widget);

    // `widget->Close();` calls the underlying `native_widget_->Close()` which
    // will schedule `native_widget_->CloseNow()` as an async task. Only
    // when `native_widget_->CloseNow()` finishes running, the window will
    // finally be removed from desk. Therefore, to remove the desk now, we have
    // to manually remove the window from desk now.
    // We also want to ensure that any windows associated with `removed_desk`'s
    // container are removed from the container in case we want to immediately
    // reuse that container. Since floated window doesn't belong to desk
    // container, handle it separately.

    // When windows are being closed, they do so asynchronously. So, to free up
    // the desk container while the windows are being closed, we want to move
    // those windows to the container `kShellWindowId_UnparentedContainer`.
    // If we move one of the windows in a snap group, the `SnapGroup` will take
    // care of moving the other to be under the same parent, so we don't need to
    // move it here again.
    auto* unparented_container = window->GetRootWindow()->GetChildById(
        kShellWindowId_UnparentedContainer);
    if (window != floated_window && unparented_container != window->parent()) {
      unparented_container->AddChild(window);
    }

    // We need to ensure that `widget->Close()` is called after we move the
    // windows to the unparented container because some windows lose access to
    // their root window immediately when their widget starts closing.
    widget->Close();
  }

  // Schedules a delayed task to forcefully close all windows that have not
  // finish closing.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&DesksController::CleanUpClosedAppWindowsTask,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(closing_window_tracker)),
      g_close_all_window_close_timeout);

  // `temporary_removed_desk_` should not be set at this point.
  DCHECK(!temporary_removed_desk_);
}

void DesksController::MaybeCommitPendingDeskRemoval(
    const std::string& toast_id) {
  if (temporary_removed_desk_ && temporary_removed_desk_->desk()) {
    // We need to tell any browser windows that are still in
    // `temporary_removed_desk_->desk()` to suppress warning the user before
    // closing.
    Shell::Get()->shell_delegate()->ForceSkipWarningUserOnClose(
        temporary_removed_desk_->desk()->GetAllAppWindows());
  }

  if (toast_id.empty() || (temporary_removed_desk_ &&
                           temporary_removed_desk_->toast_id() == toast_id)) {
    temporary_removed_desk_.reset();
  }
}

bool DesksController::IsUndoToastFocused() const {
  return temporary_removed_desk_ &&
         ToastManager::Get()->IsToastDismissButtonFocused(
             temporary_removed_desk_->toast_id());
}

void DesksController::TrackWindowOnAllDesks(aura::Window* window) {
  for (auto& desk : desks_) {
    desk->TrackAllDeskWindow(window);
  }
}

void DesksController::UntrackWindowFromAllDesks(aura::Window* window) {
  for (auto& desk : desks_) {
    desk->UntrackAllDeskWindow(window,
                               /*recent_root=*/window->GetRootWindow());
  }
}

void DesksController::CleanUpClosedAppWindowsTask(
    std::unique_ptr<aura::WindowTracker> closing_window_tracker) {
  // We have waited long enough for these app windows to close cleanly.
  // If there is any app windows still around, we will close them forcefully.
  // These window's desk has already been removed. We should not let these
  // windows linger around.
  while (!closing_window_tracker->windows().empty()) {
    aura::Window* window = closing_window_tracker->Pop();
    views::Widget* widget = views::Widget::GetWidgetForNativeView(window);

    // Forcefully close this app window. `CloseNow` which directly deleted the
    // associated native widget. This will skip many Window shutdown hook
    // logic. However, the desk controller has waited for the app window to
    // close cleanly before this.
    if (widget) {
      widget->CloseNow();
    }
  }
}

void DesksController::MoveVisibleOnAllDesksWindowsFromActiveDeskTo(
    Desk* new_desk) {
  // Ignore activations in the MRU tracker until we finish moving all visible on
  // all desks windows so we maintain global MRU order that is used later
  // for stacking visible on all desks windows.
  auto* mru_tracker = Shell::Get()->mru_window_tracker();
  mru_tracker->SetIgnoreActivations(true);

  for (aura::Window* visible_on_all_desks_window :
       visible_on_all_desks_windows_) {
    MoveWindowFromActiveDeskTo(
        visible_on_all_desks_window, new_desk,
        visible_on_all_desks_window->GetRootWindow(),
        DesksMoveWindowFromActiveDeskSource::kVisibleOnAllDesks);
  }

  mru_tracker->SetIgnoreActivations(false);
}

void DesksController::NotifyFullScreenStateChangedAcrossDesksIfNeeded(
    const Desk* previous_active_desk) {
  Shell* shell = Shell::Get();
  for (aura::Window* root : Shell::GetAllRootWindows()) {
    aura::Window* active_desk_container =
        active_desk_->GetDeskContainerForRoot(root);
    const bool is_active_desk_fullscreen =
        GetWorkspaceController(active_desk_container)
            ->layout_manager()
            ->is_fullscreen();
    if (GetWorkspaceController(
            previous_active_desk->GetDeskContainerForRoot(root))
            ->layout_manager()
            ->is_fullscreen() != is_active_desk_fullscreen) {
      shell->NotifyFullscreenStateChanged(is_active_desk_fullscreen,
                                          active_desk_container);
    }
  }
}

void DesksController::RestackVisibleOnAllDesksWindowsOnActiveDesk() {
  active_desk_->RestackAllDeskWindows();
}

const Desk* DesksController::FindDeskOfWindow(aura::Window* window) const {
  DCHECK(window);

  // Floating windows are stored in float container, their relationship with
  // desks can be found in `FloatController::FloatedWindowInfo`.
  if (WindowState::Get(window)->IsFloated()) {
    return Shell::Get()->float_controller()->FindDeskOfFloatedWindow(window);
  }

  for (const auto& desk : desks_) {
    if (base::Contains(desk->windows(), window))
      return desk.get();
  }

  return nullptr;
}

void DesksController::ReportNumberOfWindowsPerDeskHistogram() const {
  DCHECK_LE(desks_.size(), desks_util::kDesksUpperLimit);
  for (size_t i = 0; i < desks_.size(); ++i) {
    const size_t windows_count = desks_[i]->windows().size();
    base::UmaHistogramCounts100(
        kNumberOfWindowsOnDeskHistogramPrefix + base::NumberToString(i + 1),
        windows_count);
  }
}

void DesksController::ReportDesksCountHistogram() const {
  DCHECK_LE(desks_.size(), desks_util::kDesksUpperLimit);
  UMA_HISTOGRAM_EXACT_LINEAR(kDesksCountHistogramName, desks_.size(),
                             desks_util::kDesksUpperLimit + 1);
}

void DesksController::RecordAndResetNumberOfWeeklyActiveDesks() {
  base::UmaHistogramCounts1000(kWeeklyActiveDesksHistogramName,
                               Desk::GetWeeklyActiveDesks());

  for (const auto& desk : desks_)
    desk->set_interacted_with_this_week(desk.get() == active_desk_);
  Desk::SetWeeklyActiveDesks(1);

  weekly_active_desks_scheduler_.Start(
      FROM_HERE, base::Days(7), this,
      &DesksController::RecordAndResetNumberOfWeeklyActiveDesks);
}

void DesksController::ReportClosedWindowsCountPerSourceHistogram(
    DesksCreationRemovalSource source,
    int windows_closed) const {
  const char* desk_removal_source_histogram = nullptr;

  // TODO(b/285029311): We may want to create a new histogram for
  // NumberOfWindowsClosed with `kDeskButtonDeskBarButton` separated out.
  switch (source) {
    case DesksCreationRemovalSource::kButton:
    case DesksCreationRemovalSource::kDeskButtonDeskBarButton:
      desk_removal_source_histogram = kNumberOfWindowsClosedByButton;
      break;
    case DesksCreationRemovalSource::kKeyboard:
      desk_removal_source_histogram = kNumberOfWindowsClosedByKeyboard;
      break;
    case DesksCreationRemovalSource::kApi:
      desk_removal_source_histogram = kNumberOfWindowsClosedByApi;
      break;
    // Skip recording for save&recall as windows are already closed before
    // reaching here.
    case DesksCreationRemovalSource::kSaveAndRecall:
    // Skip recording for source that can't close a desk.
    case DesksCreationRemovalSource::kDragToNewDeskButton:
    case DesksCreationRemovalSource::kLaunchTemplate:
    case DesksCreationRemovalSource::kDesksRestore:
    case DesksCreationRemovalSource::kFloatingWorkspace:
    case DesksCreationRemovalSource::kEnsureDefaultDesk:
    case DesksCreationRemovalSource::kCoral:
      break;
  }
  if (desk_removal_source_histogram)
    base::UmaHistogramCounts100(desk_removal_source_histogram, windows_closed);
}

void DesksController::ReportCustomDeskNames() const {
  int custom_names_count =
      base::ranges::count(desks_, true, &Desk::is_name_set_by_user);

  base::UmaHistogramCounts100(kNumberOfCustomNamesHistogramName,
                              custom_names_count);
  base::UmaHistogramPercentage(kPercentageOfCustomNamesHistogramName,
                               custom_names_count * 100 / desks_.size());
}

// static
base::TimeDelta DesksController::GetCloseAllWindowCloseTimeoutForTest() {
  return g_close_all_window_close_timeout;
}

// static
base::AutoReset<base::TimeDelta>
DesksController::SetCloseAllWindowCloseTimeoutForTest(
    base::TimeDelta interval) {
  return {&g_close_all_window_close_timeout, interval};
}

}  // namespace ash
