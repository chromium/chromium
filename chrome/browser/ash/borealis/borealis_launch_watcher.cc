// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_launch_watcher.h"

#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace borealis {

BorealisLaunchWatcher::BorealisLaunchWatcher(Profile* profile,
                                             std::string vm_name)
    : owner_id_(chromeos::ProfileHelper::GetUserIdHashFromProfile(profile)),
      vm_name_(vm_name) {
  chromeos::DBusThreadManager::Get()->GetCiceroneClient()->AddObserver(this);
}

BorealisLaunchWatcher::~BorealisLaunchWatcher() {
  chromeos::DBusThreadManager::Get()->GetCiceroneClient()->RemoveObserver(this);
}

void BorealisLaunchWatcher::AwaitLaunch(OnLaunchCallback callback) {
  if (container_started_signal_) {
    std::move(callback).Run(container_started_signal_->container_name());
  } else {
    if (callback_queue_.empty()) {
      base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&BorealisLaunchWatcher::TimeoutCallback,
                         weak_factory_.GetWeakPtr()),
          timeout_);
    }
    callback_queue_.push(std::move(callback));
  }
}

void BorealisLaunchWatcher::OnContainerStarted(
    const vm_tools::cicerone::ContainerStartedSignal& signal) {
  if (signal.owner_id() == owner_id_ && signal.vm_name() == vm_name_ &&
      !container_started_signal_) {
    container_started_signal_ = signal;
    while (!callback_queue_.empty()) {
      std::move(callback_queue_.front())
          .Run(container_started_signal_->container_name());
      callback_queue_.pop();
    }
  }
}

void BorealisLaunchWatcher::TimeoutCallback() {
  while (!callback_queue_.empty()) {
    std::move(callback_queue_.front()).Run(base::nullopt);
    callback_queue_.pop();
  }
}

}  // namespace borealis
