// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_controller.h"

#include <utility>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/fps_counter.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_animations.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/root_window_desk_switch_animator.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "base/stl_util.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/compositor.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

constexpr char kNewDeskHistogramName[] = "Ash.Desks.NewDesk";
constexpr char kDesksCountHistogramName[] = "Ash.Desks.DesksCount";
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
constexpr char kDeskActivationSmoothnessHistogramName[] =
    "Ash.Desks.AnimationSmoothness.DeskActivation";
constexpr char kDeskRemovalSmoothnessHistogramName[] =
    "Ash.Desks.AnimationSmoothness.DeskRemoval";

// Appends the given |windows| to the end of the currently active overview mode
// session such that the most-recently used window is added first. If
// |should_animate| is true, the windows will animate to their positions in the
// overview grid.
void AppendWindowsToOverview(const std::vector<aura::Window*>& windows,
                             bool should_animate) {
  DCHECK(Shell::Get()->overview_controller()->InOverviewSession());

  auto* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  for (auto* window :
       Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk)) {
    if (!base::Contains(windows, window) ||
        window_util::ShouldExcludeForOverview(window)) {
      continue;
    }

    overview_session->AppendItem(window, /*reposition=*/true, should_animate);
  }
}

// Removes the given |windows| from the currently active overview mode session.
void RemoveWindowsFromOverview(const base::flat_set<aura::Window*>& windows) {
  DCHECK(Shell::Get()->overview_controller()->InOverviewSession());

  auto* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  for (auto* window : windows) {
    auto* item = overview_session->GetOverviewItemForWindow(window);
    if (item)
      overview_session->RemoveItem(item);
  }
}

// Selects and returns the compositor that the FpsCounter will use to measure
// the animation smoothness.
ui::Compositor* GetSelectedCompositorForAnimationSmoothness() {
  // Favor the compositor associated with the active window's root window (if
  // any), or that of the primary root window.
  auto* active_window = window_util::GetActiveWindow();
  auto* selected_root = active_window && active_window->GetRootWindow()
                            ? active_window->GetRootWindow()
                            : Shell::GetPrimaryRootWindow();
  DCHECK(selected_root);
  return selected_root->layer()->GetCompositor();
}

}  // namespace

// -----------------------------------------------------------------------------
// DesksController::AbstractDeskSwitchAnimation:

