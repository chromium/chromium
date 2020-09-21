// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/shared_vector.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

template <typename VectorType>
class VectorTester {
 public:
  // FindType must be copyable.
  template <typename FindType>
  class Finder {
   public:
    explicit Finder(const FindType& item)
        : sought_item_(item), run_loop_(std::make_unique<base::RunLoop>()) {}

    bool Compare(const FindType& item) const { return sought_item_ == item; }

    void OnFound(FindType& item) {
      found_result_ = item;
      run_loop_->Quit();
    }

    void OnNotFound() { run_loop_->Quit(); }

    const FindType& sought_item() const { return sought_item_; }

    const base::Optional<FindType>& found_result() const {
      return found_result_;
    }

    void Wait() {
      run_loop_->Run();
      run_loop_ = std::make_unique<base::RunLoop>();
    }

   private:
    const FindType sought_item_;
    std::unique_ptr<base::RunLoop> run_loop_;

    base::Optional<FindType> found_result_;
  };

  template <typename ExecuteType>
  class Executor {
   public:
    explicit Executor(size_t expected_value_count)
        : expected_value_count_(expected_value_count),
          run_loop_(std::make_unique<base::RunLoop>()) {}

    void CountValue(ExecuteType& item) { found_count_++; }

    void Complete() { run_loop_->Quit(); }

    void Wait() {
      run_loop_->Run();
      run_loop_ = std::make_unique<base::RunLoop>();
    }

    size_t DifferenceInCount() const {
      return expected_value_count_ - found_count_;
    }

    size_t found_count() const { return found_count_; }

   private:
    const size_t expected_value_count_;
    std::unique_ptr<base::RunLoop> run_loop_;
    size_t found_count_{0};
  };

  VectorTester()
      : vector_(SharedVector<VectorType>::Create()),
        sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})),
        run_loop_(std::make_unique<base::RunLoop>()) {}

  ~VectorTester() = default;

  // Find only works on copyable items - so VectorType must be copyable.
  Finder<VectorType> GetFinder(VectorType sought_item) {
    return Finder<VectorType>(sought_item);
  }

  Executor<VectorType> GetExecutor(size_t expected_value_count) {
    return Executor<VectorType>(expected_value_count);
  }

  void PushBack(VectorType item) {
    vector_->PushBack(
        std::move(item),
        base::BindOnce(&VectorTester<VectorType>::OnPushBackComplete,
                       base::Unretained(this)));
  }

  // Resets |insert_success| before returning its value.
  base::Optional<bool> GetPushBackSuccess() {
    base::Optional<bool> return_value;
    return_value.swap(insert_success_);
    return return_value;
  }

  void Erase(VectorType value) {
    auto predicate_cb = base::BindRepeating(
        [](const VectorType& expected_value, const VectorType& comparison_value)
            -> bool { return expected_value == comparison_value; },
        value);
    vector_->Erase(std::move(predicate_cb),
                   base::BindOnce(&VectorTester<VectorType>::OnEraseComplete,
                                  base::Unretained(this)));
  }

  void Erase(base::RepeatingCallback<bool(const VectorType&)> predicate_cb) {
    vector_->Erase(std::move(predicate_cb),
                   base::BindOnce(&VectorTester<VectorType>::OnEraseComplete,
                                  base::Unretained(this)));
  }

  base::Optional<uint64_t> GetEraseValue() {
    base::Optional<uint64_t> return_value;
    return_value.swap(number_deleted_);
    return return_value;
  }

  void ExecuteIfFound(Finder<VectorType>* finder) {
    vector_->ExecuteIfFound(
        base::BindRepeating(&Finder<VectorType>::Compare,
                            base::Unretained(finder)),
        base::BindOnce(&Finder<VectorType>::OnFound, base::Unretained(finder)),
        base::BindOnce(&Finder<VectorType>::OnNotFound,
                       base::Unretained(finder)));
  }

  void ExecuteOnEachElement(Executor<VectorType>* executor) {
    vector_->ExecuteOnEachElement(
        base::BindRepeating(&Executor<VectorType>::CountValue,
                            base::Unretained(executor)),
        base::BindOnce(&Executor<VectorType>::Complete,
                       base::Unretained(executor)));
  }

  void Wait() {
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

 private:
  void OnPushBackComplete() {
    sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VectorTester<VectorType>::VectorPushBackSuccess,
                       base::Unretained(this)));
  }

  void VectorPushBackSuccess() {
    insert_success_ = true;
    Signal();
  }

  void OnEraseComplete(size_t number_deleted) {
    sequenced_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VectorTester<VectorType>::VectorEraseValue,
                                  base::Unretained(this), number_deleted));
  }

  void VectorEraseValue(uint64_t number_deleted) {
    number_deleted_ = number_deleted;
    Signal();
  }

  void Signal() { run_loop_->Quit(); }

  scoped_refptr<SharedVector<VectorType>> vector_;
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  std::unique_ptr<base::RunLoop> run_loop_;

  base::Optional<bool> insert_success_;
  base::Optional<uint64_t> number_deleted_;
};

