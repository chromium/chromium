// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is a framework to measure the memory overhead of different containers.
// Under the hood, it works by logging allocations and frees using an allocator
// hook.
//
// Since the free callback does not report a size, and the allocator hooks run
// in the middle of allocation, the logger simply takes the simplest approach
// and logs out the raw data, relying on analyze_containers_memory_usage.py to
// turn the raw output into useful numbers.
//
// The output of consists of m (number of different key/value combinations being
// tested) x n (number of different map types being tested) sections:
//
// <key type 1> -> <value type 1>
// ===== <map type 1> =====
// iteration 0
// alloc <address 1> size <size 1>
// iteration 1
// alloc <address 2> size <size 2>
// free <address 1>
// iteration 2
// alloc <address 3> size <size 3>
// free <address 2>
// ...
// ...
// ...
// ===== <map type n>
// iteration 0
// alloc <address 1000> size <size 1000>
// iteration 1
// alloc <address 1001> size <size 1001>
// free <address 1000>
// iteration 2
// alloc <address 1002> size <size 1002>
// free <address 1001>
// ...
// ...
// ...
// <key type m> -> <value type m>
// ===== <map type 1> =====
// ...
// ...
// ===== <map type n> =====
//
// Alternate output strategies are possible, but most of them are worse/more
// complex, and do not eliminate the postprocessing step.

#include <array>
#include <atomic>
#include <charconv>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include "base/allocator/dispatcher/dispatcher.h"
#include "base/allocator/dispatcher/notification_data.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/strings/safe_sprintf.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/container/btree_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/node_hash_map.h"

namespace {

std::atomic<bool> log_allocs_and_frees;

struct AllocationLogger {
 public:
  void OnAllocation(
      const base::allocator::dispatcher::AllocationNotificationData&
          allocation_data) {
    if (log_allocs_and_frees.load(std::memory_order_acquire)) {
      char buffer[128];
      // Assume success; ignore return value.
      base::strings::SafeSPrintf(buffer, "alloc address %p size %d\n",
                                 allocation_data.address(),
                                 allocation_data.size());
      RAW_LOG(INFO, buffer);
    }
  }

  void OnFree(
      const base::allocator::dispatcher::FreeNotificationData& free_data) {
    if (log_allocs_and_frees.load(std::memory_order_acquire)) {
      char buffer[128];
      // Assume success; ignore return value.
      base::strings::SafeSPrintf(buffer, "freed address %p\n",
                                 free_data.address());
      RAW_LOG(INFO, buffer);
    }
  }

  static void Install() {
    static AllocationLogger logger;
    base::allocator::dispatcher::Dispatcher::GetInstance().InitializeForTesting(
        &logger);
  }
};

class ScopedLogAllocAndFree {
 public:
  ScopedLogAllocAndFree() {
    log_allocs_and_frees.store(true, std::memory_order_release);
  }

  ~ScopedLogAllocAndFree() {
    log_allocs_and_frees.store(false, std::memory_order_release);
  }
};

// Measures the memory usage for a container with type `Container` from 0 to
// 6857 elements, using `inserter` to insert a single element at a time.
// `inserter` should be a functor that takes a `Container& container` as its
// first parameter and a `size_t current_index` as its second parameter.
//
// Note that `inserter` can't use `base::FunctionRef` since the inserter is
// passed through several layers before actually being instantiated below in
// this function.
template <typename Container, typename Inserter>
void MeasureOneContainer(const Inserter& inserter) {
  char buffer[128];

  RAW_LOG(INFO, "iteration 0");
  // Record any initial allocations made by an empty container.
  std::optional<ScopedLogAllocAndFree> base_size_logger;
  base_size_logger.emplace();
  Container c;
  base_size_logger.reset();
  // As a hack, also log out sizeof(c) since the initial base size of the
  // container should be counted too. The exact placeholder used for the address
  // (in this case "(stack)") isn't important as long as it will not have a
  // corresponding free line logged for it.
  base::strings::SafeSPrintf(buffer, "alloc address (stack) size %d",
                             sizeof(c));
  RAW_LOG(INFO, buffer);

  // Swisstables resizes the backing store around 6858 elements.
  for (size_t i = 1; i <= 6857; ++i) {
    base::strings::SafeSPrintf(buffer, "iteration %d", i);
    RAW_LOG(INFO, buffer);
    inserter(c, i);
  }
}

// Measures the memory usage for all the container types under test. `inserter`
// is used to insert a single element at a time into the tested container.
template <typename K, typename V, typename Inserter>
void Measure(const Inserter& inserter) {
  using Hasher = std::conditional_t<std::is_same_v<base::UnguessableToken, K>,
                                    base::UnguessableTokenHash, std::hash<K>>;

  RAW_LOG(INFO, "===== base::flat_map =====");
  MeasureOneContainer<base::flat_map<K, V>>(inserter);
  RAW_LOG(INFO, "===== std::map =====");
  MeasureOneContainer<std::map<K, V>>(inserter);
  RAW_LOG(INFO, "===== std::unordered_map =====");
  MeasureOneContainer<std::unordered_map<K, V, Hasher>>(inserter);
  RAW_LOG(INFO, "===== absl::btree_map =====");
  MeasureOneContainer<absl::btree_map<K, V>>(inserter);
  RAW_LOG(INFO, "===== absl::flat_hash_map =====");
  MeasureOneContainer<absl::flat_hash_map<K, V, Hasher>>(inserter);
  RAW_LOG(INFO, "===== absl::node_hash_map =====");
  MeasureOneContainer<absl::node_hash_map<K, V, Hasher>>(inserter);
}

}  // namespace

