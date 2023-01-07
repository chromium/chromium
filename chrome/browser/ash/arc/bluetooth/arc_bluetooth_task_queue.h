// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_BLUETOOTH_ARC_BLUETOOTH_TASK_QUEUE_H_
#define CHROME_BROWSER_ASH_ARC_BLUETOOTH_ARC_BLUETOOTH_TASK_QUEUE_H_

#include "base/containers/queue.h"
#include "base/functional/callback.h"

namespace arc {

// A queue to ensure serial and ordered execution of tasks.
class ArcBluetoothTaskQueue {
 public:
  ArcBluetoothTaskQueue();

  ArcBluetoothTaskQueue(const ArcBluetoothTaskQueue&) = delete;
  ArcBluetoothTaskQueue& operator=(const ArcBluetoothTaskQueue&) = delete;

  ~ArcBluetoothTaskQueue();

  // Pushes |task| into this queue.
  void Push(base::OnceClosure task);

  // Pops the current task from this queue, indicating its completion.
  void Pop();

 private:
  base::queue<base::OnceClosure> queue_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_BLUETOOTH_ARC_BLUETOOTH_TASK_QUEUE_H_
