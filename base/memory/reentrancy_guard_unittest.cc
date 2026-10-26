// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2) && \
    (PA_BUILDFLAG(IS_POSIX) || PA_BUILDFLAG(IS_WIN))

#include <sanitizer/allocator_interface.h>

#include <array>
#include <atomic>
#include <memory>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/reentrancy_guard.h"  // nogncheck
#include "base/rand_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::test {

namespace {

std::atomic<size_t> g_malloc_hook_called{0u};
std::atomic<size_t> g_free_hook_called{0u};

typedef void (*malloc_hook_fn_type)(const volatile void*, size_t);
typedef void (*free_hook_fn_type)(const volatile void*);

malloc_hook_fn_type g_malloc_hook_fn = nullptr;
free_hook_fn_type g_free_hook_fn = nullptr;

void MallocHookWithMalloc(const volatile void* ptr, size_t size) {
  ALLOC_REENTRANCY_GUARD();
  // LOG() causes malloc() and free(). If the reentrancy guard
  // doesn't work correctly, this LOG() will cause crash because
  // of stack overflow caused by reentrancy.
  LOG(INFO) << "MallocHook";
  g_malloc_hook_called.fetch_add(1u);
}

void FreeHookWithMalloc(const volatile void* ptr) {
  FREE_REENTRANCY_GUARD();
  // LOG() causes malloc() and free(). If the reentrancy guard
  // doesn't work correctly, this LOG() will cause crash because
  // of stack overflow caused by reentrancy.
  LOG(INFO) << "FreeHook";
  g_free_hook_called.fetch_add(1u);
}

// Since `__sanitizer_install_malloc_and_free_hooks()` doesn't support
// uninstalling malloc and free hooks, use `g_malloc_hook_fn` and
// `g_free_hook_fn` to test ReentrancyGuard. Once the test is done,
// set `g_malloc_hook_fn` and `g_free_hook_fn` `nullptr`. So the
// later tests will not be affected by the installed hooks.
[[maybe_unused]] void MallocHook(const volatile void* ptr, size_t size) {
  if (!g_malloc_hook_fn) {
    return;
  }
  (*g_malloc_hook_fn)(ptr, size);
}

[[maybe_unused]] void FreeHook(const volatile void* ptr) {
  if (!g_free_hook_fn) {
    return;
  }
  (*g_free_hook_fn)(ptr);
}

struct MallocFreeHookScope {
  MallocFreeHookScope() {
    previous_handler_ = logging::GetLogMessageHandler();
    logging::SetLogMessageHandler(&SuppressStandardLogMessageHandler);

    g_malloc_hook_fn = MallocHookWithMalloc;
    g_free_hook_fn = FreeHookWithMalloc;
  }

  ~MallocFreeHookScope() {
    g_malloc_hook_fn = nullptr;
    g_free_hook_fn = nullptr;

    logging::SetLogMessageHandler(previous_handler_);
  }

  static bool SuppressStandardLogMessageHandler(int severity,
                                                const char* file,
                                                int line,
                                                size_t message_start,
                                                const std::string& str) {
    return severity == logging::LOGGING_INFO;
  }

 private:
  logging::LogMessageHandlerFunction previous_handler_;
};

class TestThread : public PlatformThread::Delegate {
 public:
  TestThread()
      : thread_id_(kInvalidThreadId),
        termination_ready_(WaitableEvent::ResetPolicy::MANUAL,
                           WaitableEvent::InitialState::NOT_SIGNALED),
        terminate_thread_(WaitableEvent::ResetPolicy::MANUAL,
                          WaitableEvent::InitialState::NOT_SIGNALED) {}

  ~TestThread() override { EXPECT_TRUE(terminate_thread_.IsSignaled()); }

  TestThread(const TestThread&) = delete;
  TestThread& operator=(const TestThread&) = delete;

  void ThreadMain() override {
    thread_id_ = PlatformThread::CurrentId();
    EXPECT_NE(thread_id_, kInvalidThreadId);
    EXPECT_EQ(thread_id_, PlatformThread::CurrentId());

    constexpr size_t kMaxAllocationSize = 256u;
    constexpr size_t kTestCount = 1024u;
    constexpr size_t kTestArraySize = 32u;
    std::array<void*, kTestArraySize> buffers = {nullptr};

    // Allocate and deallocate memory.
    for (size_t i = 0; i < kTestCount; ++i) {
      size_t random_size = RandGenerator(kMaxAllocationSize);
      size_t random_index = RandGenerator(kTestArraySize);
      if (buffers[random_index]) {
        free(buffers[random_index]);
        buffers[random_index] = nullptr;
      }
      buffers[random_index] = malloc(random_size);
    }

    for (size_t i = 0; i < buffers.size(); ++i) {
      if (buffers[i]) {
        free(buffers[i]);
      }
    }

    termination_ready_.Signal();
    terminate_thread_.Wait();
    done_ = true;
  }

