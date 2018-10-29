// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/bluetooth/arc_bluetooth_task_queue.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

TEST(ArcBluetoothTaskQueueTest, Serial) {
  bool done = false;
  ArcBluetoothTaskQueue task_queue;
  task_queue.Push(base::DoNothing());
  task_queue.Push(base::BindOnce([](bool* done) { *done = true; }, &done));
  EXPECT_FALSE(done);
  task_queue.Pop();
  EXPECT_TRUE(done);
  task_queue.Pop();
}

TEST(ArcBluetoothTaskQueueTest, Order) {
  std::string str;
  ArcBluetoothTaskQueue task_queue;
  task_queue.Push(base::BindOnce([](std::string* str) { *str += '1'; }, &str));
  task_queue.Push(base::BindOnce([](std::string* str) { *str += '2'; }, &str));
  task_queue.Push(base::BindOnce([](std::string* str) { *str += '3'; }, &str));
  task_queue.Pop();
  task_queue.Pop();
  task_queue.Pop();
  EXPECT_EQ("123", str);
}

}  // namespace arc
