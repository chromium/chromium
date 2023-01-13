// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/default_delayed_task_handle_delegate.h"

#include "base/functional/callback_helpers.h"
#include "base/task/delayed_task_handle.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

// Tests that running the bound callback invalidates the handle.
TEST(DefaultDelayedTaskHandleDelegateTest, RunTask) {
  auto delegate = std::make_unique<DefaultDelayedTaskHandleDelegate>();
  EXPECT_FALSE(delegate->IsValid());

  auto bound_callback = delegate->BindCallback(DoNothing());
  EXPECT_TRUE(delegate->IsValid());

  DelayedTaskHandle delayed_task_handle(std::move(delegate));
  EXPECT_TRUE(delayed_task_handle.IsValid());

  std::move(bound_callback).Run();
  EXPECT_FALSE(delayed_task_handle.IsValid());
}

// Tests that DefaultDelayedTaskHandleDelegate supports CancelTask().
TEST(DefaultDelayedTaskHandleDelegateTest, CancelTask) {
  auto delegate = std::make_unique<DefaultDelayedTaskHandleDelegate>();
  EXPECT_FALSE(delegate->IsValid());

  auto bound_callback = delegate->BindCallback(DoNothing());
  EXPECT_TRUE(delegate->IsValid());

  DelayedTaskHandle delayed_task_handle(std::move(delegate));
  EXPECT_TRUE(delayed_task_handle.IsValid());

  delayed_task_handle.CancelTask();
  EXPECT_FALSE(delayed_task_handle.IsValid());
  EXPECT_TRUE(bound_callback.IsCancelled());
}

// Tests that destroying the bound callback invalidates the handle.
TEST(DefaultDelayedTaskHandleDelegateTest, DestroyTask) {
  auto delegate = std::make_unique<DefaultDelayedTaskHandleDelegate>();
  EXPECT_FALSE(delegate->IsValid());

  auto bound_callback = delegate->BindCallback(DoNothing());
  EXPECT_TRUE(delegate->IsValid());

  DelayedTaskHandle delayed_task_handle(std::move(delegate));
  EXPECT_TRUE(delayed_task_handle.IsValid());

  bound_callback = OnceClosure();
  EXPECT_FALSE(delayed_task_handle.IsValid());
}

// Tests that the handle is invalid while the task is running.
TEST(DefaultDelayedTaskHandleDelegateTest, HandleInvalidInsideCallback) {
  auto delegate = std::make_unique<DefaultDelayedTaskHandleDelegate>();
  EXPECT_FALSE(delegate->IsValid());

  auto bound_callback = delegate->BindCallback(
      BindLambdaForTesting([delegate = delegate.get()]() {
        // Check that the delegate is invalid inside the callback.
        EXPECT_FALSE(delegate->IsValid());
      }));
  EXPECT_TRUE(delegate->IsValid());

  DelayedTaskHandle delayed_task_handle(std::move(delegate));
  EXPECT_TRUE(delayed_task_handle.IsValid());

  std::move(bound_callback).Run();
  EXPECT_FALSE(delayed_task_handle.IsValid());
}

}  // namespace base