  bool IsRunning() const { return termination_ready_.IsSignaled() && !done_; }

  void WaitForTerminationReady() { termination_ready_.Wait(); }

  void MarkForTermination() { terminate_thread_.Signal(); }

 private:
  PlatformThreadId thread_id_;
  mutable WaitableEvent termination_ready_;
  WaitableEvent terminate_thread_;
  bool done_ = false;
};

// Disable optimization not to make compiler remove `malloc()` and free()`.
NOOPT void RunSimpleTestCase() {
  MallocFreeHookScope scope;
  g_malloc_hook_called = g_free_hook_called = 0u;

  // Without NOOPT, compiler may find the allocated memory is not used, i.e.
  // `free(malloc(32u));` and may remove both malloc() and free() from
  // the binary. But this test really needs the malloc() and free() calls. So
  // disable optimization.
  void* ptr = malloc(32u);
  EXPECT_TRUE(ptr);

  size_t malloc_hook_called = g_malloc_hook_called;
  EXPECT_LT(0u, malloc_hook_called);

  free(ptr);
  size_t free_hook_called = g_free_hook_called;
  EXPECT_LT(0u, free_hook_called);
}

}  // namespace

class ReentrancyGuardTest : public testing::Test {
 public:
  ReentrancyGuardTest() = default;
  ~ReentrancyGuardTest() override = default;

  void SetUp() override {
    CHECK(0 != __sanitizer_install_malloc_and_free_hooks(MallocHook, FreeHook));
  }

  void TearDown() override {
    g_malloc_hook_fn = nullptr;
    g_free_hook_fn = nullptr;

    g_malloc_hook_called = 0u;
    g_free_hook_called = 0u;
  }
};

TEST_F(ReentrancyGuardTest, Basic) {
  internal::ReentrancyState::InitializeKey();
  RunSimpleTestCase();
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

TEST_F(ReentrancyGuardTest, TooManyKeys) {
  internal::ReentrancyState::ResetKeyForTesting();

  constexpr size_t kTooManyKeys = 64u;
  std::array<pthread_key_t, kTooManyKeys> too_many_keys;
  // Before allocating `pthread_key` for the reentrancy state, allocate many
  // `pthread_key` to cause `pthread_setspecific()` used by reentrancy state
  // memory allocation.
  for (size_t i = 0; i < too_many_keys.size(); ++i) {
    CHECK(0 == pthread_key_create(&too_many_keys[i], nullptr));
  }
  internal::ReentrancyState::InitializeKey();

  RunSimpleTestCase();

  for (size_t i = 0; i < too_many_keys.size(); ++i) {
    pthread_key_delete(too_many_keys[i]);
  }
}

#endif  //  BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

TEST_F(ReentrancyGuardTest, MultiThreads) {
  internal::ReentrancyState::InitializeKey();
  constexpr size_t kMaxThreads = 16u;

  std::array<TestThread, kMaxThreads> threads;
  std::array<PlatformThreadHandle, std::size(threads)> handles;

  MallocFreeHookScope scope;

  for (const auto& n : threads) {
    ASSERT_FALSE(n.IsRunning());
  }

  g_malloc_hook_called = 0u;
  g_free_hook_called = 0u;

  for (size_t i = 0; i < threads.size(); ++i) {
    CHECK(PlatformThread::Create(0, &threads[i], &handles[i]));
  }
  for (auto& n : threads) {
    n.WaitForTerminationReady();
  }
  for (auto& n : threads) {
    n.MarkForTermination();
  }

  for (auto& n : handles) {
    PlatformThread::Join(n);
  }
  for (const auto& n : threads) {
    ASSERT_FALSE(n.IsRunning());
  }

  size_t malloc_hook_called = g_malloc_hook_called;
  size_t free_hook_called = g_free_hook_called;
  EXPECT_LT(0u, malloc_hook_called);
  EXPECT_LT(0u, free_hook_called);
}

}  // namespace base::test

#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2) && (PA_BUILDFLAG(IS_POSIX)
        // || PA_BUILDFLAG(IS_WIN))
