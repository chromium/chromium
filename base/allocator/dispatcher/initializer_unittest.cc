// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/allocator/dispatcher/configuration.h"
#include "base/allocator/dispatcher/initializer.h"
#include "base/allocator/dispatcher/testing/observer_mock.h"
#include "base/allocator/dispatcher/testing/tools.h"

#include <functional>
#include <map>
#include <tuple>

namespace base::allocator::dispatcher {
namespace testing {

// A mock Dispatcher for testing. Since Initializer and Dispatcher rely on
// templating, we can't employ GoogleMocks for mocking. The mock dispatcher
// records the number of invocations of Initialize for a given tuple of
// observers.
struct Dispatcher {
  Dispatcher() = default;

  ~Dispatcher() {
    for (const auto& reset_data : reseter_) {
      reset_data.second();
    }
  }

  template <typename... Observers>
  void Initialize(const std::tuple<Observers*...>& observers) {
    ++total_number_of_inits_;
    ++(GetInitCounterForObservers(observers));
  }

  size_t GetTotalInitCounter() const { return total_number_of_inits_; }

  template <typename... Observers>
  size_t& GetInitCounterForObservers(
      const std::tuple<Observers*...>& observers) {
    static std::map<std::tuple<Observers*...>, size_t>
        observer_init_counter_map;
    reseter_[&observer_init_counter_map] = [] {
      observer_init_counter_map.clear();
    };
    return observer_init_counter_map[observers];
  }

