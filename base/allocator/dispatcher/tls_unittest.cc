// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/dispatcher/tls.h"

#if USE_LOCAL_TLS_EMULATION()
#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <utility>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnNull;

namespace base::allocator::dispatcher {
namespace {
struct DataToStore {
  int data_int = 0;
  float data_float = 0.0;
  size_t data_size_t = 0;
  double data_double = 0.0;
};

struct AllocatorMockBase {
  AllocatorMockBase() {
    ON_CALL(*this, AllocateMemory(_)).WillByDefault([](size_t size_in_bytes) {
      return malloc(size_in_bytes);
    });
    ON_CALL(*this, FreeMemoryForTesting(_, _))
        .WillByDefault([](void* pointer_to_allocated, size_t size_in_bytes) {
          free(pointer_to_allocated);
          return true;
        });
  }

  MOCK_METHOD(void*, AllocateMemory, (size_t size_in_bytes), ());
  MOCK_METHOD(bool,
              FreeMemoryForTesting,
              (void* pointer_to_allocated, size_t size_in_bytes),
              ());
};

struct TLSSystemMockBase {
  TLSSystemMockBase() {
    ON_CALL(*this, Setup(_)).WillByDefault(Return(true));
    ON_CALL(*this, TearDownForTesting()).WillByDefault(Return(true));
    ON_CALL(*this, SetThreadSpecificData(_)).WillByDefault(Return(true));
  }

