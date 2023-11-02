// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/delayed_task_handle.h"

#include "base/memory/raw_ptr.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// A non-working delegate that allows the testing of DelayedTaskHandle.
class TestDelegate : public DelayedTaskHandle::Delegate {
 public:
  explicit TestDelegate(bool* was_cancel_task_called = nullptr)
      : was_cancel_task_called_(was_cancel_task_called) {
    DCHECK(!was_cancel_task_called_ || !*was_cancel_task_called_);
  }
  ~TestDelegate() override = default;

  bool IsValid() const override { return is_valid_; }
  void CancelTask() override {
    is_valid_ = false;

    if (was_cancel_task_called_)
      *was_cancel_task_called_ = true;
  }

 private:
  // Indicates if this delegate is currently valid.
  bool is_valid_ = true;

  // Indicates if CancelTask() was invoked, if not null. Must outlives |this|.
  raw_ptr<bool> was_cancel_task_called_;
};

}  // namespace

// Tests that a default-constructed handle is invalid.
TEST(DelayedTaskHandleTest, DefaultConstructor) {
  DelayedTaskHandle delayed_task_handle;
  EXPECT_FALSE(delayed_task_handle.IsValid());
}

// Tests that creating a handle with an invalid delegate will DCHECK.
TEST(DelayedTaskHandleTest, RequiresValidDelegateOnConstruction) {
  auto delegate = std::make_unique<TestDelegate>();
  EXPECT_TRUE(delegate->IsValid());

  // Invalidate the delegate before creating the handle.
  delegate->CancelTask();
  EXPECT_FALSE(delegate->IsValid());

  EXPECT_DCHECK_DEATH(
      { DelayedTaskHandle delayed_task_handle(std::move(delegate)); });
}

// Tests that calling CancelTask() on the handle will call CancelTask() on the
// delegate and invalidate it.
TEST(DelayedTaskHandleTest, CancelTask) {
  bool was_cancel_task_called = false;
  auto delegate = std::make_unique<TestDelegate>(&was_cancel_task_called);
  EXPECT_FALSE(was_cancel_task_called);
  EXPECT_TRUE(delegate->IsValid());

  auto* delegate_ptr = delegate.get();
  DelayedTaskHandle delayed_task_handle(std::move(delegate));
  EXPECT_FALSE(was_cancel_task_called);
  EXPECT_TRUE(delegate_ptr->IsValid());
  EXPECT_TRUE(delayed_task_handle.IsValid());

  delayed_task_handle.CancelTask();

  EXPECT_TRUE(was_cancel_task_called);
  EXPECT_FALSE(delayed_task_handle.IsValid());
}

// Tests that calling CancelTask() on a handle with no delegate will no-op.
TEST(DelayedTaskHandleTest, CancelTaskNoDelegate) {
  DelayedTaskHandle delayed_task_handle;
  EXPECT_FALSE(delayed_task_handle.IsValid());

  delayed_task_handle.CancelTask();
  EXPECT_FALSE(delayed_task_handle.IsValid());
}

// Tests that calling CancelTask() on a handle with an invalid delegate doesn't
// crash and keeps the handle invalid.
TEST(DelayedTaskHandleTest, CancelTaskInvalidDelegate) {
  bool was_cancel_task_called = false;
  auto delegate = std::make_unique<TestDelegate>(&was_cancel_task_called);
  EXPECT_FALSE(was_cancel_task_called);
  EXPECT_TRUE(delegate->IsValid());

  auto* delegate_ptr = delegate.get();
  DelayedTaskHandle delayed_task_handle(std::move(delegate));
  EXPECT_FALSE(was_cancel_task_called);
  EXPECT_TRUE(delegate_ptr->IsValid());
  EXPECT_TRUE(delayed_task_handle.IsValid());

  delegate_ptr->CancelTask();

  EXPECT_TRUE(was_cancel_task_called);
  EXPECT_FALSE(delegate_ptr->IsValid());
  EXPECT_FALSE(delayed_task_handle.IsValid());

  // Reset |was_cancel_task_called| to ensure Delegate::CancelTask() is still
  // invoked on an invalid delegate.
  was_cancel_task_called = false;

  delayed_task_handle.CancelTask();

  EXPECT_TRUE(was_cancel_task_called);
  EXPECT_FALSE(delayed_task_handle.IsValid());
}

