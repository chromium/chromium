// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/chromeos/arc/bluetooth/arc_bluetooth_task_queue.h"

namespace arc {

ArcBluetoothTaskQueue::ArcBluetoothTaskQueue() = default;
ArcBluetoothTaskQueue::~ArcBluetoothTaskQueue() = default;

void ArcBluetoothTaskQueue::Push(base::OnceClosure task) {
  queue_.emplace(std::move(task));
  if (queue_.size() == 1)
    std::move(queue_.front()).Run();
}

void ArcBluetoothTaskQueue::Pop() {
  queue_.pop();
  if (!queue_.empty())
    std::move(queue_.front()).Run();
}

}  // namespace arc
