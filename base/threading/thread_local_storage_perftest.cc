// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <memory>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind_test_util.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread_local_storage.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

#if defined(OS_WIN)
#include <windows.h>
#include "base/win/windows_types.h"
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
#include <pthread.h>
#endif

namespace base {
namespace internal {

namespace {

// A thread that waits for the caller to signal an event before proceeding to
// call action.Run().
class TLSThread : public SimpleThread {
 public:
  // Creates a PostingThread that waits on |start_event| before calling
  // action.Run().
  TLSThread(WaitableEvent* start_event,
            base::OnceClosure action,
            base::OnceClosure completion)
      : SimpleThread("TLSThread"),
        start_event_(start_event),
        action_(std::move(action)),
        completion_(std::move(completion)) {
    Start();
  }

  void Run() override {
    start_event_->Wait();
    std::move(action_).Run();
    std::move(completion_).Run();
  }

 private:
  WaitableEvent* const start_event_;
  base::OnceClosure action_;
  base::OnceClosure completion_;

  DISALLOW_COPY_AND_ASSIGN(TLSThread);
};

class ThreadLocalStoragePerfTest : public testing::Test {
 protected:
  ThreadLocalStoragePerfTest() = default;
  ~ThreadLocalStoragePerfTest() override = default;

  template <class Read, class Write>
  void Benchmark(const std::string& trace,
                 Read read,
                 Write write,
                 size_t num_operation,
                 size_t num_threads) {
    write(2);

    BenchmarkImpl("TLS read throughput", trace,
                  base::BindLambdaForTesting([&]() {
                    volatile intptr_t total = 0;
                    for (size_t i = 0; i < num_operation; ++i)
                      total += read();
                  }),
                  num_operation, num_threads);

    BenchmarkImpl("TLS write throughput", trace,
                  base::BindLambdaForTesting([&]() {
                    for (size_t i = 0; i < num_operation; ++i)
                      write(i);
                  }),
                  num_operation, num_threads);

    BenchmarkImpl("TLS read-write throughput", trace,
                  base::BindLambdaForTesting([&]() {
                    for (size_t i = 0; i < num_operation; ++i)
                      write(read() + 1);
                  }),
                  num_operation, num_threads);
  }

  void BenchmarkImpl(const std::string& measurment,
                     const std::string& trace,
                     base::RepeatingClosure action,
                     size_t num_operation,
                     size_t num_threads) {
    WaitableEvent start_thread;
    WaitableEvent complete_thread;

    base::RepeatingClosure done = BarrierClosure(
        num_threads,
        base::BindLambdaForTesting([&]() { complete_thread.Signal(); }));

    std::vector<std::unique_ptr<TLSThread>> threads;
    for (size_t i = 0; i < num_threads; ++i) {
      threads.emplace_back(
          std::make_unique<TLSThread>(&start_thread, action, done));
    }

    TimeTicks operation_start = TimeTicks::Now();
    start_thread.Signal();
    complete_thread.Wait();
    TimeDelta operation_duration = TimeTicks::Now() - operation_start;

    for (auto& thread : threads)
      thread->Join();

    perf_test::PrintResult(measurment, "", trace,
                           num_operation / operation_duration.InMillisecondsF(),
                           "operations/ms", true);
    size_t nanos_per_operation =
        operation_duration.InNanoseconds() / num_operation;
    perf_test::PrintResult(measurment, "", trace, nanos_per_operation,
                           "ns/operation", true);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ThreadLocalStoragePerfTest);
};

}  // namespace

TEST_F(ThreadLocalStoragePerfTest, ThreadLocalStorage) {
  ThreadLocalStorage::Slot tls;
  auto read = [&]() { return reinterpret_cast<intptr_t>(tls.Get()); };
  auto write = [&](intptr_t value) { tls.Set(reinterpret_cast<void*>(value)); };

  Benchmark("ThreadLocalStorage", read, write, 10000000, 1);
  Benchmark("ThreadLocalStorage 4 threads", read, write, 10000000, 4);
}

#if defined(OS_WIN)

void WINAPI destroy(void*) {}

TEST_F(ThreadLocalStoragePerfTest, PlatformFls) {
  DWORD key = FlsAlloc(destroy);
  ASSERT_NE(PlatformThreadLocalStorage::TLS_KEY_OUT_OF_INDEXES, key);

  auto read = [&]() { return reinterpret_cast<intptr_t>(FlsGetValue(key)); };
  auto write = [&](intptr_t value) {
    FlsSetValue(key, reinterpret_cast<void*>(value));
  };

  Benchmark("PlatformFls", read, write, 10000000, 1);
  Benchmark("PlatformFls 4 threads", read, write, 10000000, 4);
}

TEST_F(ThreadLocalStoragePerfTest, PlatformTls) {
  DWORD key = TlsAlloc();
  ASSERT_NE(PlatformThreadLocalStorage::TLS_KEY_OUT_OF_INDEXES, key);

  auto read = [&]() { return reinterpret_cast<intptr_t>(TlsGetValue(key)); };
  auto write = [&](intptr_t value) {
    TlsSetValue(key, reinterpret_cast<void*>(value));
  };

  Benchmark("PlatformTls", read, write, 10000000, 1);
  Benchmark("PlatformTls 4 threads", read, write, 10000000, 4);
}

#elif defined(OS_POSIX) || defined(OS_FUCHSIA)

TEST_F(ThreadLocalStoragePerfTest, PlatformTls) {
  pthread_key_t key;
  ASSERT_FALSE(pthread_key_create(&key, [](void*) {}));
  ASSERT_NE(PlatformThreadLocalStorage::TLS_KEY_OUT_OF_INDEXES, key);

  auto read = [&]() {
    return reinterpret_cast<intptr_t>(pthread_getspecific(key));
  };
  auto write = [&](intptr_t value) {
    pthread_setspecific(key, reinterpret_cast<void*>(value));
  };

  Benchmark("PlatformTls", read, write, 10000000, 1);
  Benchmark("PlatformTls 4 threads", read, write, 10000000, 4);
}

#endif

TEST_F(ThreadLocalStoragePerfTest, Cpp11Tls) {
  thread_local intptr_t thread_local_variable;

  auto read = [&]() { return thread_local_variable; };
  auto write = [&](intptr_t value) {
    reinterpret_cast<volatile intptr_t*>(&thread_local_variable)[0] = value;
  };

  Benchmark("C++ thread_local TLS", read, write, 10000000, 1);
  Benchmark("C++ thread_local TLS 4 threads", read, write, 10000000, 4);
}

}  // namespace internal
}  // namespace base
