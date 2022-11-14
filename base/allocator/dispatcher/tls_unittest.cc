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
  std::unordered_set<void*> stored_object_addresses;
  std::mutex stored_object_addresses_mutex;
  std::condition_variable cv;
  std::mutex jam_threads_mutex;
  std::atomic_uint32_t values_inserted_counter(0);

  for (auto& t : threads) {
    t = std::thread{[&] {
      {
        std::lock_guard<std::mutex> lock(stored_object_addresses_mutex);
        stored_object_addresses.insert(sut.GetThreadLocalData());
      }

      ++values_inserted_counter;

      std::unique_lock<std::mutex> lock(jam_threads_mutex);
      cv.wait(lock,
              [&] { return values_inserted_counter.load() == threads.size(); });
    }};
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  cv.notify_all();

  for (auto& t : threads) {
    t.join();
  }

  EXPECT_EQ(stored_object_addresses.size(), threads.size());
}

TEST_F(BaseThreadLocalStorageTest, VerifyDataIsSameWithinEachThread) {
  auto sut = CreateThreadLocalStorage<DataToStore>();
  using TLSType = decltype(sut);

  std::array<std::thread, 2 * TLSType::ItemsPerChunk> threads;

  for (auto& t : threads) {
    t = std::thread{
        [&] { EXPECT_EQ(sut.GetThreadLocalData(), sut.GetThreadLocalData()); }};
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

  EXPECT_CALL(allocator_mock, AllocateMemory(_))
      .Times(5)
      .WillRepeatedly(
          [](size_t size_in_bytes) { return malloc(size_in_bytes); });

  EXPECT_CALL(allocator_mock, FreeMemoryForTesting(_, _))
      .Times(5)
      .WillRepeatedly([](void* pointer_to_allocated, size_t size_in_bytes) {
        free(pointer_to_allocated);
        return true;
      });

  auto sut =
      CreateThreadLocalStorage<DataToStore>(allocator_mock, tlsSystem_mock);

  std::array<std::thread, 5 * decltype(sut)::ItemsPerChunk> threads;
  std::condition_variable cv;
  std::mutex jam_threads_mutex;
  std::atomic_uint32_t threads_spawned_counter(0);

  for (auto& t : threads) {
    t = std::thread{[&] {
      sut.GetThreadLocalData();
      ++threads_spawned_counter;

      std::unique_lock<std::mutex> lock(jam_threads_mutex);
      cv.wait(lock,
              [&] { return threads_spawned_counter.load() == threads.size(); });
    }};
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  cv.notify_all();

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

  EXPECT_TRUE(sut.Setup(nullptr));
  EXPECT_TRUE(sut.TearDownForTesting());
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
  std::array<std::thread, 10> threads;

  std::condition_variable cv;
  std::mutex jam_threads_mutex;
  std::atomic_bool sut_torn_down{false};

  internal::PThreadTLSSystem sut;
  sut.Setup(nullptr);

  for (auto& t : threads) {
    t = std::thread{[&] {
      int x = 0;
      ASSERT_TRUE(sut.SetThreadSpecificData(&x));

      std::unique_lock<std::mutex> lock(jam_threads_mutex);
      cv.wait(lock, [&]() -> bool { return sut_torn_down; });

      EXPECT_EQ(sut.GetThreadSpecificData(), nullptr);
    }};
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  sut.TearDownForTesting();

  sut_torn_down = true;
  cv.notify_all();

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
#endif
}  // namespace base::allocator::dispatcher

#endif  // USE_LOCAL_TLS_EMULATION()