// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/bind_post_task.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/sequence_checker_impl.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

void SetBool(bool* variable, bool value) {
  *variable = value;
}

void SetInt(int* variable, int value) {
  *variable = value;
}

void SetIntFromUniquePtr(int* variable, std::unique_ptr<int> value) {
  *variable = *value;
}

int Multiply(int value) {
  return value * 5;
}

void ClearReference(OnceClosure callback) {}

class SequenceRestrictionChecker {
 public:
  explicit SequenceRestrictionChecker(bool& set_on_destroy)
      : set_on_destroy_(set_on_destroy) {}

  ~SequenceRestrictionChecker() {
    EXPECT_TRUE(checker_.CalledOnValidSequence());
    *set_on_destroy_ = true;
  }

  void Run() { EXPECT_TRUE(checker_.CalledOnValidSequence()); }

 private:
  SequenceCheckerImpl checker_;
  const raw_ref<bool> set_on_destroy_;
};

}  // namespace

class BindPostTaskTest : public testing::Test {
 protected:
  test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<SequencedTaskRunner> task_runner_ =
      SequencedTaskRunner::GetCurrentDefault();
};

TEST_F(BindPostTaskTest, OnceClosure) {
  bool val = false;
  OnceClosure cb = BindOnce(&SetBool, &val, true);
  OnceClosure post_cb = BindPostTask(task_runner_, std::move(cb));

  std::move(post_cb).Run();
  EXPECT_FALSE(val);

  RunLoop().RunUntilIdle();
  EXPECT_TRUE(val);
}

TEST_F(BindPostTaskTest, OnceCallback) {
  OnceCallback<void(bool*, bool)> cb = BindOnce(&SetBool);
  OnceCallback<void(bool*, bool)> post_cb =
      BindPostTask(task_runner_, std::move(cb));

  bool val = false;
  std::move(post_cb).Run(&val, true);
  EXPECT_FALSE(val);

  RunLoop().RunUntilIdle();
  EXPECT_TRUE(val);
}

TEST_F(BindPostTaskTest, OnceWithBoundMoveOnlyArg) {
  int val = 0;
  OnceClosure cb =
      BindOnce(&SetIntFromUniquePtr, &val, std::make_unique<int>(10));
  OnceClosure post_cb = BindPostTask(task_runner_, std::move(cb));

  std::move(post_cb).Run();
  EXPECT_EQ(val, 0);

  RunLoop().RunUntilIdle();
  EXPECT_EQ(val, 10);
}

TEST_F(BindPostTaskTest, OnceWithUnboundMoveOnlyArg) {
  int val = 0;
  OnceCallback<void(std::unique_ptr<int>)> cb =
      BindOnce(&SetIntFromUniquePtr, &val);
  OnceCallback<void(std::unique_ptr<int>)> post_cb =
      BindPostTask(task_runner_, std::move(cb));

  std::move(post_cb).Run(std::make_unique<int>(10));
  EXPECT_EQ(val, 0);

  RunLoop().RunUntilIdle();
  EXPECT_EQ(val, 10);
}

TEST_F(BindPostTaskTest, OnceWithIgnoreResult) {
  OnceCallback<void(int)> post_cb =
      BindPostTask(task_runner_, BindOnce(IgnoreResult(&Multiply)));
  std::move(post_cb).Run(1);
  RunLoop().RunUntilIdle();
}

TEST_F(BindPostTaskTest, OnceThen) {
  int value = 0;

  // Multiply() returns an int and SetInt() takes an int as a parameter.
  OnceClosure then_cb =
      BindOnce(&Multiply, 5)
          .Then(BindPostTask(task_runner_, BindOnce(&SetInt, &value)));

  std::move(then_cb).Run();
  EXPECT_EQ(value, 0);
  RunLoop().RunUntilIdle();
  EXPECT_EQ(value, 25);
}

// Ensure that the input callback is run/destroyed on the correct thread even if
// the callback returned from BindPostTask() is run on a different thread.
TEST_F(BindPostTaskTest, OnceRunDestroyedOnBound) {
  Thread target_thread("testing");
  ASSERT_TRUE(target_thread.Start());

  // SequenceRestrictionChecker checks it's creation, Run() and deletion all
  // happen on the main thread.
  bool destroyed = false;
  auto checker = std::make_unique<SequenceRestrictionChecker>(destroyed);

  // `checker` is owned by `cb` which is wrapped in `post_cb`. `post_cb` is run
  // on a different thread which triggers a PostTask() back to the test main
  // thread to invoke `cb` which runs SequenceRestrictionChecker::Run(). After
  // `cb` has been invoked `checker` is destroyed along with the BindState.
  OnceClosure cb =
      BindOnce(&SequenceRestrictionChecker::Run, std::move(checker));
  OnceClosure post_cb = BindPostTask(task_runner_, std::move(cb));
  target_thread.task_runner()->PostTask(FROM_HERE, std::move(post_cb));

  target_thread.FlushForTesting();
  EXPECT_FALSE(destroyed);
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(destroyed);
}

// Ensure that the input callback is destroyed on the correct thread even if the
// callback returned from BindPostTask() is destroyed without being run on a
// different thread.
TEST_F(BindPostTaskTest, OnceNotRunDestroyedOnBound) {
  Thread target_thread("testing");
  ASSERT_TRUE(target_thread.Start());

  // SequenceRestrictionChecker checks it's creation and deletion all happen on
  // the test main thread.
  bool destroyed = false;
  auto checker = std::make_unique<SequenceRestrictionChecker>(destroyed);

  // `checker` is owned by `cb` which is wrapped in `post_cb`. `post_cb` is
  // deleted on a different thread which triggers a PostTask() back to the test
  // main thread to destroy `cb` and `checker`.
  OnceClosure cb =
      BindOnce(&SequenceRestrictionChecker::Run, std::move(checker));
  OnceClosure post_cb = BindPostTask(task_runner_, std::move(cb));
  target_thread.task_runner()->PostTask(
      FROM_HERE, BindOnce(&ClearReference, std::move(post_cb)));

  target_thread.FlushForTesting();
  EXPECT_FALSE(destroyed);
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(destroyed);
}