// Ensures that the vector accept values, and will erase inserted values.
TEST(SharedVectorTest, PushBackAndEraseWorkCorrectly) {
  base::test::TaskEnvironment task_envrionment;

  const std::vector<int> kValues = {1, 2, 3, 4, 5};
  const int kInsertLoopTimes = 10;

  VectorTester<int> vector_tester;

  // PushBack Values
  for (auto value : kValues) {
    vector_tester.PushBack(value);
    vector_tester.Wait();
    auto insert_success = vector_tester.GetPushBackSuccess();
    ASSERT_TRUE(insert_success.has_value());
    EXPECT_TRUE(insert_success.value());
  }

  // Attempt to erase inserted values - should find one each.
  for (auto value : kValues) {
    vector_tester.Erase(value);
    vector_tester.Wait();
    auto erase_success = vector_tester.GetEraseValue();
    ASSERT_TRUE(erase_success.has_value());
    EXPECT_EQ(erase_success.value(), uint64_t(1));
  }

  // Attempt to erase the values again - shouldn't find any.
  for (auto value : kValues) {
    vector_tester.Erase(value);
    vector_tester.Wait();
    auto erase_success = vector_tester.GetEraseValue();
    ASSERT_TRUE(erase_success.has_value());
    EXPECT_EQ(erase_success.value(), uint64_t(0));
  }

  // Attempt to insert the values multiple times - should succeed.
  for (int i = 0; i < kInsertLoopTimes; i++) {
    for (auto value : kValues) {
      vector_tester.PushBack(value);
      vector_tester.Wait();
      auto insert_success = vector_tester.GetPushBackSuccess();
      ASSERT_TRUE(insert_success.has_value());
      EXPECT_TRUE(insert_success.value());
    }
  }

  // Attempt to erase inserted values - should find kInsertLoopTimes each.
  for (auto value : kValues) {
    vector_tester.Erase(value);
    vector_tester.Wait();
    auto erase_success = vector_tester.GetEraseValue();
    ASSERT_TRUE(erase_success.has_value());
    EXPECT_EQ(erase_success.value(), uint64_t(kInsertLoopTimes));
  }
}

// Ensures that SharedVector::ExecuteIfFound works correctly
TEST(SharedVectorTest, ExecuteIfFoundSucceeds) {
  base::test::TaskEnvironment task_envrionment;

  const int kExpectedValue = 1701;
  const int kUnexpectedValue = 42;

  VectorTester<int> vector_tester;
  vector_tester.PushBack(kExpectedValue);
  vector_tester.Wait();

  auto expected_finder = vector_tester.GetFinder(kExpectedValue);
  vector_tester.ExecuteIfFound(&expected_finder);
  expected_finder.Wait();
  auto found_result = expected_finder.found_result();
  ASSERT_TRUE(found_result.has_value());
  EXPECT_EQ(found_result.value(), kExpectedValue);

  auto unexpected_finder = vector_tester.GetFinder(kUnexpectedValue);
  vector_tester.ExecuteIfFound(&unexpected_finder);
  unexpected_finder.Wait();
  found_result = unexpected_finder.found_result();
  EXPECT_FALSE(found_result.has_value());
}

TEST(SharedVectorTest, ExecuteAllElements) {
  base::test::TaskEnvironment task_envrionment;

  const std::vector<int> kValues = {1, 2, 3, 4, 5};

  VectorTester<int> vector_tester;

  // PushBack Values
  for (auto value : kValues) {
    vector_tester.PushBack(value);
    vector_tester.Wait();
    auto insert_success = vector_tester.GetPushBackSuccess();
    ASSERT_TRUE(insert_success.has_value());
    EXPECT_TRUE(insert_success.value());
  }

  auto executor = vector_tester.GetExecutor(kValues.size());
  vector_tester.ExecuteOnEachElement(&executor);
  executor.Wait();
  EXPECT_EQ(executor.DifferenceInCount(), 0u);
}

// Ensures that execution can happen on elements that are moveable but not
// copyable.
TEST(SharedVectorTest, InsertAndExecuteAndEraseOnUniquePtr) {
  base::test::TaskEnvironment task_envrionment;

  const std::vector<int> kValues = {1, 2, 3, 4, 5};

  VectorTester<std::unique_ptr<int>> vector_tester;

  for (auto value : kValues) {
    vector_tester.PushBack(std::make_unique<int>(value));
    vector_tester.Wait();
    auto insert_success = vector_tester.GetPushBackSuccess();
    ASSERT_TRUE(insert_success.has_value());
    EXPECT_TRUE(insert_success.value());
  }

  auto executor = vector_tester.GetExecutor(kValues.size());
  vector_tester.ExecuteOnEachElement(&executor);
  executor.Wait();
  EXPECT_EQ(executor.DifferenceInCount(), 0u);

  for (auto value : kValues) {
    auto comparator_cb = base::BindRepeating(
        [](int expected_value, const std::unique_ptr<int>& comparison_value)
            -> bool { return expected_value == *comparison_value; },
        value);
    vector_tester.Erase(comparator_cb);
    vector_tester.Wait();
    auto erase_success = vector_tester.GetEraseValue();
    ASSERT_TRUE(erase_success.has_value());
    EXPECT_EQ(erase_success.value(), uint64_t(1));
  }
}

}  // namespace
}  // namespace reporting
