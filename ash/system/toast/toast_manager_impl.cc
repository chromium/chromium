// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/toast/toast_manager_impl.h"

#include <algorithm>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"

namespace ash {

namespace {

// Minimum duration for a toast to be visible (in millisecond).
const int32_t kMinimumDurationMs = 200;

}  // anonymous namespace

ToastManagerImpl::ToastManagerImpl()
    : locked_(Shell::Get()->session_controller()->IsScreenLocked()) {}

ToastManagerImpl::~ToastManagerImpl() = default;

void ToastManagerImpl::Show(const ToastData& data) {
  const std::string& id = data.id;
  DCHECK(!id.empty());

  if (current_toast_data_ && current_toast_data_->id == id) {
    // TODO(yoshiki): Replaces the visible toast.
    return;
  }

  auto existing_toast =
      std::find_if(queue_.begin(), queue_.end(),
                   [&id](const ToastData& data) { return data.id == id; });

  if (existing_toast == queue_.end()) {
    queue_.emplace_back(data);
  } else {
    *existing_toast = data;
  }

  if (queue_.size() == 1 && overlay_ == nullptr)
    ShowLatest();
}

void ToastManagerImpl::Cancel(const std::string& id) {
  if (current_toast_data_ && current_toast_data_->id == id) {
    overlay_->Show(false);
    return;
  }

  auto cancelled_toast =
      std::find_if(queue_.begin(), queue_.end(),
                   [&id](const ToastData& data) { return data.id == id; });
  if (cancelled_toast != queue_.end())
    queue_.erase(cancelled_toast);
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
      current_toast_data_->visible_on_lock_screen && locked_);
  overlay_->Show(true);

  if (current_toast_data_->duration_ms != ToastData::kInfiniteDuration) {
    int32_t duration_ms =
        std::max(current_toast_data_->duration_ms, kMinimumDurationMs);
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ToastManagerImpl::OnDurationPassed,
                       weak_ptr_factory_.GetWeakPtr(), serial_),
        base::TimeDelta::FromMilliseconds(duration_ms));
  }
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