  size_t total_number_of_inits_ = 0;
  std::map<void*, std::function<void()>> reseter_;
};
}  // namespace testing

using testing::ObserverMock;

struct BaseAllocatorDispatcherInitializerTest : public ::testing::Test {};

TEST_F(BaseAllocatorDispatcherInitializerTest, VerifyEmptyInitializer) {
  const auto initializer = CreateInitializer();

  EXPECT_EQ(initializer.GetOptionalObservers(), std::make_tuple());
  EXPECT_EQ(initializer.GetMandatoryObservers(), std::make_tuple());
}

TEST_F(BaseAllocatorDispatcherInitializerTest, VerifySettingOptionalObservers) {
  ObserverMock<int> optional_observer_1;
  ObserverMock<float> optional_observer_2;
  ObserverMock<size_t> optional_observer_3;

  auto initializer_1 = CreateInitializer().SetOptionalObservers(
      &optional_observer_1, &optional_observer_2);
  EXPECT_EQ(initializer_1.GetOptionalObservers(),
            std::make_tuple(&optional_observer_1, &optional_observer_2));
  EXPECT_EQ(initializer_1.GetMandatoryObservers(), std::make_tuple());

  auto initializer_2 = initializer_1.SetOptionalObservers(&optional_observer_3);
  EXPECT_EQ(initializer_2.GetOptionalObservers(),
            std::make_tuple(&optional_observer_3));
  EXPECT_EQ(initializer_2.GetMandatoryObservers(), std::make_tuple());

  auto initializer_3 = initializer_2.SetOptionalObservers();
  EXPECT_EQ(initializer_3.GetOptionalObservers(), std::make_tuple());
  EXPECT_EQ(initializer_3.GetMandatoryObservers(), std::make_tuple());
}

TEST_F(BaseAllocatorDispatcherInitializerTest, VerifyAddingOptionalObservers) {
  ObserverMock<int> optional_observer_1;
  ObserverMock<float> optional_observer_2;
  ObserverMock<size_t> optional_observer_3;

  auto initializer_1 = CreateInitializer().AddOptionalObservers(
      &optional_observer_1, &optional_observer_2);
  EXPECT_EQ(initializer_1.GetOptionalObservers(),
            std::make_tuple(&optional_observer_1, &optional_observer_2));
  EXPECT_EQ(initializer_1.GetMandatoryObservers(), std::make_tuple());

  auto initializer_2 = initializer_1.AddOptionalObservers(&optional_observer_3);
  EXPECT_EQ(initializer_2.GetOptionalObservers(),
            std::make_tuple(&optional_observer_1, &optional_observer_2,
                            &optional_observer_3));
  EXPECT_EQ(initializer_2.GetMandatoryObservers(), std::make_tuple());

  auto initializer_3 = initializer_2.AddOptionalObservers();
  EXPECT_EQ(initializer_3.GetOptionalObservers(),
            std::make_tuple(&optional_observer_1, &optional_observer_2,
                            &optional_observer_3));
  EXPECT_EQ(initializer_3.GetMandatoryObservers(), std::make_tuple());

  auto initializer_4 = initializer_3.SetOptionalObservers();
  EXPECT_EQ(initializer_4.GetOptionalObservers(), std::make_tuple());
  EXPECT_EQ(initializer_4.GetMandatoryObservers(), std::make_tuple());
}

TEST_F(BaseAllocatorDispatcherInitializerTest,
       VerifySettingMandatoryObservers) {
  ObserverMock<int> mandatory_observer_1;
  ObserverMock<float> mandatory_observer_2;
  ObserverMock<size_t> mandatory_observer_3;

  auto initializer_1 = CreateInitializer().SetMandatoryObservers(
      &mandatory_observer_1, &mandatory_observer_2);
  EXPECT_EQ(initializer_1.GetMandatoryObservers(),
            std::make_tuple(&mandatory_observer_1, &mandatory_observer_2));
  EXPECT_EQ(initializer_1.GetOptionalObservers(), std::make_tuple());

  auto initializer_2 =
      initializer_1.SetMandatoryObservers(&mandatory_observer_3);
  EXPECT_EQ(initializer_2.GetMandatoryObservers(),
            std::make_tuple(&mandatory_observer_3));
  EXPECT_EQ(initializer_2.GetOptionalObservers(), std::make_tuple());

  auto initializer_3 = initializer_2.SetMandatoryObservers();
  EXPECT_EQ(initializer_3.GetMandatoryObservers(), std::make_tuple());
  EXPECT_EQ(initializer_3.GetOptionalObservers(), std::make_tuple());
}

TEST_F(BaseAllocatorDispatcherInitializerTest, VerifyAddingMandatoryObservers) {
  ObserverMock<int> mandatory_observer_1;
  ObserverMock<float> mandatory_observer_2;
  ObserverMock<size_t> mandatory_observer_3;

  auto initializer_1 = CreateInitializer().AddMandatoryObservers(
      &mandatory_observer_1, &mandatory_observer_2);
  EXPECT_EQ(initializer_1.GetMandatoryObservers(),
            std::make_tuple(&mandatory_observer_1, &mandatory_observer_2));
  EXPECT_EQ(initializer_1.GetOptionalObservers(), std::make_tuple());

  auto initializer_2 =
      initializer_1.AddMandatoryObservers(&mandatory_observer_3);
  EXPECT_EQ(initializer_2.GetMandatoryObservers(),
            std::make_tuple(&mandatory_observer_1, &mandatory_observer_2,
                            &mandatory_observer_3));
  EXPECT_EQ(initializer_2.GetOptionalObservers(), std::make_tuple());

  auto initializer_3 = initializer_2.AddMandatoryObservers();
  EXPECT_EQ(initializer_3.GetMandatoryObservers(),
            std::make_tuple(&mandatory_observer_1, &mandatory_observer_2,
                            &mandatory_observer_3));
  EXPECT_EQ(initializer_3.GetOptionalObservers(), std::make_tuple());

  auto initializer_4 = initializer_3.SetMandatoryObservers();
  EXPECT_EQ(initializer_4.GetMandatoryObservers(), std::make_tuple());
  EXPECT_EQ(initializer_4.GetOptionalObservers(), std::make_tuple());
}

TEST_F(BaseAllocatorDispatcherInitializerTest, VerifyBasicInitialization) {
  ObserverMock<int> optional_observer_1;
  ObserverMock<float> optional_observer_2;
  ObserverMock<size_t> mandatory_observer_1;
  ObserverMock<double> mandatory_observer_2;

  testing::Dispatcher test_dispatcher;

  CreateInitializer()
      .SetMandatoryObservers(&mandatory_observer_1, &mandatory_observer_2)
      .SetOptionalObservers(&optional_observer_1, &optional_observer_2)
      .DoInitialize(test_dispatcher);

  const auto observer_ptrs =
      std::make_tuple(&mandatory_observer_1, &mandatory_observer_2,
                      &optional_observer_1, &optional_observer_2);

  EXPECT_EQ(1ul, test_dispatcher.GetInitCounterForObservers(observer_ptrs));
}

TEST_F(BaseAllocatorDispatcherInitializerTest,
       VerifyInitializationWithMandatoryNullObservers) {
  ObserverMock<int> optional_observer_1;
  ObserverMock<float> optional_observer_2;
  ObserverMock<size_t> mandatory_observer;
  ObserverMock<double>* mandatory_null_observer = nullptr;

  testing::Dispatcher test_dispatcher;

  CreateInitializer()
      .SetMandatoryObservers(&mandatory_observer, mandatory_null_observer)
      .SetOptionalObservers(&optional_observer_1, &optional_observer_2)
      .DoInitialize(test_dispatcher);

  // For mandatory observers being null we expect them to be passed straight
  // down to the dispatcher, which will then perform a check of ALL observers.
  const auto valid_observer_ptrs =
      std::make_tuple(&mandatory_observer, mandatory_null_observer,
                      &optional_observer_1, &optional_observer_2);

  EXPECT_EQ(1ul, test_dispatcher.GetTotalInitCounter());
  EXPECT_EQ(1ul,
            test_dispatcher.GetInitCounterForObservers(valid_observer_ptrs));
}

TEST_F(BaseAllocatorDispatcherInitializerTest,
       VerifyInitializationWithOptionalNullObservers) {
  ObserverMock<int> optional_observer;
  ObserverMock<float>* optional_null_observer = nullptr;
  ObserverMock<size_t> mandatory_observer_1;
  ObserverMock<double> mandatory_observer_2;

  testing::Dispatcher test_dispatcher;

  CreateInitializer()
      .SetMandatoryObservers(&mandatory_observer_1, &mandatory_observer_2)
      .SetOptionalObservers(&optional_observer, optional_null_observer)
      .DoInitialize(test_dispatcher);

  const auto valid_observer_ptrs = std::make_tuple(
      &mandatory_observer_1, &mandatory_observer_2, &optional_observer);

  EXPECT_EQ(1ul, test_dispatcher.GetTotalInitCounter());
  EXPECT_EQ(1ul,
            test_dispatcher.GetInitCounterForObservers(valid_observer_ptrs));
}

}  // namespace base::allocator::dispatcher