// An abstract class that handles the shared operations need to be performed
// when doing an animation that causes a desk switch animation. Subclasses
// such as DeskActivationAnimation and DeskRemovalAnimation implement the
// abstract interface of this class to handle the unique operations specific to
// each animation type.
class DesksController::DeskAnimationBase
    : public RootWindowDeskSwitchAnimator::Delegate {
 public:
  ~DeskAnimationBase() override = default;

  // Launches the animation. This should be done once all animators
  // are created and added to `desk_switch_animators_`. This is to avoid any
  // potential race conditions that might happen if one animator finished phase
  // (1) of the animation while other animators are still being constructed.
  void Launch() {
    for (auto& observer : controller_->observers_)
      observer.OnDeskSwitchAnimationLaunching();

    fps_counter_ = std::make_unique<FpsCounter>(
        GetSelectedCompositorForAnimationSmoothness());

    DCHECK(!desk_switch_animators_.empty());
    for (auto& animator : desk_switch_animators_)
      animator->TakeStartingDeskScreenshot();
  }

  // RootWindowDeskSwitchAnimator::Delegate:
  void OnStartingDeskScreenshotTaken(const Desk* ending_desk) override {
    DCHECK(!desk_switch_animators_.empty());

    // Once all starting desk screenshots on all roots are taken and placed on
    // the screens, do the actual desk activation logic.
    for (const auto& animator : desk_switch_animators_) {
      if (!animator->starting_desk_screenshot_taken())
        return;
    }

    // Extend the compositors' timeouts in order to prevents any repaints until
    // the desks are switched and overview mode exits.
    const auto roots = Shell::GetAllRootWindows();
    for (auto* root : roots)
      root->GetHost()->compositor()->SetAllowLocksToExtendTimeout(true);

    OnStartingDeskScreenshotTakenInternal(ending_desk);

    for (auto* root : roots)
      root->GetHost()->compositor()->SetAllowLocksToExtendTimeout(false);

    // Continue the second phase of the animation by taking the ending desk
    // screenshot and actually animating the layers.
    for (auto& animator : desk_switch_animators_)
      animator->TakeEndingDeskScreenshot();
  }

  void OnEndingDeskScreenshotTaken() override {
    DCHECK(!desk_switch_animators_.empty());

    // Once all ending desk screenshots on all roots are taken, start the
    // animation on all roots at the same time, so that they look synchrnoized.
    for (const auto& animator : desk_switch_animators_) {
      if (!animator->ending_desk_screenshot_taken())
        return;
    }

    for (auto& animator : desk_switch_animators_)
      animator->StartAnimation();
  }

  void OnDeskSwitchAnimationFinished() override {
    DCHECK(!desk_switch_animators_.empty());

    // Once all desk switch animations on all roots finish, destroy all the
    // animators.
    for (const auto& animator : desk_switch_animators_) {
      if (!animator->animation_finished())
        return;
    }

    OnDeskSwitchAnimationFinishedInternal();

    desk_switch_animators_.clear();

    ComputeAnimationSmoothnessAndReport();

    for (auto& observer : controller_->observers_)
      observer.OnDeskSwitchAnimationFinished();

    controller_->OnAnimationFinished(this);
    // `this` is now deleted.
  }

 protected:
  explicit DeskAnimationBase(DesksController* controller)
      : controller_(controller) {}

  // Abstract functions that can be overridden by child classes to do different
  // things when phase (1), and phase (3) completes. Note that
  // `OnDeskSwitchAnimationFinishedInternal()` will be called before the desks
  // screenshot layers, stored in `desk_switch_animators_`, are destroyed.
  virtual void OnStartingDeskScreenshotTakenInternal(
      const Desk* ending_desk) = 0;
  virtual void OnDeskSwitchAnimationFinishedInternal() = 0;

  // Since performance here matters, we have to use the UMA histograms macros to
  // report the smoothness histograms, but each macro use has to be associated
  // with exactly one histogram name. This function allows subclasses to report
  // the histogram using the macro with their desired name.
  virtual void ReportSmoothness(int smoothness) const = 0;

  DesksController* const controller_;

  // An animator object per each root. Once all the animations are complete,
  // this list is cleared.
  std::vector<std::unique_ptr<RootWindowDeskSwitchAnimator>>
      desk_switch_animators_;

 private:
  // Computes the animation smoothness and reports an UMA stat for it.
  void ComputeAnimationSmoothnessAndReport() {
    DCHECK(fps_counter_);
    const int smoothness = fps_counter_->ComputeSmoothness();
    if (smoothness < 0)
      return;
    ReportSmoothness(smoothness);
  }

  // The FPS counter used for measuring this animation smoothness.
  std::unique_ptr<FpsCounter> fps_counter_;

  DISALLOW_COPY_AND_ASSIGN(DeskAnimationBase);
};

// -----------------------------------------------------------------------------
// DesksController::DeskActivationAnimation:

