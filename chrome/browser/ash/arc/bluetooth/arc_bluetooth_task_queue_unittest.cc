// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/bluetooth/arc_bluetooth_task_queue.h"

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

TEST(ArcBluetoothTaskQueueTest, Serial) {
  bool done = false;
  ArcBluetoothTaskQueue task_queue;
  task_queue.Push(base::DoNothing());
  task_queue.Push(base::BindLambdaForTesting([&]() { done = true; }));
  EXPECT_FALSE(done);
  task_queue.Pop();
  EXPECT_TRUE(done);
  task_queue.Pop();
}

TEST(ArcBluetoothTaskQueueTest, Order) {
  std::string str;
  ArcBluetoothTaskQueue task_queue;
  task_queue.Push(base::BindLambdaForTesting([&]() { str += '1'; }));
  task_queue.Push(base::BindLambdaForTesting([&]() { str += '2'; }));
  task_queue.Push(base::BindLambdaForTesting([&]() { str += '3'; }));
  task_queue.Pop();
  task_queue.Pop();
  task_queue.Pop();
  EXPECT_EQ("123", str);
}

}  // namespace arc
