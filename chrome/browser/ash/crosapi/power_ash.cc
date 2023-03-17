// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/power_ash.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "extensions/browser/api/power/activity_reporter_delegate.h"
#include "services/device/wake_lock/power_save_blocker/power_save_blocker.h"

namespace crosapi {

PowerAsh::PowerAsh()
    : main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      file_task_runner_(base::ThreadPool::CreateSingleThreadTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})) {
  lock_set_.set_disconnect_handler(base::BindRepeating(
      &PowerAsh::OnLockDisconnect, weak_ptr_factory_.GetWeakPtr()));
}

PowerAsh::~PowerAsh() = default;

void PowerAsh::BindReceiver(mojo::PendingReceiver<mojom::Power> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void PowerAsh::AddPowerSaveBlocker(
    mojo::PendingRemote<mojom::PowerWakeLock> lock,
    device::mojom::WakeLockType type,
    device::mojom::WakeLockReason reason,
    const std::string& description) {
  mojo::RemoteSetElementId id = lock_set_.Add(std::move(lock));
  power_save_blockers_[id] = std::make_unique<device::PowerSaveBlocker>(
      type, reason, description, main_task_runner_, file_task_runner_);
}

void PowerAsh::ReportActivity() {
  extensions::ActivityReporterDelegate::GetDelegate()->ReportActivity();
}

void PowerAsh::OnLockDisconnect(mojo::RemoteSetElementId id) {
  power_save_blockers_.erase(id);
}

}  // namespace crosapi