class DesksController::DeskActivationAnimation
    : public DesksController::DeskAnimationBase {
 public:
  DeskActivationAnimation(DesksController* controller,
                          const Desk* ending_desk,
                          bool move_left)
      : DeskAnimationBase(controller) {
    for (auto* root : Shell::GetAllRootWindows()) {
      desk_switch_animators_.emplace_back(
          std::make_unique<RootWindowDeskSwitchAnimator>(root, ending_desk,
                                                         this, move_left,
                                                         /*for_remove=*/false));
    }
  }

  ~DeskActivationAnimation() override = default;

  // DesksController::AbstractDeskSwitchAnimation:
  void OnStartingDeskScreenshotTakenInternal(const Desk* ending_desk) override {
    // The order here matters. Overview must end before ending tablet split view
    // before switching desks. (If clamshell split view is active on one or more
    // displays, then it simply will end when we end overview.) That's because
    // we don't want |TabletModeWindowManager| maximizing all windows because we
    // cleared the snapped ones in |SplitViewController| first. See
    // |TabletModeWindowManager::OnOverviewModeEndingAnimationComplete|.
    // See also test coverage for this case in
    // `TabletModeDesksTest.SnappedStateRetainedOnSwitchingDesksFromOverview`.
    const bool in_overview =
        Shell::Get()->overview_controller()->InOverviewSession();
    if (in_overview) {
      // Exit overview mode immediately without any animations before taking the
      // ending desk screenshot. This makes sure that the ending desk
      // screenshot will only show the windows in that desk, not overview stuff.
      Shell::Get()->overview_controller()->EndOverview(
          OverviewSession::EnterExitOverviewType::kImmediateExit);
    }
    SplitViewController* split_view_controller =
        SplitViewController::Get(Shell::GetPrimaryRootWindow());
    split_view_controller->EndSplitView(
        SplitViewController::EndReason::kDesksChange);

    controller_->ActivateDeskInternal(ending_desk,
                                      /*update_window_activation=*/true);

    MaybeRestoreSplitView(/*refresh_snapped_windows=*/true);
  }

  void OnDeskSwitchAnimationFinishedInternal() override {}

  void ReportSmoothness(int smoothness) const override {
    UMA_HISTOGRAM_PERCENTAGE(kDeskActivationSmoothnessHistogramName,
                             smoothness);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DeskActivationAnimation);
};

// -----------------------------------------------------------------------------
// DesksController::DeskRemovalAnimation:

class DesksController::DeskRemovalAnimation
    : public DesksController::DeskAnimationBase {
 public:
  DeskRemovalAnimation(DesksController* controller,
                       const Desk* desk_to_remove,
                       const Desk* desk_to_activate,
                       bool move_left,
                       DesksCreationRemovalSource source)
      : DeskAnimationBase(controller),
        desk_to_remove_(desk_to_remove),
        request_source_(source) {
    DCHECK(!Shell::Get()->overview_controller()->InOverviewSession());
    DCHECK_EQ(controller_->active_desk(), desk_to_remove_);

    for (auto* root : Shell::GetAllRootWindows()) {
      desk_switch_animators_.emplace_back(
          std::make_unique<RootWindowDeskSwitchAnimator>(root, desk_to_activate,
                                                         this, move_left,
                                                         /*for_remove=*/true));
    }
  }

  ~DeskRemovalAnimation() override = default;

  // DesksController::AbstractDeskSwitchAnimation:
  void OnStartingDeskScreenshotTakenInternal(const Desk* ending_desk) override {
    DCHECK_EQ(controller_->active_desk(), desk_to_remove_);
    // We are removing the active desk, which may have tablet split view active.
    // We will restore the split view state of the newly activated desk at the
    // end of the animation. Clamshell split view is impossible because
    // |DeskRemovalAnimation| is not used in overview.
    SplitViewController* split_view_controller =
        SplitViewController::Get(Shell::GetPrimaryRootWindow());
    split_view_controller->EndSplitView(
        SplitViewController::EndReason::kDesksChange);

    // At the end of phase (1), we activate the target desk (i.e. the desk that
    // will be activated after the active desk `desk_to_remove_` is removed).
    // This means that phase (2) will take a screenshot of that desk before we
    // move the windows of `desk_to_remove_` to that target desk.
    controller_->ActivateDeskInternal(ending_desk,
                                      /*update_window_activation=*/false);
  }

  void OnDeskSwitchAnimationFinishedInternal() override {
    // Do the actual desk removal behind the scenes before the screenshot layers
    // are destroyed.
    controller_->RemoveDeskInternal(desk_to_remove_, request_source_);

    MaybeRestoreSplitView(/*refresh_snapped_windows=*/true);
  }

  void ReportSmoothness(int smoothness) const override {
    UMA_HISTOGRAM_PERCENTAGE(kDeskRemovalSmoothnessHistogramName, smoothness);
  }

 private:
  const Desk* const desk_to_remove_;
  const DesksCreationRemovalSource request_source_;

  DISALLOW_COPY_AND_ASSIGN(DeskRemovalAnimation);
};