// Tests that invalidating the delegate will also invalidate the handle.
TEST(DelayedTaskHandleTest, InvalidateDelegate) {
  auto delegate = std::make_unique<TestDelegate>();
  EXPECT_TRUE(delegate->IsValid());

  auto* delegate_ptr = delegate.get();
  DelayedTaskHandle delayed_task_handle(std::move(delegate));
  EXPECT_TRUE(delegate_ptr->IsValid());
  EXPECT_TRUE(delayed_task_handle.IsValid());

  delegate_ptr->CancelTask();

  EXPECT_FALSE(delegate_ptr->IsValid());
  EXPECT_FALSE(delayed_task_handle.IsValid());
}

// Tests that destroying a valid handle will DCHECK.
TEST(DelayedTaskHandleTest, InvalidOnDestuction) {
  auto delegate = std::make_unique<TestDelegate>();
  EXPECT_TRUE(delegate->IsValid());

  auto* delegate_ptr = delegate.get();
  EXPECT_DCHECK_DEATH({
    DelayedTaskHandle delayed_task_handle(std::move(delegate));
    EXPECT_TRUE(delegate_ptr->IsValid());
    EXPECT_TRUE(delayed_task_handle.IsValid());
  });
}

// Tests the move-constructor.
TEST(DelayedTaskHandleTest, MoveConstructor) {
  auto delegate = std::make_unique<TestDelegate>();
  EXPECT_TRUE(delegate->IsValid());

  auto* delegate_ptr = delegate.get();
  DelayedTaskHandle delayed_task_handle(std::move(delegate));
  EXPECT_TRUE(delegate_ptr->IsValid());
  EXPECT_TRUE(delayed_task_handle.IsValid());

  DelayedTaskHandle other_delayed_task_handle(std::move(delayed_task_handle));
  EXPECT_TRUE(delegate_ptr->IsValid());
  EXPECT_TRUE(other_delayed_task_handle.IsValid());

  // Clean-up.
  other_delayed_task_handle.CancelTask();
}

// Tests the move-assignment.
TEST(DelayedTaskHandleTest, MoveAssignment) {
  auto delegate = std::make_unique<TestDelegate>();
  EXPECT_TRUE(delegate->IsValid());

  auto* delegate_ptr = delegate.get();
  DelayedTaskHandle delayed_task_handle(std::move(delegate));
  EXPECT_TRUE(delegate_ptr->IsValid());
  EXPECT_TRUE(delayed_task_handle.IsValid());

  DelayedTaskHandle other_delayed_task_handle;
  EXPECT_FALSE(other_delayed_task_handle.IsValid());

  other_delayed_task_handle = std::move(delayed_task_handle);
  EXPECT_TRUE(delegate_ptr->IsValid());
  EXPECT_TRUE(other_delayed_task_handle.IsValid());

  // Clean-up.
  other_delayed_task_handle.CancelTask();
}

// Tests that assigning to a valid handle will DCHECK.
TEST(DelayedTaskHandleTest, AssignToValidHandle) {
  auto delegate = std::make_unique<TestDelegate>();
  EXPECT_TRUE(delegate->IsValid());

  auto* delegate_ptr = delegate.get();
  DelayedTaskHandle delayed_task_handle(std::move(delegate));
  EXPECT_TRUE(delegate_ptr->IsValid());
  EXPECT_TRUE(delayed_task_handle.IsValid());

  EXPECT_DCHECK_DEATH({ delayed_task_handle = DelayedTaskHandle(); });

  // Clean-up.
  delayed_task_handle.CancelTask();
}

}  // namespace base