TEST_F(BindPostTaskTest, RepeatingClosure) {
  bool val = false;
  RepeatingClosure cb = BindRepeating(&SetBool, &val, true);
  RepeatingClosure post_cb = BindPostTask(task_runner_, std::move(cb));

  post_cb.Run();
  EXPECT_FALSE(val);

  RunLoop().RunUntilIdle();
  EXPECT_TRUE(val);

  val = false;
  post_cb.Run();
  EXPECT_FALSE(val);

  RunLoop().RunUntilIdle();
  EXPECT_TRUE(val);
}

TEST_F(BindPostTaskTest, RepeatingCallback) {
  RepeatingCallback<void(bool*, bool)> cb = BindRepeating(&SetBool);
  RepeatingCallback<void(bool*, bool)> post_cb =
      BindPostTask(task_runner_, std::move(cb));

  bool val = false;
  post_cb.Run(&val, true);
  EXPECT_FALSE(val);

  RunLoop().RunUntilIdle();
  EXPECT_TRUE(val);

  post_cb.Run(&val, false);
  EXPECT_TRUE(val);

  RunLoop().RunUntilIdle();
  EXPECT_FALSE(val);
}

TEST_F(BindPostTaskTest, RepeatingWithUnboundMoveOnlyArg) {
  int val = 0;
  RepeatingCallback<void(std::unique_ptr<int>)> cb =
      BindRepeating(&SetIntFromUniquePtr, &val);
  RepeatingCallback<void(std::unique_ptr<int>)> post_cb =
      BindPostTask(task_runner_, std::move(cb));

  post_cb.Run(std::make_unique<int>(10));
  EXPECT_EQ(val, 0);

  RunLoop().RunUntilIdle();
  EXPECT_EQ(val, 10);

  post_cb.Run(std::make_unique<int>(20));
  EXPECT_EQ(val, 10);

  RunLoop().RunUntilIdle();
  EXPECT_EQ(val, 20);
}

TEST_F(BindPostTaskTest, RepeatingWithIgnoreResult) {
  RepeatingCallback<void(int)> post_cb =
      BindPostTask(task_runner_, BindRepeating(IgnoreResult(&Multiply)));
  std::move(post_cb).Run(1);
  RunLoop().RunUntilIdle();
}

TEST_F(BindPostTaskTest, RepeatingThen) {
  int value = 0;

  // Multiply() returns an int and SetInt() takes an int as a parameter.
  RepeatingCallback<void(int)> then_cb = BindRepeating(&Multiply).Then(
      BindPostTask(task_runner_, BindRepeating(&SetInt, &value)));

  then_cb.Run(5);
  EXPECT_EQ(value, 0);
  RunLoop().RunUntilIdle();
  EXPECT_EQ(value, 25);

  then_cb.Run(10);
  EXPECT_EQ(value, 25);
  RunLoop().RunUntilIdle();
  EXPECT_EQ(value, 50);
}

// Ensure that the input callback is run/destroyed on the correct thread even if
// the callback returned from BindPostTask() is run on a different thread.
TEST_F(BindPostTaskTest, RepeatingRunDestroyedOnBound) {
  Thread target_thread("testing");
  ASSERT_TRUE(target_thread.Start());

  // SequenceRestrictionChecker checks it's creation, Run() and deletion all
  // happen on the main thread.
  bool destroyed = false;
  auto checker = std::make_unique<SequenceRestrictionChecker>(destroyed);

  // `checker` is owned by `cb` which is wrapped in `post_cb`. `post_cb` is run
  // on a different thread which triggers a PostTask() back to the test main
  // thread to invoke `cb` which runs SequenceRestrictionChecker::Run(). After
  // `cb` has been invoked `checker` is destroyed along with the BindState.
  RepeatingClosure cb =
      BindRepeating(&SequenceRestrictionChecker::Run, std::move(checker));
  RepeatingClosure post_cb = BindPostTask(task_runner_, std::move(cb));
  target_thread.task_runner()->PostTask(FROM_HERE, std::move(post_cb));

  target_thread.FlushForTesting();
  EXPECT_FALSE(destroyed);
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(destroyed);
}

// Ensure that the input callback is destroyed on the correct thread even if the
// callback returned from BindPostTask() is destroyed without being run on a
// different thread.
TEST_F(BindPostTaskTest, RepeatingNotRunDestroyedOnBound) {
  Thread target_thread("testing");
  ASSERT_TRUE(target_thread.Start());

  // SequenceRestrictionChecker checks it's creation and deletion all happen on
  // the test main thread.
  bool destroyed = false;
  auto checker = std::make_unique<SequenceRestrictionChecker>(destroyed);

  // `checker` is owned by `cb` which is wrapped in `post_cb`. `post_cb` is
  // deleted on a different thread which triggers a PostTask() back to the test
  // main thread to destroy `cb` and `checker`.
  RepeatingClosure cb =
      BindRepeating(&SequenceRestrictionChecker::Run, std::move(checker));
  RepeatingClosure post_cb = BindPostTask(task_runner_, std::move(cb));
  target_thread.task_runner()->PostTask(
      FROM_HERE, BindRepeating(&ClearReference, std::move(post_cb)));

  target_thread.FlushForTesting();
  EXPECT_FALSE(destroyed);
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(destroyed);
}

}  // namespace base