// -----------------------------------------------------------------------------
// DesksController:

DesksController::DesksController() {
  Shell::Get()->activation_client()->AddObserver(this);
  Shell::Get()->session_controller()->AddObserver(this);

  for (int id : desks_util::GetDesksContainersIds())
    available_container_ids_.push(id);

  // There's always one default desk.
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

void DesksController::Shutdown() {
  animations_.clear();
}

void DesksController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DesksController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool DesksController::AreDesksBeingModified() const {
  return are_desks_being_modified_ || !animations_.empty();
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

  desks_.emplace_back(std::make_unique<Desk>(available_container_ids_.front()));
  available_container_ids_.pop();

  UMA_HISTOGRAM_ENUMERATION(kNewDeskHistogramName, source);
  ReportDesksCountHistogram();
  Shell::Get()
      ->accessibility_controller()
      ->TriggerAccessibilityAlertWithMessage(l10n_util::GetStringFUTF8(
          IDS_ASH_VIRTUAL_DESKS_ALERT_NEW_DESK_CREATED,
          base::NumberToString16(desks_.size())));

  for (auto& observer : observers_)
    observer.OnDeskAdded(desks_.back().get());
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
    const bool move_left = current_desk_index < target_desk_index;
    animations_.emplace_back(std::make_unique<DeskRemovalAnimation>(
        this, desk, desks_[target_desk_index].get(), move_left, source));
    animations_.back()->Launch();
    return;
  }

  RemoveDeskInternal(desk, source);
}

void DesksController::ActivateDesk(const Desk* desk, DesksSwitchSource source) {
  DCHECK(HasDesk(desk));

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
            IDS_ASH_VIRTUAL_DESKS_ALERT_DESK_ACTIVATED,
            base::NumberToString16(target_desk_index + 1)));
  }

  if (source == DesksSwitchSource::kDeskRemoved ||
      source == DesksSwitchSource::kUserSwitch) {
    // Desk switches due to desks removal or user switches in a multi-profile
    // session result in immediate desk activation without animation.
    ActivateDeskInternal(desk, /*update_window_activation=*/!in_overview);
    return;
  }

  // New desks are always added at the end of the list to the right of existing
  // desks. Therefore, desks at lower indices are located on the left of desks
  // with higher indices.
  const bool move_left = GetDeskIndex(active_desk_) < target_desk_index;
  animations_.emplace_back(
      std::make_unique<DeskActivationAnimation>(this, desk, move_left));
  animations_.back()->Launch();
}

bool DesksController::ActivateAdjacentDesk(bool going_left,
                                           DesksSwitchSource source) {
  // An on-going desk switch animation might be in progress. For now skip this
  // accelerator or touchpad event. Later we might want to consider queueing
  // these animations, or cancelling the on-going ones and start over.
  // TODO(afakhry): Discuss with UX.
  if (AreDesksBeingModified())
    return false;

  const Desk* desk_to_activate = going_left ? GetPreviousDesk() : GetNextDesk();
  if (desk_to_activate) {
    ActivateDesk(desk_to_activate, source);
  } else {
    for (auto* root : Shell::GetAllRootWindows())
      desks_animations::PerformHitTheWallAnimation(root, going_left);
  }

  return true;
}

