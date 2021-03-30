// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_usage_estimator.h"

#include <stdlib.h>

#include <string>

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(ARCH_CPU_64_BITS)
#define EXPECT_EQ_32_64(_, e, a) EXPECT_EQ(e, a)
#else
#define EXPECT_EQ_32_64(e, _, a) EXPECT_EQ(e, a)
#endif

namespace base {
namespace trace_event {

namespace {

// Test class with predictable memory usage.
class Data {
 public:
  explicit Data(size_t size = 17): size_(size) {
  }

  size_t size() const { return size_; }

  size_t EstimateMemoryUsage() const {
    return size_;
  }

  bool operator < (const Data& other) const {
    return size_ < other.size_;
  }
  bool operator == (const Data& other) const {
    return size_ == other.size_;
  }

  struct Hasher {
    size_t operator () (const Data& data) const {
      return data.size();
    }
  };

 private:
  size_t size_;
};

}  // namespace

namespace internal {

// This kills variance of bucket_count across STL implementations.
template <>
size_t HashMapBucketCountForTesting<Data>(size_t) {
  return 10;
}
template <>
size_t HashMapBucketCountForTesting<std::pair<const Data, short>>(size_t) {
  return 10;
}

}  // namespace internal

TEST(EstimateMemoryUsageTest, String) {
  std::string string(777, 'a');
  EXPECT_EQ(string.capacity() + 1, EstimateMemoryUsage(string));
}

TEST(EstimateMemoryUsageTest, String16) {
  std::u16string string(777, 'a');
  EXPECT_EQ(sizeof(char16_t) * (string.capacity() + 1),
            EstimateMemoryUsage(string));
}

TEST(EstimateMemoryUsageTest, Arrays) {
  // std::array
  {
    std::array<Data, 10> array;
    EXPECT_EQ(170u, EstimateMemoryUsage(array));
  }

  // T[N]
  {
    Data array[10];
    EXPECT_EQ(170u, EstimateMemoryUsage(array));
  }

  // C array
  {
    struct Item {
      char payload[10];
    };
    Item* array = new Item[7];
    EXPECT_EQ(70u, EstimateMemoryUsage(array, 7));
    delete[] array;
  }
}

TEST(EstimateMemoryUsageTest, UniquePtr) {
  // Empty
  {
    std::unique_ptr<Data> ptr;
    EXPECT_EQ(0u, EstimateMemoryUsage(ptr));
  }

  // Not empty
  {
    std::unique_ptr<Data> ptr(new Data());
    EXPECT_EQ_32_64(21u, 25u, EstimateMemoryUsage(ptr));
  }

  // With a pointer
  {
    std::unique_ptr<Data*> ptr(new Data*());
    EXPECT_EQ(sizeof(void*), EstimateMemoryUsage(ptr));
  }

  // With an array
  {
    struct Item {
      uint32_t payload[10];
    };
    std::unique_ptr<Item[]> ptr(new Item[7]);
    EXPECT_EQ(280u, EstimateMemoryUsage(ptr, 7));
  }
}

TEST(EstimateMemoryUsageTest, Vector) {
  std::vector<Data> vector;
  vector.reserve(1000);

  // For an empty vector we should return memory usage of its buffer
  size_t capacity = vector.capacity();
  size_t expected_size = capacity * sizeof(Data);
  EXPECT_EQ(expected_size, EstimateMemoryUsage(vector));

  // If vector is not empty, its size should also include memory usages
  // of all elements.
  for (size_t i = 0; i != capacity / 2; ++i) {
    vector.push_back(Data(i));
    expected_size += EstimateMemoryUsage(vector.back());
  }
  EXPECT_EQ(expected_size, EstimateMemoryUsage(vector));
}

TEST(EstimateMemoryUsageTest, List) {
  struct POD {
    short data;
  };
  std::list<POD> list;
  for (int i = 0; i != 1000; ++i) {
    list.push_back(POD());
  }
  EXPECT_EQ_32_64(12000u, 24000u, EstimateMemoryUsage(list));
}

TEST(EstimateMemoryUsageTest, Set) {
  std::set<std::pair<int, Data>> set;
  for (int i = 0; i != 1000; ++i) {
    set.insert({i, Data(i)});
  }
  EXPECT_EQ_32_64(523500u, 547500u, EstimateMemoryUsage(set));
}

TEST(EstimateMemoryUsageTest, MultiSet) {
  std::multiset<bool> set;
  for (int i = 0; i != 1000; ++i) {
    set.insert((i & 1) != 0);
  }
  EXPECT_EQ_32_64(16000u, 32000u, EstimateMemoryUsage(set));
}

TEST(EstimateMemoryUsageTest, Map) {
  std::map<Data, int> map;
  for (int i = 0; i != 1000; ++i) {
    map.insert({Data(i), i});
  }
  EXPECT_EQ_32_64(523500u, 547500u, EstimateMemoryUsage(map));
}

TEST(EstimateMemoryUsageTest, MultiMap) {
  std::multimap<char, Data> map;
  for (int i = 0; i != 1000; ++i) {
    map.insert({static_cast<char>(i), Data(i)});
  }
  EXPECT_EQ_32_64(523500u, 547500u, EstimateMemoryUsage(map));
}

TEST(EstimateMemoryUsageTest, UnorderedSet) {
  std::unordered_set<Data, Data::Hasher> set;
  for (int i = 0; i != 1000; ++i) {
    set.insert(Data(i));
  }
  EXPECT_EQ_32_64(511540u, 523580u, EstimateMemoryUsage(set));
}

TEST(EstimateMemoryUsageTest, UnorderedMultiSet) {
  std::unordered_multiset<Data, Data::Hasher> set;
  for (int i = 0; i != 500; ++i) {
    set.insert(Data(i));
    set.insert(Data(i));
  }
  EXPECT_EQ_32_64(261540u, 273580u, EstimateMemoryUsage(set));
}

TEST(EstimateMemoryUsageTest, UnorderedMap) {
  std::unordered_map<Data, short, Data::Hasher> map;
  for (int i = 0; i != 1000; ++i) {
    map.insert({Data(i), static_cast<short>(i)});
  }
  EXPECT_EQ_32_64(515540u, 531580u, EstimateMemoryUsage(map));
}

TEST(EstimateMemoryUsageTest, UnorderedMultiMap) {
  std::unordered_multimap<Data, short, Data::Hasher> map;
  for (int i = 0; i != 1000; ++i) {
    map.insert({Data(i), static_cast<short>(i)});
  }
  EXPECT_EQ_32_64(515540u, 531580u, EstimateMemoryUsage(map));
}

TEST(EstimateMemoryUsageTest, Deque) {
  std::deque<Data> deque;

  // Pick a large value so that platform-specific accounting
  // for deque's blocks is small compared to usage of all items.
  constexpr size_t kDataSize = 100000;
  for (int i = 0; i != 1500; ++i) {
    deque.push_back(Data(kDataSize));
  }

  // Compare against a reasonable minimum (i.e. no overhead).
  size_t min_expected_usage = deque.size() * (sizeof(Data) + kDataSize);
  EXPECT_LE(min_expected_usage, EstimateMemoryUsage(deque));
}

TEST(EstimateMemoryUsageTest, IsStandardContainerComplexIteratorTest) {
  struct abstract {
    virtual void method() = 0;
  };

  static_assert(
      internal::IsStandardContainerComplexIterator<std::list<int>::iterator>(),
      "");
  static_assert(internal::IsStandardContainerComplexIterator<
                    std::list<int>::const_iterator>(),
                "");
  static_assert(internal::IsStandardContainerComplexIterator<
                    std::list<int>::reverse_iterator>(),
                "");
  static_assert(internal::IsStandardContainerComplexIterator<
                    std::list<int>::const_reverse_iterator>(),
                "");
  static_assert(!internal::IsStandardContainerComplexIterator<int>(), "");
  static_assert(!internal::IsStandardContainerComplexIterator<abstract*>(), "");
}

}  // namespace trace_event
}  // namespace base
