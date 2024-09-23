// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/toast_manager_impl.h"

#include "ash/public/cpp/system/scoped_toast_pause.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"

namespace ash {

namespace {

constexpr char NotifierFrameworkToastHistogram[] =
    "Ash.NotifierFramework.Toast";

// Used in histogram names.
std::string GetToastDismissedTimeRange(const base::TimeDelta& time) {
  if (time <= base::Seconds(2))
    return "Within2s";
  // Toast default duration is 6s, but with animation it's usually
  // around ~6.2s, so recording 7s as the default case.
  if (time <= base::Seconds(7))
    return "Within7s";
  return "After7s";
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// PausableTimer:
// Timer class that owns a `base::OneShotTimer` that can be paused and resumed
// by the `ToastManagerImpl` to continue with the remainder of its duration.
// Different from `base::RetainingOneShotTimer` in that restarting it will set
// the duration for the remainder of the time that it had left when it was
// paused.
class ToastManagerImpl::PausableTimer {
 public:
  PausableTimer() = default;
  PausableTimer(const PausableTimer&) = delete;
  PausableTimer& operator=(const PausableTimer&) = delete;
  ~PausableTimer() = default;

  // Returns whether `timer_` is running.
  bool IsRunning() const { return timer_.IsRunning(); }

  // Starts `timer_` with a duration of `duration` and a scheduled task of
  // `task`.
  void Start(base::TimeDelta duration, base::RepeatingClosure task) {
    DCHECK(!duration.is_max());
    DCHECK(task);
    DCHECK(!IsRunning());
    duration_remaining_ = duration;
    task_ = task;
    timer_.Start(FROM_HERE, duration_remaining_, task_);
    time_last_started_ = base::TimeTicks::Now();
  }

  // Stops the timer, allowing for the user to call `Resume` at a later time to
  // continue the timer.
  void Pause() {
    DCHECK(IsRunning());
    timer_.Stop();
    duration_remaining_ -= base::TimeTicks::Now() - time_last_started_;
  }

  // Restarts the timer with a duration of `duration_remaining_`.
  void Resume() { Start(duration_remaining_, task_); }

  // Fully stops the timer without leaving a chance to call `Resume` later.
  void Stop() {
    task_.Reset();
    duration_remaining_ = base::Seconds(0);
    timer_.Stop();
  }

 private:
  // Task that will be run when `timer_` has elapsed.
  base::RepeatingClosure task_;

  // Time remaining for the timer. Allows for us to calculate how much time is
  // remaining when the timer is paused.
  base::TimeDelta duration_remaining_;

  // Tracks when `timer_` was last started so that we can calculate
  // `duration_remaining_` if the timer is later paused.
  base::TimeTicks time_last_started_;

  // A timer that will run `task_` when `duration_remaining_` has elapsed if
  // it is not paused before then.
  base::OneShotTimer timer_;
};

///////////////////////////////////////////////////////////////////////////////
// ToastManagerImpl:
ToastManagerImpl::ToastManagerImpl()
    : current_toast_expiration_timer_(std::make_unique<PausableTimer>()),
      locked_(Shell::Get()->session_controller()->IsScreenLocked()) {
  Shell::Get()->AddShellObserver(this);
}

ToastManagerImpl::~ToastManagerImpl() {
  Shell::Get()->RemoveShellObserver(this);

  // If there are live `ToastOverlay`s, destroying `current_toast_data_` can
  // call into the `ToastOverlay`s and then back into `ToastManagerImpl`, which
  // then tries to destroy the already-being-destroyed `current_toast_data_`.
  CloseAllToastsWithoutAnimation();
}

void ToastManagerImpl::Show(ToastData data) {
  std::string_view id = data.id;
  DCHECK(!id.empty());

  LOG(ERROR) << "Show toast called, toast id: " << id;

  // If `pause_counter_` is greater than 0, no toasts should be shown.
  if (pause_counter_ > 0) {
    LOG(ERROR)
        << "Toast not shown, pause_counter_ is creater than 0, toast id: "
        << id;
    return;
  }

  auto existing_toast = base::ranges::find(queue_, id, &ToastData::id);

  if (existing_toast != queue_.end()) {
    // Assigns given `data` to existing queued toast, but keeps the existing
    // toast's `time_created` value.
    const base::TimeTicks old_time_created = existing_toast->time_created;
    *existing_toast = std::move(data);
    existing_toast->time_created = old_time_created;
  } else {
    if (IsToastShown(id)) {
      // Replace the visible toast by adding the new toast data to the front of
      // the queue and hiding the visible toast. Once the visible toast finishes
      // hiding, the new toast will be displayed.
      queue_.emplace_front(std::move(data));

      CloseAllToastsWithAnimation();

      return;
    }

    queue_.emplace_back(std::move(data));
  }

  if (queue_.size() == 1 && !HasActiveToasts())
    ShowLatest();
}

void ToastManagerImpl::Cancel(std::string_view id) {
  if (IsToastShown(id)) {
    CloseAllToastsWithAnimation();
    return;
  }

  auto cancelled_toast = base::ranges::find(queue_, id, &ToastData::id);
  if (cancelled_toast != queue_.end())
    queue_.erase(cancelled_toast);
}

bool ToastManagerImpl::RequestFocusOnActiveToastDismissButton(
    std::string_view id) {
  CHECK(IsToastShown(id));
  for (auto& [_, overlay] : root_window_to_overlay_) {
    if (overlay && overlay->RequestFocusOnActiveToastDismissButton()) {
      return true;
    }
  }
  return false;
}

bool ToastManagerImpl::IsToastShown(std::string_view id) const {
  return HasActiveToasts() && current_toast_data_ &&
         current_toast_data_->id == id;
}

bool ToastManagerImpl::IsToastDismissButtonFocused(std::string_view id) const {
  if (!IsToastShown(id)) {
    return false;
  }

  for (const auto& [_, overlay] : root_window_to_overlay_) {
    if (overlay && overlay->IsDismissButtonFocused()) {
      return true;
    }
  }

  return false;
}

std::unique_ptr<ScopedToastPause> ToastManagerImpl::CreateScopedPause() {
  return std::make_unique<ScopedToastPause>();
}

void ToastManagerImpl::CloseToast() {
  const base::TimeDelta user_journey_time =
      base::TimeTicks::Now() - current_toast_data_->time_start_showing;
  const std::string time_range = GetToastDismissedTimeRange(user_journey_time);
  base::UmaHistogramEnumeration(
      base::StringPrintf("%s.Dismissed.%s", NotifierFrameworkToastHistogram,
                         time_range.c_str()),
      current_toast_data_->catalog_name);

  CloseAllToastsWithoutAnimation();

  current_toast_data_.reset();
  current_toast_expiration_timer_->Stop();

  // Show the next toast if available.
  // Note that don't show during the lock state is changing, since we reshow
  // manually after the state is changed. See OnLockStateChanged.
  if (!queue_.empty())
    ShowLatest();
}

void ToastManagerImpl::OnToastHoverStateChanged(bool is_hovering) {
  DCHECK(current_toast_data_->persist_on_hover);

  if (is_hovering != current_toast_expiration_timer_->IsRunning())
    return;

  is_hovering ? current_toast_expiration_timer_->Pause()
              : current_toast_expiration_timer_->Resume();
}

void ToastManagerImpl::OnSessionStateChanged(
    session_manager::SessionState state) {
  locked_ = state != session_manager::SessionState::ACTIVE;
  current_toast_data_.reset();
  CloseAllToastsWithoutAnimation();
}

void ToastManagerImpl::ShowLatest() {
  DCHECK(!HasActiveToasts());
  DCHECK(!current_toast_data_);

  auto it = locked_ ? base::ranges::find(queue_, true,
                                         &ToastData::visible_on_lock_screen)
                    : queue_.begin();
  if (it == queue_.end())
    return;

  current_toast_data_ = std::move(*it);
  queue_.erase(it);

  LOG(ERROR) << "Showing latest toast, toast id: " << current_toast_data_->id;
  serial_++;

  if (current_toast_data_->show_on_all_root_windows) {
    for (aura::Window* root_window : Shell::GetAllRootWindows()) {
      CreateToastOverlayForRoot(root_window);
    }
  } else {
    CreateToastOverlayForRoot(Shell::GetRootWindowForNewWindows());
  }

  DCHECK(!current_toast_expiration_timer_->IsRunning());

  current_toast_expiration_timer_->Start(
      current_toast_data_->duration,
      base::BindRepeating(&ToastManagerImpl::CloseAllToastsWithAnimation,
                          base::Unretained(this)));

  base::UmaHistogramEnumeration("Ash.NotifierFramework.Toast.ShownCount",
                                current_toast_data_->catalog_name);
  base::UmaHistogramMediumTimes(
      "Ash.NotifierFramework.Toast.TimeInQueue",
      base::TimeTicks::Now() - current_toast_data_->time_created);
}

void ToastManagerImpl::CreateToastOverlayForRoot(aura::Window* root_window) {
  auto& new_overlay = root_window_to_overlay_[root_window];
  DCHECK(!new_overlay);
  DCHECK(current_toast_data_);
  new_overlay = std::make_unique<ToastOverlay>(
      /*delegate=*/this, *current_toast_data_, root_window);
  new_overlay->Show(true);

  // We only want to record this value when the first instance of the toast is
  // initialized.
  if (current_toast_data_->time_start_showing.is_null())
    current_toast_data_->time_start_showing = base::TimeTicks::Now();
}

void ToastManagerImpl::CloseAllToastsWithAnimation() {
  for (auto& [_, overlay] : root_window_to_overlay_) {
    if (overlay) {
      overlay->Show(false);
    }
  }
}

void ToastManagerImpl::CloseAllToastsWithoutAnimation() {
  for (auto& [_, overlay] : root_window_to_overlay_) {
    overlay.reset();
  }

  // `OnClosed` (the other place where we stop the
  // `current_toast_expiration_timer_`) is only called when the toast is being
  // closed with animation, so we still want to stop the timer here for when it
  // is not animating to close.
  current_toast_expiration_timer_->Stop();
}

bool ToastManagerImpl::HasActiveToasts() const {
  for (const auto& [_, overlay] : root_window_to_overlay_) {
    if (overlay) {
      return true;
    }
  }

  return false;
}

ToastOverlay* ToastManagerImpl::GetCurrentOverlayForTesting(
    aura::Window* root_window) {
  return root_window_to_overlay_[root_window].get();
}

void ToastManagerImpl::OnRootWindowAdded(aura::Window* root_window) {
  if (HasActiveToasts() && current_toast_data_ &&
      current_toast_data_->show_on_all_root_windows) {
    CreateToastOverlayForRoot(root_window);
  }
}

void ToastManagerImpl::OnRootWindowWillShutdown(aura::Window* root_window) {
  // If the toast only exists in the root window that is being closed, inform
  // the manager that the toast should be closed.
  if (root_window_to_overlay_[root_window] &&
      !current_toast_data_->show_on_all_root_windows) {
    CloseToast();
  }

  root_window_to_overlay_.erase(root_window);
}

void ToastManagerImpl::Pause() {
  ++pause_counter_;

  // Immediately closes all the toasts. Since `OnClosed` will not be called,
  // manually resets `current_toast_data_` and `queue_`.
  CloseAllToastsWithoutAnimation();
  current_toast_data_.reset();
  queue_.clear();
}

void ToastManagerImpl::Resume() {
  CHECK_GT(pause_counter_, 0);
  --pause_counter_;
}

}  // namespace ash