int main() {
  AllocationLogger::Install();

  RAW_LOG(INFO, "int -> int");
  Measure<int, int>([](auto& container, size_t i) {
    ScopedLogAllocAndFree scoped_logging;
    container.insert({i, 0});
  });
  RAW_LOG(INFO, "int -> void*");
  Measure<int, void*>([](auto& container, size_t i) {
    ScopedLogAllocAndFree scoped_logging;
    container.insert({i, nullptr});
  });
  RAW_LOG(INFO, "int -> std::string");
  Measure<int, std::string>([](auto& container, size_t i) {
    ScopedLogAllocAndFree scoped_logging;
    container.insert({i, ""});
  });
  RAW_LOG(INFO, "size_t -> int");
  Measure<size_t, int>([](auto& container, size_t i) {
    ScopedLogAllocAndFree scoped_logging;
    container.insert({i, 0});
  });
  RAW_LOG(INFO, "size_t -> void*");
  Measure<size_t, void*>([](auto& container, size_t i) {
    ScopedLogAllocAndFree scoped_logging;
    container.insert({i, nullptr});
  });
  RAW_LOG(INFO, "size_t -> std::string");
  Measure<size_t, std::string>([](auto& container, size_t i) {
    ScopedLogAllocAndFree scoped_logging;
    container.insert({i, ""});
  });
  RAW_LOG(INFO, "std::string -> std::string");
  Measure<std::string, std::string>([](auto& container, size_t i) {
    std::string key;
    key.resize(std::numeric_limits<size_t>::digits10 + 1);
    auto result = std::to_chars(&key.front(), &key.back(), i);
    key.resize(result.ptr - &key.front());
    ScopedLogAllocAndFree scoped_logging;
    container.insert({key, ""});
  });
  RAW_LOG(INFO, "base::UnguessableToken -> void*");
  Measure<base::UnguessableToken, void*>([](auto& container, size_t i) {
    auto token = base::UnguessableToken::Create();
    ScopedLogAllocAndFree scoped_logging;
    container.insert({token, nullptr});
  });
  RAW_LOG(INFO, "base::UnguessableToken -> base::Value");
  Measure<base::UnguessableToken, base::Value>([](auto& container, size_t i) {
    auto token = base::UnguessableToken::Create();
    base::Value value;
    ScopedLogAllocAndFree scoped_logging;
    container.insert({token, std::move(value)});
  });
  RAW_LOG(INFO, "base::UnguessableToken -> std::array<std::string, 4>");
  Measure<base::UnguessableToken, std::array<std::string, 4>>(
      [](auto& container, size_t i) {
        auto token = base::UnguessableToken::Create();
        ScopedLogAllocAndFree scoped_logging;
        container.insert({token, {}});
      });
  RAW_LOG(INFO, "base::UnguessableToken -> std::array<std::string, 8>");
  Measure<base::UnguessableToken, std::array<std::string, 8>>(
      [](auto& container, size_t i) {
        auto token = base::UnguessableToken::Create();
        ScopedLogAllocAndFree scoped_logging;
        container.insert({token, {}});
      });
  RAW_LOG(INFO, "base::UnguessableToken -> std::array<std::string, 16>");
  Measure<base::UnguessableToken, std::array<std::string, 16>>(
      [](auto& container, size_t i) {
        auto token = base::UnguessableToken::Create();
        ScopedLogAllocAndFree scoped_logging;
        container.insert({token, {}});
      });

  return 0;
}
