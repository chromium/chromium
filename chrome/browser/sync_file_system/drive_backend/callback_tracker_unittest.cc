// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/callback_tracker.h"

#include "base/functional/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

void Receiver(bool* called) {
  DCHECK(called);
  EXPECT_FALSE(*called);
  *called = true;
}

}  // namespace

TEST(CallbackTrackerTest, AbortAll) {
  CallbackTracker tracker;

  bool aborted = false;
  bool invoked = false;
  base::OnceClosure callback = tracker.Register(
      base::BindOnce(&Receiver, &aborted), base::BindOnce(&Receiver, &invoked));
  tracker.AbortAll();
  EXPECT_TRUE(aborted);
  EXPECT_FALSE(invoked);

  std::move(callback).Run();
  EXPECT_TRUE(aborted);
  EXPECT_FALSE(invoked);
}

TEST(CallbackTrackerTest, Invoke) {
  CallbackTracker tracker;

  bool aborted = false;
  bool invoked = false;
  base::OnceClosure callback = tracker.Register(
      base::BindOnce(&Receiver, &aborted), base::BindOnce(&Receiver, &invoked));
  std::move(callback).Run();
  EXPECT_FALSE(aborted);
  EXPECT_TRUE(invoked);

  tracker.AbortAll();
  EXPECT_FALSE(aborted);
}

}  // namespace drive_backend
}  // namespace sync_file_system