  MOCK_METHOD(
      bool,
      Setup,
      (internal::OnThreadTerminationFunction thread_termination_function),
      ());
  MOCK_METHOD(bool, TearDownForTesting, (), ());
  MOCK_METHOD(void*, GetThreadSpecificData, (), ());
  MOCK_METHOD(bool, SetThreadSpecificData, (void* data), ());
};

using AllocatorMock = NiceMock<AllocatorMockBase>;
using TLSSystemMock = NiceMock<TLSSystemMockBase>;

template <typename T, typename Allocator, typename TLSSystem>
ThreadLocalStorage<T,
                   std::reference_wrapper<Allocator>,
                   std::reference_wrapper<TLSSystem>,
                   0,
                   true>
CreateThreadLocalStorage(Allocator& allocator, TLSSystem& tlsSystem) {
  return {std::ref(allocator), std::ref(tlsSystem)};
}

template <typename T>
ThreadLocalStorage<T,
                   internal::DefaultAllocator,
                   internal::DefaultTLSSystem,
                   0,
                   true>
CreateThreadLocalStorage() {
  return {};
}

}  // namespace

struct BaseThreadLocalStorageTest : public ::testing::Test {};

TEST_F(BaseThreadLocalStorageTest,
       VerifyDataIsIndependentBetweenDifferentSUTs) {
  auto sut_1 = CreateThreadLocalStorage<DataToStore>();
  auto sut_2 = CreateThreadLocalStorage<DataToStore>();

  EXPECT_NE(sut_1.GetThreadLocalData(), sut_2.GetThreadLocalData());
}

TEST_F(BaseThreadLocalStorageTest, VerifyDistinctEntriesForEachThread) {
  auto sut = CreateThreadLocalStorage<DataToStore>();
  using TLSType = decltype(sut);

  std::array<std::thread, 2 * TLSType::ItemsPerChunk> threads;
  std::mutex thread_worker_mutex;
  std::condition_variable thread_counter_cv;
  std::atomic_uint32_t thread_counter{0};
  std::unordered_set<void*> stored_object_addresses;

  std::mutex threads_can_finish_mutex;
  std::condition_variable threads_can_finish_cv;
  std::atomic_bool threads_can_finish{false};

  for (auto& t : threads) {
    t = std::thread{[&] {
      {
        std::lock_guard<std::mutex> lock(thread_worker_mutex);
        stored_object_addresses.insert(sut.GetThreadLocalData());
        ++thread_counter;
        thread_counter_cv.notify_one();
      }

      {
        std::unique_lock<std::mutex> lock(threads_can_finish_mutex);
        threads_can_finish_cv.wait(lock,
                                   [&] { return threads_can_finish.load(); });
      }
    }};
  }

  {
    std::unique_lock<std::mutex> lock(thread_worker_mutex);
    thread_counter_cv.wait(
        lock, [&] { return thread_counter.load() == threads.size(); });
  }

  {
    std::unique_lock<std::mutex> lock(threads_can_finish_mutex);
    threads_can_finish = true;
    threads_can_finish_cv.notify_all();
  }

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_EQ(stored_object_addresses.size(), threads.size());
}

TEST_F(BaseThreadLocalStorageTest, VerifyEntriesAreReusedForNewThreads) {
  auto sut = CreateThreadLocalStorage<DataToStore>();
  using TLSType = decltype(sut);

  std::unordered_set<void*> stored_object_addresses;

  for (size_t thread_count = 0; thread_count < (2 * TLSType::ItemsPerChunk);
       ++thread_count) {
    auto thread = std::thread{
        [&] { stored_object_addresses.insert(sut.GetThreadLocalData()); }};

    thread.join();
  }

  EXPECT_EQ(stored_object_addresses.size(), 1ul);
}

TEST_F(BaseThreadLocalStorageTest, VerifyDataIsSameWithinEachThread) {
  auto sut = CreateThreadLocalStorage<DataToStore>();
  using TLSType = decltype(sut);

  std::array<std::thread, 2 * TLSType::ItemsPerChunk> threads;

  for (auto& t : threads) {
    t = std::thread{[&] {
      EXPECT_EQ(sut.GetThreadLocalData(), sut.GetThreadLocalData());
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      // Check once again to verify the data doesn't change in the course of a
      // thread's lifetime.
      EXPECT_EQ(sut.GetThreadLocalData(), sut.GetThreadLocalData());
    }};
  }

  for (auto& t : threads) {
    t.join();
  }
}

TEST_F(BaseThreadLocalStorageTest, VerifySetupTeardownSequence) {
  AllocatorMock allocator_mock;
  TLSSystemMock tlsSystem_mock;

  InSequence execution_sequence;

  EXPECT_CALL(allocator_mock, AllocateMemory(_))
      .WillOnce([](size_t size_in_bytes) { return malloc(size_in_bytes); });
  EXPECT_CALL(tlsSystem_mock, Setup(NotNull())).WillOnce(Return(true));
  EXPECT_CALL(tlsSystem_mock, TearDownForTesting()).WillOnce(Return(true));
  EXPECT_CALL(allocator_mock, FreeMemoryForTesting(_, _))
      .WillOnce([](void* pointer_to_allocated, size_t size_in_bytes) {
        free(pointer_to_allocated);
        return true;
      });

  auto sut =
      CreateThreadLocalStorage<DataToStore>(allocator_mock, tlsSystem_mock);
}

TEST_F(BaseThreadLocalStorageTest, VerifyAllocatorIsUsed) {
  AllocatorMock allocator_mock;
  TLSSystemMock tlsSystem_mock;

  EXPECT_CALL(allocator_mock, AllocateMemory(_))
      .WillOnce([](size_t size_in_bytes) { return malloc(size_in_bytes); });

  EXPECT_CALL(allocator_mock, FreeMemoryForTesting(_, _))
      .WillOnce([](void* pointer_to_allocated, size_t size_in_bytes) {
        free(pointer_to_allocated);
        return true;
      });

  auto sut =
      CreateThreadLocalStorage<DataToStore>(allocator_mock, tlsSystem_mock);
}

TEST_F(BaseThreadLocalStorageTest, VerifyAllocatorIsUsedForMultipleChunks) {
  AllocatorMock allocator_mock;
  TLSSystemMock tlsSystem_mock;

  constexpr auto number_of_chunks = 5;

  EXPECT_CALL(allocator_mock, AllocateMemory(_))
      .Times(number_of_chunks)
      .WillRepeatedly(
          [](size_t size_in_bytes) { return malloc(size_in_bytes); });

  EXPECT_CALL(allocator_mock, FreeMemoryForTesting(_, _))
      .Times(number_of_chunks)
      .WillRepeatedly([](void* pointer_to_allocated, size_t size_in_bytes) {
        free(pointer_to_allocated);
        return true;
      });

  auto sut =
      CreateThreadLocalStorage<DataToStore>(allocator_mock, tlsSystem_mock);

  std::array<std::thread, number_of_chunks* decltype(sut)::ItemsPerChunk>
      threads;
  std::mutex thread_worker_mutex;
  std::condition_variable thread_counter_cv;
  std::atomic_uint32_t thread_counter{0};
  std::unordered_set<void*> stored_object_addresses;

  std::mutex threads_can_finish_mutex;
  std::condition_variable threads_can_finish_cv;
  std::atomic_bool threads_can_finish{false};

  for (auto& t : threads) {
    t = std::thread{[&] {
      sut.GetThreadLocalData();

      {
        std::lock_guard<std::mutex> lock(thread_worker_mutex);
        ++thread_counter;
        thread_counter_cv.notify_one();
      }

      {
        std::unique_lock<std::mutex> lock(threads_can_finish_mutex);
        threads_can_finish_cv.wait(lock,
                                   [&] { return threads_can_finish.load(); });
      }
    }};
  }

  {
    std::unique_lock<std::mutex> lock(thread_worker_mutex);
    thread_counter_cv.wait(
        lock, [&] { return thread_counter.load() == threads.size(); });
  }

  {
    std::unique_lock<std::mutex> lock(threads_can_finish_mutex);
    threads_can_finish = true;
    threads_can_finish_cv.notify_all();
  }

  for (auto& t : threads) {
    t.join();
  }
}

TEST_F(BaseThreadLocalStorageTest, VerifyTLSSystemIsUsed) {
  AllocatorMock allocator_mock;
  TLSSystemMock tlsSystem_mock;

  InSequence execution_sequence;

  EXPECT_CALL(tlsSystem_mock, Setup(NotNull())).WillOnce(Return(true));
  EXPECT_CALL(tlsSystem_mock, GetThreadSpecificData())
      .WillOnce(Return(nullptr));
  EXPECT_CALL(tlsSystem_mock, SetThreadSpecificData(NotNull()));
  EXPECT_CALL(tlsSystem_mock, TearDownForTesting())
      .Times(1)
      .WillOnce(Return(true));

  auto sut =
      CreateThreadLocalStorage<DataToStore>(allocator_mock, tlsSystem_mock);

  sut.GetThreadLocalData();
}

#if defined(GTEST_HAS_DEATH_TEST)
struct BaseThreadLocalStorageDeathTest : public ::testing::Test {};

TEST_F(BaseThreadLocalStorageDeathTest, VerifyDeathIfAllocationFails) {
  auto f = [] {
    AllocatorMock allocator_mock;
    TLSSystemMock tlsSystem_mock;

    // Setup all expectations here. If we're setting them up in the parent
    // process, they will fail because the parent doesn't execute any test.
    EXPECT_CALL(allocator_mock, AllocateMemory(_)).WillOnce(ReturnNull());

    CreateThreadLocalStorage<DataToStore>(allocator_mock, tlsSystem_mock);
  };

  EXPECT_DEATH(f(), "");
}

TEST_F(BaseThreadLocalStorageDeathTest, VerifyDeathIfFreeFails) {
  auto f = [] {
    AllocatorMock allocator_mock;
    TLSSystemMock tlsSystem_mock;

    // Setup all expectations here. If we're setting them up in the parent
    // process, they will fail because the parent doesn't execute any test.
    EXPECT_CALL(allocator_mock, FreeMemoryForTesting(_, _))
        .WillOnce([](void* allocated_memory, size_t size_in_bytes) {
          free(allocated_memory);
          return false;
        });

    CreateThreadLocalStorage<DataToStore>(allocator_mock, tlsSystem_mock);
  };

  EXPECT_DEATH(f(), "");
}

TEST_F(BaseThreadLocalStorageDeathTest, VerifyDeathIfTLSSetupFails) {
  auto f = [] {
    AllocatorMock allocator_mock;
    TLSSystemMock tlsSystem_mock;

    // Setup all expectations here. If we're setting them up in the parent
    // process, they will fail because the parent doesn't execute any test.
    EXPECT_CALL(tlsSystem_mock, Setup(_)).WillOnce(Return(false));
    EXPECT_CALL(tlsSystem_mock, GetThreadSpecificData()).Times(0);
    EXPECT_CALL(tlsSystem_mock, SetThreadSpecificData(_)).Times(0);
    EXPECT_CALL(tlsSystem_mock, TearDownForTesting()).Times(0);

    CreateThreadLocalStorage<DataToStore>(allocator_mock, tlsSystem_mock);
  };

  EXPECT_DEATH(f(), "");
}

TEST_F(BaseThreadLocalStorageDeathTest, VerifyDeathIfStoringTLSDataFails) {
  auto f = [] {
    AllocatorMock allocator_mock;
    TLSSystemMock tlsSystem_mock;

    // Setup all expectations here. If we're setting them up in the parent
    // process, they will fail because the parent doesn't execute any test.
    EXPECT_CALL(tlsSystem_mock, SetThreadSpecificData(_))
        .Times(1)
        .WillOnce(Return(false));
    EXPECT_CALL(tlsSystem_mock, TearDownForTesting()).Times(0);

    CreateThreadLocalStorage<DataToStore>(allocator_mock, tlsSystem_mock)
        .GetThreadLocalData();
  };

  EXPECT_DEATH(f(), "");
}

TEST_F(BaseThreadLocalStorageDeathTest, VerifyDeathIfTLSTeardownFails) {
  auto f = [] {
    AllocatorMock allocator_mock;
    TLSSystemMock tlsSystem_mock;

    // Setup all expectations here. If we're setting them up in the parent
    // process, they will fail because the parent doesn't execute any test.
    EXPECT_CALL(tlsSystem_mock, Setup(_)).WillOnce(Return(true));
    EXPECT_CALL(tlsSystem_mock, TearDownForTesting()).WillOnce(Return(false));

    CreateThreadLocalStorage<DataToStore>(allocator_mock, tlsSystem_mock);
  };

  EXPECT_DEATH(f(), "");
}
#endif  // GTEST_HAS_DEATH_TEST

struct BasePThreadTLSSystemTest : public ::testing::Test {
  void SetUp() override { thread_termination_counter = 0; }

