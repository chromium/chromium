// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/thread_local_storage.h"

#if defined(OS_WIN)
#include <windows.h>
#include <process.h>
#endif

#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/threading/simple_thread.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
// Ignore warnings about ptr->int conversions that we use when
// storing ints into ThreadLocalStorage.
#pragma warning(disable : 4311 4312)
#endif

namespace base {

#if defined(OS_POSIX)

namespace internal {

// This class is friended by ThreadLocalStorage.
class ThreadLocalStorageTestInternal {
 public:
  static bool HasBeenDestroyed() {
    return ThreadLocalStorage::HasBeenDestroyed();
  }
};

}  // namespace internal

#endif  // defined(OS_POSIX)

namespace {

const int kInitialTlsValue = 0x5555;
const int kFinalTlsValue = 0x7777;
// How many times must a destructor be called before we really are done.
const int kNumberDestructorCallRepetitions = 3;

void ThreadLocalStorageCleanup(void* value);

ThreadLocalStorage::Slot& TLSSlot() {
  static NoDestructor<ThreadLocalStorage::Slot> slot(
      &ThreadLocalStorageCleanup);
  return *slot;
}

class ThreadLocalStorageRunner : public DelegateSimpleThread::Delegate {
 public:
  explicit ThreadLocalStorageRunner(int* tls_value_ptr)
      : tls_value_ptr_(tls_value_ptr) {}

  ~ThreadLocalStorageRunner() override = default;

  void Run() override {
    *tls_value_ptr_ = kInitialTlsValue;
    TLSSlot().Set(tls_value_ptr_);

    int* ptr = static_cast<int*>(TLSSlot().Get());
    EXPECT_EQ(ptr, tls_value_ptr_);
    EXPECT_EQ(*ptr, kInitialTlsValue);
    *tls_value_ptr_ = 0;

    ptr = static_cast<int*>(TLSSlot().Get());
    EXPECT_EQ(ptr, tls_value_ptr_);
    EXPECT_EQ(*ptr, 0);

    *ptr = kFinalTlsValue + kNumberDestructorCallRepetitions;
  }

 private:
  int* tls_value_ptr_;
  DISALLOW_COPY_AND_ASSIGN(ThreadLocalStorageRunner);
};


void ThreadLocalStorageCleanup(void *value) {
  int *ptr = reinterpret_cast<int*>(value);
  // Destructors should never be called with a NULL.
  ASSERT_NE(reinterpret_cast<int*>(NULL), ptr);
  if (*ptr == kFinalTlsValue)
    return;  // We've been called enough times.
  ASSERT_LT(kFinalTlsValue, *ptr);
  ASSERT_GE(kFinalTlsValue + kNumberDestructorCallRepetitions, *ptr);
  --*ptr;  // Move closer to our target.
  // Tell tls that we're not done with this thread, and still need destruction.
  TLSSlot().Set(value);
}

#if defined(OS_POSIX)
constexpr intptr_t kDummyValue = 0xABCD;
constexpr size_t kKeyCount = 20;

// The order in which pthread keys are destructed is not specified by the POSIX
// specification. Hopefully, of the 20 keys we create, some of them should be
// destroyed after the TLS key is destroyed.
class UseTLSDuringDestructionRunner {
 public:
  UseTLSDuringDestructionRunner() = default;

  // The order in which pthread_key destructors are called is not well defined.
  // Hopefully, by creating 10 both before and after initializing TLS on the
  // thread, at least 1 will be called after TLS destruction.
  void Run() {
    ASSERT_FALSE(internal::ThreadLocalStorageTestInternal::HasBeenDestroyed());

    // Create 10 pthread keys before initializing TLS on the thread.
    size_t slot_index = 0;
    for (; slot_index < 10; ++slot_index) {
      CreateTlsKeyWithDestructor(slot_index);
    }

    // Initialize the Chrome TLS system. It's possible that base::Thread has
    // already initialized Chrome TLS, but we don't rely on that.
    slot_.Set(reinterpret_cast<void*>(kDummyValue));

    // Create 10 pthread keys after initializing TLS on the thread.
    for (; slot_index < kKeyCount; ++slot_index) {
      CreateTlsKeyWithDestructor(slot_index);
    }
  }

  bool teardown_works_correctly() { return teardown_works_correctly_; }

 private:
  struct TLSState {
    pthread_key_t key;
    bool* teardown_works_correctly;
  };

