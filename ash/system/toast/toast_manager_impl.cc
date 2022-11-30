// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/toast_manager_impl.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
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

ToastManagerImpl::ToastManagerImpl()
    : locked_(Shell::Get()->session_controller()->IsScreenLocked()) {
  Shell::Get()->AddShellObserver(this);
}

ToastManagerImpl::~ToastManagerImpl() {
  Shell::Get()->RemoveShellObserver(this);
}

void ToastManagerImpl::Show(const ToastData& data) {
  const std::string& id = data.id;
  DCHECK(!id.empty());

  auto existing_toast = base::ranges::find(queue_, id, &ToastData::id);

  if (existing_toast != queue_.end()) {
    // Assigns given `data` to existing queued toast, but keeps the existing
    // toast's `time_created` value.
    const base::TimeTicks old_time_created = existing_toast->time_created;
    *existing_toast = data;
    existing_toast->time_created = old_time_created;
  } else {
    if (IsRunning(id)) {
      // Replace the visible toast by adding the new toast data to the front of
      // the queue and hiding the visible toast. Once the visible toast finishes
      // hiding, the new toast will be displayed.
      queue_.emplace_front(data);

      CloseAllToastsWithAnimation();

      return;
    }

    queue_.emplace_back(data);
  }

  if (queue_.size() == 1 && !HasActiveToasts())
    ShowLatest();
}

void ToastManagerImpl::Cancel(const std::string& id) {
  if (IsRunning(id)) {
    CloseAllToastsWithAnimation();
    return;
  }

  auto cancelled_toast = base::ranges::find(queue_, id, &ToastData::id);
  if (cancelled_toast != queue_.end())
    queue_.erase(cancelled_toast);
}

bool ToastManagerImpl::MaybeToggleA11yHighlightOnActiveToastDismissButton(
    const std::string& id) {
  DCHECK(IsRunning(id));
  for (auto& iter : root_window_to_overlay_) {
    if (iter.second && iter.second->MaybeToggleA11yHighlightOnDismissButton())
      return true;
  }

  return false;
}

bool ToastManagerImpl::MaybeActivateHighlightedDismissButtonOnActiveToast(
    const std::string& id) {
  DCHECK(IsRunning(id));
  for (auto& iter : root_window_to_overlay_) {
    if (iter.second && iter.second->MaybeActivateHighlightedDismissButton())
      return true;
  }

  return false;
}

bool ToastManagerImpl::IsRunning(const std::string& id) const {
  return HasActiveToasts() && current_toast_data_ &&
         current_toast_data_->id == id;
}

void ToastManagerImpl::OnClosed() {
  const base::TimeDelta user_journey_time =
      base::TimeTicks::Now() - current_toast_data_->time_start_showing;
  const std::string time_range = GetToastDismissedTimeRange(user_journey_time);
  base::UmaHistogramEnumeration(
      base::StringPrintf("%s.Dismissed.%s", NotifierFrameworkToastHistogram,
                         time_range.c_str()),
      current_toast_data_->catalog_name);

  CloseAllToastsWithoutAnimation();

  current_toast_data_.reset();

  // Show the next toast if available.
  // Note that don't show during the lock state is changing, since we reshow
  // manually after the state is changed. See OnLockStateChanged.
  if (!queue_.empty())
    ShowLatest();
}

void ToastManagerImpl::OnToastHoverStateChanged(bool is_hovering) {
  DCHECK(current_toast_data_->persist_on_hover);
  if (!current_toast_data_->show_on_all_root_windows)
    return;

  for (auto& iter : root_window_to_overlay_) {
    if (iter.second)
      iter.second->UpdateToastExpirationTimer(is_hovering);
  }
}

void ToastManagerImpl::OnSessionStateChanged(
    session_manager::SessionState state) {
  const bool locked = state != session_manager::SessionState::ACTIVE;

  if ((locked != locked_) && current_toast_data_) {
    // Re-queue the currently visible toast which is not for lock screen.
    queue_.push_front(*current_toast_data_);
    current_toast_data_.reset();
    // Hide the currently visible toast instances without any animation.
    CloseAllToastsWithoutAnimation();
  }

  locked_ = locked;
  if (!queue_.empty()) {
    // Try to reshow a queued toast from a previous OnSessionStateChanged.
    ShowLatest();
  }
}

void ToastManagerImpl::ShowLatest() {
  DCHECK(!HasActiveToasts());
  DCHECK(!current_toast_data_);

  auto it = locked_ ? base::ranges::find(queue_, true,
                                         &ToastData::visible_on_lock_screen)
                    : queue_.begin();
  if (it == queue_.end())
    return;

  current_toast_data_ = *it;
  queue_.erase(it);

  serial_++;

  if (current_toast_data_->show_on_all_root_windows) {
    for (auto* root_window : Shell::GetAllRootWindows())
      CreateToastOverlayForRoot(root_window);
  } else {
    CreateToastOverlayForRoot(Shell::GetRootWindowForNewWindows());
  }

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
      this, current_toast_data_->text, current_toast_data_->dismiss_text,
      current_toast_data_->duration,
      current_toast_data_->visible_on_lock_screen && locked_,
      current_toast_data_->is_managed, current_toast_data_->persist_on_hover,
      root_window, current_toast_data_->dismiss_callback,
      current_toast_data_->expired_callback);
  new_overlay->Show(true);

  // We only want to record this value when the first instance of the toast is
  // initialized.
  if (current_toast_data_->time_start_showing.is_null())
    current_toast_data_->time_start_showing = new_overlay->time_started();
}

void ToastManagerImpl::CloseAllToastsWithAnimation() {
  for (auto& iter : root_window_to_overlay_) {
    if (iter.second)
      iter.second->Show(false);
  }
}

void ToastManagerImpl::CloseAllToastsWithoutAnimation() {
  for (auto& iter : root_window_to_overlay_)
    iter.second.reset();
}

bool ToastManagerImpl::HasActiveToasts() const {
  for (auto& iter : root_window_to_overlay_) {
    if (iter.second)
      return true;
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
  if (current_toast_data_ && !current_toast_data_->show_on_all_root_windows)
    return;

  // If the toast is displaying on multiple monitors and one of the root windows
  // shuts down, then we do not want for that toast to run the
  // `expired_callback_` when it is being destroyed.
  auto& toast_overlay = root_window_to_overlay_[root_window];

  if (!toast_overlay)
    return;

  toast_overlay->ResetExpiredCallback();
  toast_overlay.reset();
}

}  // namespace ash
