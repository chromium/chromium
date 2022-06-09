// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/toast_manager_impl.h"

#include <algorithm>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"

namespace ash {

ToastManagerImpl::ToastManagerImpl()
    : locked_(Shell::Get()->session_controller()->IsScreenLocked()) {}

ToastManagerImpl::~ToastManagerImpl() = default;

void ToastManagerImpl::Show(const ToastData& data) {
  const std::string& id = data.id;
  DCHECK(!id.empty());

  auto existing_toast =
      std::find_if(queue_.begin(), queue_.end(),
                   [&id](const ToastData& data) { return data.id == id; });

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
      overlay_->Show(false);
      return;
    }

    queue_.emplace_back(data);
  }

  if (queue_.size() == 1 && overlay_ == nullptr)
    ShowLatest();
}

void ToastManagerImpl::Cancel(const std::string& id) {
  if (IsRunning(id)) {
    overlay_->Show(false);
    return;
  }

  auto cancelled_toast =
      std::find_if(queue_.begin(), queue_.end(),
                   [&id](const ToastData& data) { return data.id == id; });
  if (cancelled_toast != queue_.end())
    queue_.erase(cancelled_toast);
}

bool ToastManagerImpl::MaybeToggleA11yHighlightOnActiveToastDismissButton(
    const std::string& id) {
  DCHECK(IsRunning(id));
  return overlay_ && overlay_->MaybeToggleA11yHighlightOnDismissButton();
}

bool ToastManagerImpl::MaybeActivateHighlightedDismissButtonOnActiveToast(
    const std::string& id) {
  DCHECK(IsRunning(id));
  return overlay_ && overlay_->MaybeActivateHighlightedDismissButton();
}

bool ToastManagerImpl::IsRunning(const std::string& id) const {
  return overlay_ && current_toast_data_ && current_toast_data_->id == id;
}

void ToastManagerImpl::OnClosed() {
  overlay_.reset();
  current_toast_data_.reset();

  // Show the next toast if available.
  // Note that don't show during the lock state is changing, since we reshow
  // manually after the state is changed. See OnLockStateChanged.
  if (!queue_.empty())
    ShowLatest();
}

void ToastManagerImpl::ShowLatest() {
  DCHECK(!overlay_);
  DCHECK(!current_toast_data_);

  auto it = locked_ ? std::find_if(queue_.begin(), queue_.end(),
                                   [](const auto& data) {
                                     return data.visible_on_lock_screen;
                                   })
                    : queue_.begin();
  if (it == queue_.end())
    return;

  current_toast_data_ = *it;
  queue_.erase(it);

  serial_++;

  overlay_ = std::make_unique<ToastOverlay>(
      this, current_toast_data_->text, current_toast_data_->dismiss_text,
      current_toast_data_->visible_on_lock_screen && locked_,
      current_toast_data_->is_managed, current_toast_data_->dismiss_callback,
      current_toast_data_->expired_callback);
  overlay_->Show(true);

  if (current_toast_data_->duration != ToastData::kInfiniteDuration) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ToastManagerImpl::OnDurationPassed,
                       weak_ptr_factory_.GetWeakPtr(), serial_),
        current_toast_data_->duration);
  }

  base::UmaHistogramEnumeration("NotifierFramework.Toast.ShownCount",
                                current_toast_data_->catalog_name);
  base::UmaHistogramMediumTimes(
      "NotifierFramework.Toast.TimeInQueue",
      base::TimeTicks::Now() - current_toast_data_->time_created);
}

void ToastManagerImpl::OnDurationPassed(int toast_number) {
  if (overlay_ && serial_ == toast_number)
    overlay_->Show(false);
}

void ToastManagerImpl::OnSessionStateChanged(
    session_manager::SessionState state) {
  const bool locked = state != session_manager::SessionState::ACTIVE;

  if ((locked != locked_) && current_toast_data_) {
    // Re-queue the currently visible toast which is not for lock screen.
    queue_.push_front(*current_toast_data_);
    current_toast_data_.reset();
    // Hide the currently visible toast without any animation.
    overlay_.reset();
  }

  locked_ = locked;
  if (!queue_.empty()) {
    // Try to reshow a queued toast from a previous OnSessionStateChanged.
    ShowLatest();
  }
}

}  // namespace ash
