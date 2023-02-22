// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnail/cc/scoped_ptr_expiring_cache.h"

#include <stddef.h>

#include <algorithm>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/rand_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#define MAX_CACHE_SIZE 5u

namespace thumbnail {
namespace {

unsigned int GenerateValue(unsigned int key) {
  return (key * key * 127) % 133 + key * 23;
}

class MockObject {
 public:
  static std::unique_ptr<MockObject> Create(unsigned int key) {
    return base::WrapUnique(new MockObject(key));
  }

  MockObject(const MockObject&) = delete;
  MockObject& operator=(const MockObject&) = delete;

  unsigned int value() const { return value_; }

 private:
  explicit MockObject(unsigned int key) : value_(GenerateValue(key)) {}
  unsigned int value_;
};

}  // namespace

typedef testing::Test ScopedPtrExpiringCacheTest;
typedef ScopedPtrExpiringCache<unsigned int, MockObject>
    TestScopedPtrExpiringCache;

TEST_F(ScopedPtrExpiringCacheTest, SimplePutAndGet) {
  TestScopedPtrExpiringCache cache(MAX_CACHE_SIZE);
  EXPECT_EQ(MAX_CACHE_SIZE, cache.MaximumCacheSize());
  EXPECT_EQ(0u, cache.size());

  for (unsigned int i = 0; i < MAX_CACHE_SIZE; i++) {
    cache.Put(i, MockObject::Create(i));
  }

  EXPECT_EQ(MAX_CACHE_SIZE, cache.size());

  unsigned int next_key = MAX_CACHE_SIZE;

  // One cache entry should have been evicted.
  cache.Put(next_key, MockObject::Create(next_key));
  EXPECT_EQ(MAX_CACHE_SIZE, cache.size());

  size_t cached_count = 0;
  for (unsigned int i = 0; i < MAX_CACHE_SIZE + 1; i++) {
    if (cache.Get(i)) {
      EXPECT_EQ(GenerateValue(i), cache.Get(i)->value());
      cached_count++;
    }
  }

  EXPECT_EQ(MAX_CACHE_SIZE, cached_count);

  // Test Get as membership test.
  cached_count = 0;
  for (unsigned int i = 0; i < MAX_CACHE_SIZE + 1; i++) {
    if (cache.Get(i)) {
      cached_count++;
    }
  }
  EXPECT_EQ(MAX_CACHE_SIZE, cached_count);

  cache.Clear();
  EXPECT_EQ(0u, cache.size());

  for (unsigned int i = 0; i < MAX_CACHE_SIZE + 1; i++) {
    EXPECT_EQ(nullptr, cache.Get(i));
  }
}

// The eviction policy is least-recently-used, where we define used as insertion
// into the cache.  We test that the first to be evicted is the first entry
// inserted into the cache.
TEST_F(ScopedPtrExpiringCacheTest, EvictedEntry) {
  TestScopedPtrExpiringCache cache(MAX_CACHE_SIZE);
  for (unsigned int i = 0; i < MAX_CACHE_SIZE; i++) {
    cache.Put(i, MockObject::Create(i));
  }

  unsigned int next_key = MAX_CACHE_SIZE;
  cache.Put(next_key, MockObject::Create(next_key));
  EXPECT_EQ(MAX_CACHE_SIZE, cache.size());
  EXPECT_EQ(GenerateValue(next_key), cache.Get(next_key)->value());

  // The first inserted entry should have been evicted.
  EXPECT_EQ(nullptr, cache.Get(0));

  // The rest of the content should be present.
  for (unsigned int i = 1; i < MAX_CACHE_SIZE; i++) {
    EXPECT_TRUE(cache.Get(i) != nullptr);
  }

  next_key++;

  // The first candidate to be evicted is the head of the iterator.
  unsigned int head_key = cache.begin()->first;
  EXPECT_TRUE(cache.Get(head_key) != nullptr);
  cache.Put(next_key, MockObject::Create(next_key));

  EXPECT_NE(cache.begin()->first, head_key);
  EXPECT_EQ(nullptr, cache.Get(head_key));
}

TEST_F(ScopedPtrExpiringCacheTest, RetainedEntry) {
  TestScopedPtrExpiringCache cache(MAX_CACHE_SIZE);
  for (unsigned int i = 0; i < MAX_CACHE_SIZE; i++) {
    cache.Put(i, MockObject::Create(i));
  }

  // Add (cache size - 1)-entries.
  for (unsigned int i = 0; i < MAX_CACHE_SIZE; i++) {
    EXPECT_TRUE(cache.Get(i) != nullptr);
  }

  for (unsigned int i = MAX_CACHE_SIZE; i < 2 * MAX_CACHE_SIZE - 1; i++) {
    cache.Put(i, MockObject::Create(i));
  }

  EXPECT_EQ(MAX_CACHE_SIZE, cache.size());

  for (unsigned int i = 0; i < MAX_CACHE_SIZE - 1; i++) {
    EXPECT_EQ(nullptr, cache.Get(i));
  }

  // The only retained entry (from the first round of insertion) is the last to
  // be inserted.
  EXPECT_TRUE(cache.Get(MAX_CACHE_SIZE - 1) != nullptr);
}

// Test that the iterator order is the insertion order.  The first element of
// the iterator is the oldest entry in the cache.
TEST_F(ScopedPtrExpiringCacheTest, Iterator) {
  TestScopedPtrExpiringCache cache(MAX_CACHE_SIZE);
  std::vector<unsigned int> test_keys;
  for (unsigned int i = 0; i < MAX_CACHE_SIZE; i++) {
    test_keys.push_back(i);
  }
  base::RandomShuffle(test_keys.begin(), test_keys.end());

  for (unsigned int i = 0; i < MAX_CACHE_SIZE; i++) {
    cache.Put(test_keys[i], MockObject::Create(test_keys[i]));
  }

  TestScopedPtrExpiringCache::iterator cache_iter = cache.begin();
  std::vector<unsigned int>::iterator key_iter = test_keys.begin();
  while (cache_iter != cache.end() && key_iter != test_keys.end()) {
    EXPECT_EQ(cache_iter->first, *key_iter);
    cache_iter++;
    key_iter++;
  }
}

}  // namespace thumbnail