bool DesksController::MoveWindowFromActiveDeskTo(
    aura::Window* window,
    Desk* target_desk,
    DesksMoveWindowFromActiveDeskSource source) {
  DCHECK_NE(active_desk_, target_desk);

  // An active window might be an always-on-top or pip which doesn't belong to
  // the active desk, and hence cannot be removed.
  if (!base::Contains(active_desk_->windows(), window))
    return false;

  base::AutoReset<bool> in_progress(&are_desks_being_modified_, true);

  auto* overview_controller = Shell::Get()->overview_controller();
  const bool in_overview = overview_controller->InOverviewSession();

  active_desk_->MoveWindowToDesk(window, target_desk);

  Shell::Get()
      ->accessibility_controller()
      ->TriggerAccessibilityAlertWithMessage(l10n_util::GetStringFUTF8(
          IDS_ASH_VIRTUAL_DESKS_ALERT_WINDOW_MOVED_FROM_ACTIVE_DESK,
          window->GetTitle(),
          base::NumberToString16(GetDeskIndex(active_desk_) + 1),
          base::NumberToString16(GetDeskIndex(target_desk) + 1)));

  UMA_HISTOGRAM_ENUMERATION(kMoveWindowFromActiveDeskHistogramName, source);
  ReportNumberOfWindowsPerDeskHistogram();

  if (in_overview) {
    DCHECK(overview_controller->InOverviewSession());
    auto* overview_session = overview_controller->overview_session();
    auto* item = overview_session->GetOverviewItemForWindow(window);
    DCHECK(item);
    // Restore the dragged item window, so that its transform is reset to
    // identity.
    item->RestoreWindow(/*reset_transform=*/true);
    // The item no longer needs to be in the overview grid.
    overview_session->RemoveItem(item);
    // When in overview, we should return immediately and not change the window
    // activation as we do below, since the dummy "OverviewModeFocusedWidget"
    // should remain active while overview mode is active..
    return true;
  }

  // A window moving out of the active desk cannot be active.
  wm::DeactivateWindow(window);
  return true;
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

void DesksController::OnAnimationFinished(DeskAnimationBase* animation) {
  base::EraseIf(animations_, base::MatchesUniquePtr(animation));
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

    // If overview mode is active, we add the windows of the removed desk to the
    // overview grid in the order of their MRU. Note that this can only be done
    // after the windows have moved to the active desk above, so that building
    // the window MRU list should contain those windows.
    if (in_overview)
      AppendWindowsToOverview(removed_desk_windows, /*should_animate=*/true);
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

    // The removed desk is the active desk, so temporarily remove its windows
    // from the overview grid which will result in removing the
    // "OverviewModeLabel" widgets created by overview mode for these windows.
    // This way the removed desk tracks only real windows, which are now ready
    // to be moved to the target desk.
    if (in_overview)
      RemoveWindowsFromOverview(removed_desk_windows);

    // If overview mode is active, change desk activation without changing
    // window activation. Activation should remain on the dummy
    // "OverviewModeFocusedWidget" while overview mode is active.
    removed_desk->MoveWindowsToDesk(target_desk);
    ActivateDesk(target_desk, DesksSwitchSource::kDeskRemoved);

    // Desk activation should not change overview mode state.
    DCHECK_EQ(in_overview, overview_controller->InOverviewSession());

    // Now that the windows from the removed and target desks merged, add them
    // all without animation to the grid in the order of their MRU.
    if (in_overview)
      AppendWindowsToOverview(target_desk->windows(), /*should_animate=*/false);
  }

  // It's OK now to refresh the mini_views of *only* the active desk, and only
  // if windows from the removed desk moved to it.
  DCHECK(active_desk_->should_notify_content_changed());
  if (!removed_desk_windows.empty())
    active_desk_->NotifyContentChanged();

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
          IDS_ASH_VIRTUAL_DESKS_ALERT_DESK_REMOVED,
          base::NumberToString16(removed_desk_number),
          base::NumberToString16(active_desk_number)));

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

}  // namespace ash