  // The POSIX TLS destruction API takes as input a single C-function, which is
  // called with the current |value| of a (key, value) pair. We need this
  // function to do two things: set the |value| to nullptr, which requires
  // knowing the associated |key|, and update the |teardown_works_correctly_|
  // state.
  //
  // To accomplish this, we set the value to an instance of TLSState, which
  // contains |key| as well as a pointer to |teardown_works_correctly|.
  static void ThreadLocalDestructor(void* value) {
    TLSState* state = static_cast<TLSState*>(value);
    int result = pthread_setspecific(state->key, nullptr);
    ASSERT_EQ(result, 0);

    // If this path is hit, then the thread local destructor was called after
    // the Chrome-TLS destructor and the internal state was updated correctly.
    // No further checks are necessary.
    if (internal::ThreadLocalStorageTestInternal::HasBeenDestroyed()) {
      *(state->teardown_works_correctly) = true;
      return;
    }

    // If this path is hit, then the thread local destructor was called before
    // the Chrome-TLS destructor is hit. The ThreadLocalStorage::Slot should
    // still function correctly.
    ASSERT_EQ(reinterpret_cast<intptr_t>(slot_.Get()), kDummyValue);
  }

  void CreateTlsKeyWithDestructor(size_t index) {
    ASSERT_LT(index, kKeyCount);

    tls_states_[index].teardown_works_correctly = &teardown_works_correctly_;
    int result = pthread_key_create(
        &(tls_states_[index].key),
        UseTLSDuringDestructionRunner::ThreadLocalDestructor);
    ASSERT_EQ(result, 0);

    result = pthread_setspecific(tls_states_[index].key, &tls_states_[index]);
    ASSERT_EQ(result, 0);
  }

  static base::ThreadLocalStorage::Slot slot_;
  bool teardown_works_correctly_ = false;
  TLSState tls_states_[kKeyCount];

  DISALLOW_COPY_AND_ASSIGN(UseTLSDuringDestructionRunner);
};

base::ThreadLocalStorage::Slot UseTLSDuringDestructionRunner::slot_;

void* UseTLSTestThreadRun(void* input) {
  UseTLSDuringDestructionRunner* runner =
      static_cast<UseTLSDuringDestructionRunner*>(input);
  runner->Run();
  return nullptr;
}

#endif  // defined(OS_POSIX)

}  // namespace

TEST(ThreadLocalStorageTest, Basics) {
  ThreadLocalStorage::Slot slot;
  slot.Set(reinterpret_cast<void*>(123));
  int value = reinterpret_cast<intptr_t>(slot.Get());
  EXPECT_EQ(value, 123);
}

#if defined(THREAD_SANITIZER)
// Do not run the test under ThreadSanitizer. Because this test iterates its
// own TSD destructor for the maximum possible number of times, TSan can't jump
// in after the last destructor invocation, therefore the destructor remains
// unsynchronized with the following users of the same TSD slot. This results
// in race reports between the destructor and functions in other tests.
#define MAYBE_TLSDestructors DISABLED_TLSDestructors
#else
#define MAYBE_TLSDestructors TLSDestructors
#endif
TEST(ThreadLocalStorageTest, MAYBE_TLSDestructors) {
  // Create a TLS index with a destructor.  Create a set of
  // threads that set the TLS, while the destructor cleans it up.
  // After the threads finish, verify that the value is cleaned up.
  const int kNumThreads = 5;
  int values[kNumThreads];
  ThreadLocalStorageRunner* thread_delegates[kNumThreads];
  DelegateSimpleThread* threads[kNumThreads];

  // Spawn the threads.
  for (int index = 0; index < kNumThreads; index++) {
    values[index] = kInitialTlsValue;
    thread_delegates[index] = new ThreadLocalStorageRunner(&values[index]);
    threads[index] = new DelegateSimpleThread(thread_delegates[index],
                                              "tls thread");
    threads[index]->Start();
  }

  // Wait for the threads to finish.
  for (int index = 0; index < kNumThreads; index++) {
    threads[index]->Join();
    delete threads[index];
    delete thread_delegates[index];

    // Verify that the destructor was called and that we reset.
    EXPECT_EQ(values[index], kFinalTlsValue);
  }
}

TEST(ThreadLocalStorageTest, TLSReclaim) {
  // Creates and destroys many TLS slots and ensures they all zero-inited.
  for (int i = 0; i < 1000; ++i) {
    ThreadLocalStorage::Slot slot(nullptr);
    EXPECT_EQ(nullptr, slot.Get());
    slot.Set(reinterpret_cast<void*>(0xBAADF00D));
    EXPECT_EQ(reinterpret_cast<void*>(0xBAADF00D), slot.Get());
  }
}

#if defined(OS_POSIX)
// Unlike POSIX, Windows does not iterate through the OS TLS to cleanup any
// values there. Instead a per-module thread destruction function is called.
// However, it is not possible to perform a check after this point (as the code
// is detached from the thread), so this check remains POSIX only.
TEST(ThreadLocalStorageTest, UseTLSDuringDestruction) {
  UseTLSDuringDestructionRunner runner;
  pthread_t thread;
  int result = pthread_create(&thread, nullptr, UseTLSTestThreadRun, &runner);
  ASSERT_EQ(result, 0);

  result = pthread_join(thread, nullptr);
  ASSERT_EQ(result, 0);

  EXPECT_TRUE(runner.teardown_works_correctly());
}
#endif  // defined(OS_POSIX)

}  // namespace base