 protected:
  static void ThreadTerminationFunction(void*) { ++thread_termination_counter; }

  static std::atomic<size_t> thread_termination_counter;
};

std::atomic<size_t> BasePThreadTLSSystemTest::thread_termination_counter{0};

TEST_F(BasePThreadTLSSystemTest, VerifySetupNTeardownSequence) {
  internal::PThreadTLSSystem sut;

  for (size_t idx = 0; idx < 5; ++idx) {
    EXPECT_TRUE(sut.Setup(nullptr));
    EXPECT_TRUE(sut.TearDownForTesting());
  }
}

TEST_F(BasePThreadTLSSystemTest, VerifyThreadTerminationFunctionIsCalled) {
  std::array<std::thread, 10> threads;

  internal::PThreadTLSSystem sut;
  sut.Setup(&ThreadTerminationFunction);

  for (auto& t : threads) {
    t = std::thread{[&] {
      int x = 0;
      ASSERT_TRUE(sut.SetThreadSpecificData(&x));
    }};
  }

  for (auto& t : threads) {
    t.join();
  }

  sut.TearDownForTesting();

  EXPECT_EQ(threads.size(), thread_termination_counter);
}

TEST_F(BasePThreadTLSSystemTest, VerifyGetWithoutSetReturnsNull) {
  internal::PThreadTLSSystem sut;
  sut.Setup(nullptr);

  EXPECT_EQ(nullptr, sut.GetThreadSpecificData());

  sut.TearDownForTesting();
}

TEST_F(BasePThreadTLSSystemTest, VerifyGetAfterTeardownReturnsNull) {
  internal::PThreadTLSSystem sut;
  sut.Setup(nullptr);
  sut.SetThreadSpecificData(this);
  sut.TearDownForTesting();

  EXPECT_EQ(sut.GetThreadSpecificData(), nullptr);
}

TEST_F(BasePThreadTLSSystemTest, VerifyGetAfterTeardownReturnsNullThreaded) {
  std::array<std::thread, 50> threads;

  std::mutex thread_worker_mutex;
  std::condition_variable thread_counter_cv;
  std::atomic_uint32_t thread_counter{0};

  std::mutex threads_can_finish_mutex;
  std::condition_variable threads_can_finish_cv;
  std::atomic_bool threads_can_finish{false};

  internal::PThreadTLSSystem sut;
  ASSERT_TRUE(sut.Setup(nullptr));

  for (auto& t : threads) {
    t = std::thread{[&] {
      // Set some thread specific data. At this stage retrieving the data must
      // return the pointer that was originally set.
      int x = 0;
      ASSERT_TRUE(sut.SetThreadSpecificData(&x));
      ASSERT_EQ(sut.GetThreadSpecificData(), &x);

      // Notify the main thread that one more test thread has started.
      {
        std::lock_guard<std::mutex> lock(thread_worker_mutex);
        ++thread_counter;
        thread_counter_cv.notify_one();
      }

      // Wait for the main thread to notify about teardown of the sut.
      {
        std::unique_lock<std::mutex> lock(threads_can_finish_mutex);
        threads_can_finish_cv.wait(lock,
                                   [&] { return threads_can_finish.load(); });
      }

      // After teardown, thread local data must be nullptr for all threads.
      EXPECT_EQ(sut.GetThreadSpecificData(), nullptr);
    }};
  }

  // Wait for notification from threads that they started and passed the initial
  // check.
  {
    std::unique_lock<std::mutex> lock(thread_worker_mutex);
    thread_counter_cv.wait(
        lock, [&] { return thread_counter.load() == threads.size(); });
  }

  ASSERT_TRUE(sut.TearDownForTesting());

  // Notify all threads that the subject under test was torn down and they can
  // proceed.
  {
    std::unique_lock<std::mutex> lock(threads_can_finish_mutex);
    threads_can_finish = true;
    threads_can_finish_cv.notify_all();
  }

  for (auto& t : threads) {
    t.join();
  }
}

TEST_F(BasePThreadTLSSystemTest, VerifyGetSetSequence) {
  std::array<std::thread, 50> threads;

  internal::PThreadTLSSystem sut;
  sut.Setup(nullptr);

  for (auto& t : threads) {
    t = std::thread{[&] {
      int x = 0;
      EXPECT_TRUE(sut.SetThreadSpecificData(&x));
      EXPECT_EQ(&x, sut.GetThreadSpecificData());
    }};
  }

  for (auto& t : threads) {
    t.join();
  }

  sut.TearDownForTesting();
}

#if DCHECK_IS_ON()
TEST_F(BasePThreadTLSSystemTest, VerifyGetWithoutSetupReturnsNull) {
  internal::PThreadTLSSystem sut;

  EXPECT_EQ(sut.GetThreadSpecificData(), nullptr);
}

TEST_F(BasePThreadTLSSystemTest, VerifyStoreWithoutSetupFails) {
  internal::PThreadTLSSystem sut;

  EXPECT_FALSE(sut.SetThreadSpecificData(this));
}
#endif

#if defined(GTEST_HAS_DEATH_TEST) && DCHECK_IS_ON()
struct BasePThreadTLSSystemDeathTest : public ::testing::Test {};

TEST_F(BasePThreadTLSSystemDeathTest, VerifyDeathIfSetupTwice) {
  internal::PThreadTLSSystem sut;

  EXPECT_TRUE(sut.Setup(nullptr));
  EXPECT_DEATH(sut.Setup(nullptr), "");
}

TEST_F(BasePThreadTLSSystemDeathTest, VerifyDeathIfTearDownWithoutSetup) {
  internal::PThreadTLSSystem sut;

  EXPECT_DEATH(sut.TearDownForTesting(), "");
}
#endif
}  // namespace base::allocator::dispatcher

#endif  // USE_LOCAL_TLS_EMULATION()