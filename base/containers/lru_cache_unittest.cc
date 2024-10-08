// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/lru_cache.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/tracing_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_BASE_TRACING)
#include "base/trace_event/memory_usage_estimator.h"  // no-presubmit-check
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

namespace base {

namespace {

using testing::_;
using testing::Pair;

int cached_item_live_count = 0;

struct CachedItem {
  CachedItem() : value(0) { cached_item_live_count++; }

  explicit CachedItem(int new_value) : value(new_value) {
    cached_item_live_count++;
  }

  CachedItem(const CachedItem& other) : value(other.value) {
    cached_item_live_count++;
  }

  ~CachedItem() { cached_item_live_count--; }

  int value;
};

}  // namespace

template <typename LRUCacheTemplate>
class LRUCacheTest : public testing::Test {};

struct LRUCacheTemplate {
  template <class Key, class Value, class KeyCompare = std::less<Key>>
  using Type = base::LRUCache<Key, Value, KeyCompare>;
};

struct HashingLRUCacheTemplate {
  template <class Key,
            class Value,
            class KeyHash = std::hash<Key>,
            class KeyEqual = std::equal_to<Key>>
  using Type = base::HashingLRUCache<Key, Value, KeyHash, KeyEqual>;
};

using LRUCacheTemplates =
    testing::Types<LRUCacheTemplate, HashingLRUCacheTemplate>;
TYPED_TEST_SUITE(LRUCacheTest, LRUCacheTemplates);

template <typename LRUCacheSetTemplate>
class LRUCacheSetTest : public testing::Test {};

struct LRUCacheSetTemplate {
  template <class Value, class Compare = std::less<Value>>
  using Type = base::LRUCacheSet<Value, Compare>;
};

struct HashingLRUCacheSetTemplate {
  template <class Value,
            class Hash = std::hash<Value>,
            class Equal = std::equal_to<Value>>
  using Type = base::HashingLRUCacheSet<Value, Hash, Equal>;
};

using LRUCacheSetTemplates =
    testing::Types<LRUCacheSetTemplate, HashingLRUCacheSetTemplate>;
TYPED_TEST_SUITE(LRUCacheSetTest, LRUCacheSetTemplates);

TYPED_TEST(LRUCacheTest, Basic) {
  typedef typename TypeParam::template Type<int, CachedItem> Cache;
  Cache cache(Cache::NO_AUTO_EVICT);

  // Check failure conditions
  {
    CachedItem test_item;
    EXPECT_TRUE(cache.Get(0) == cache.end());
    EXPECT_TRUE(cache.Peek(0) == cache.end());
  }

  static const int kItem1Key = 5;
  CachedItem item1(10);
  auto inserted_item = cache.Put(kItem1Key, item1);
  EXPECT_EQ(1U, cache.size());

  // Check that item1 was properly inserted.
  {
    auto found = cache.Get(kItem1Key);
    EXPECT_TRUE(inserted_item == cache.begin());
    EXPECT_TRUE(found != cache.end());

    found = cache.Peek(kItem1Key);
    EXPECT_TRUE(found != cache.end());

    EXPECT_EQ(kItem1Key, found->first);
    EXPECT_EQ(item1.value, found->second.value);
  }

  static const int kItem2Key = 7;
  CachedItem item2(12);
  cache.Put(kItem2Key, item2);
  EXPECT_EQ(2U, cache.size());

  // Check that item1 is the oldest since item2 was added afterwards.
  {
    auto oldest = cache.rbegin();
    ASSERT_TRUE(oldest != cache.rend());
    EXPECT_EQ(kItem1Key, oldest->first);
    EXPECT_EQ(item1.value, oldest->second.value);
  }

  // Check that item1 is still accessible by key.
  {
    auto test_item = cache.Get(kItem1Key);
    ASSERT_TRUE(test_item != cache.end());
    EXPECT_EQ(kItem1Key, test_item->first);
    EXPECT_EQ(item1.value, test_item->second.value);
  }

  // Check that retrieving item1 pushed item2 to oldest.
  {
    auto oldest = cache.rbegin();
    ASSERT_TRUE(oldest != cache.rend());
    EXPECT_EQ(kItem2Key, oldest->first);
    EXPECT_EQ(item2.value, oldest->second.value);
  }

  // Remove the oldest item and check that item1 is now the only member.
  {
    auto next = cache.Erase(cache.rbegin());

    EXPECT_EQ(1U, cache.size());

    EXPECT_TRUE(next == cache.rbegin());
    EXPECT_EQ(kItem1Key, next->first);
    EXPECT_EQ(item1.value, next->second.value);

    cache.Erase(cache.begin());
    EXPECT_EQ(0U, cache.size());
  }

  // Check that Clear() works properly.
  cache.Put(kItem1Key, item1);
  cache.Put(kItem2Key, item2);
  EXPECT_EQ(2U, cache.size());
  cache.Clear();
  EXPECT_EQ(0U, cache.size());
}

TYPED_TEST(LRUCacheTest, GetVsPeek) {
  typedef typename TypeParam::template Type<int, CachedItem> Cache;
  Cache cache(Cache::NO_AUTO_EVICT);

  static const int kItem1Key = 1;
  CachedItem item1(10);
  cache.Put(kItem1Key, item1);

  static const int kItem2Key = 2;
  CachedItem item2(20);
  cache.Put(kItem2Key, item2);

  // This should do nothing since the size is bigger than the number of items.
  cache.ShrinkToSize(100);

  // Check that item1 starts out as oldest
  {
    auto iter = cache.rbegin();
    ASSERT_TRUE(iter != cache.rend());
    EXPECT_EQ(kItem1Key, iter->first);
    EXPECT_EQ(item1.value, iter->second.value);
  }

  // Check that Peek doesn't change ordering
  {
    auto peekiter = cache.Peek(kItem1Key);
    ASSERT_TRUE(peekiter != cache.end());

    auto iter = cache.rbegin();
    ASSERT_TRUE(iter != cache.rend());
    EXPECT_EQ(kItem1Key, iter->first);
    EXPECT_EQ(item1.value, iter->second.value);
  }
}

TYPED_TEST(LRUCacheTest, KeyReplacement) {
  typedef typename TypeParam::template Type<int, CachedItem> Cache;
  Cache cache(Cache::NO_AUTO_EVICT);

  static const int kItem1Key = 1;
  CachedItem item1(10);
  cache.Put(kItem1Key, item1);

  static const int kItem2Key = 2;
  CachedItem item2(20);
  cache.Put(kItem2Key, item2);

  static const int kItem3Key = 3;
  CachedItem item3(30);
  cache.Put(kItem3Key, item3);

  static const int kItem4Key = 4;
  CachedItem item4(40);
  cache.Put(kItem4Key, item4);

  CachedItem item5(50);
  cache.Put(kItem3Key, item5);

  EXPECT_EQ(4U, cache.size());
  for (int i = 0; i < 3; ++i) {
    auto iter = cache.rbegin();
    ASSERT_TRUE(iter != cache.rend());
  }

  // Make it so only the most important element is there.
  cache.ShrinkToSize(1);

  auto iter = cache.begin();
  EXPECT_EQ(kItem3Key, iter->first);
  EXPECT_EQ(item5.value, iter->second.value);
}

// Make sure that the cache release its pointers properly.
TYPED_TEST(LRUCacheTest, Owning) {
  using Cache =
      typename TypeParam::template Type<int, std::unique_ptr<CachedItem>>;
  Cache cache(Cache::NO_AUTO_EVICT);

  int initial_count = cached_item_live_count;

  // First insert and item and then overwrite it.
  static const int kItem1Key = 1;
  cache.Put(kItem1Key, std::make_unique<CachedItem>(20));
  cache.Put(kItem1Key, std::make_unique<CachedItem>(22));

  // There should still be one item, and one extra live item.
  auto iter = cache.Get(kItem1Key);
  EXPECT_EQ(1U, cache.size());
  EXPECT_TRUE(iter != cache.end());
  EXPECT_EQ(initial_count + 1, cached_item_live_count);

  // Now remove it.
  cache.Erase(cache.begin());
  EXPECT_EQ(initial_count, cached_item_live_count);

  // Now try another cache that goes out of scope to make sure its pointers
  // go away.
  {
    Cache cache2(Cache::NO_AUTO_EVICT);
    cache2.Put(1, std::make_unique<CachedItem>(20));
    cache2.Put(2, std::make_unique<CachedItem>(20));
  }

  // There should be no objects leaked.
  EXPECT_EQ(initial_count, cached_item_live_count);

  // Check that Clear() also frees things correctly.
  {
    Cache cache2(Cache::NO_AUTO_EVICT);
    cache2.Put(1, std::make_unique<CachedItem>(20));
    cache2.Put(2, std::make_unique<CachedItem>(20));
    EXPECT_EQ(initial_count + 2, cached_item_live_count);
    cache2.Clear();
    EXPECT_EQ(initial_count, cached_item_live_count);
  }
}

TYPED_TEST(LRUCacheTest, AutoEvict) {
  using Cache =
      typename TypeParam::template Type<int, std::unique_ptr<CachedItem>>;
  static const typename Cache::size_type kMaxSize = 3;

  int initial_count = cached_item_live_count;

  {
    Cache cache(kMaxSize);

    static const int kItem1Key = 1, kItem2Key = 2, kItem3Key = 3, kItem4Key = 4;
    cache.Put(kItem1Key, std::make_unique<CachedItem>(20));
    cache.Put(kItem2Key, std::make_unique<CachedItem>(21));
    cache.Put(kItem3Key, std::make_unique<CachedItem>(22));
    cache.Put(kItem4Key, std::make_unique<CachedItem>(23));

    // The cache should only have kMaxSize items in it even though we inserted
    // more.
    EXPECT_EQ(kMaxSize, cache.size());
  }

  // There should be no objects leaked.
  EXPECT_EQ(initial_count, cached_item_live_count);
}

TYPED_TEST(LRUCacheTest, HashingLRUCache) {
  // Very simple test to make sure that the hashing cache works correctly.
  typedef typename TypeParam::template Type<std::string, CachedItem> Cache;
  Cache cache(Cache::NO_AUTO_EVICT);

  CachedItem one(1);
  cache.Put("First", one);

  CachedItem two(2);
  cache.Put("Second", two);

  EXPECT_EQ(one.value, cache.Get("First")->second.value);
  EXPECT_EQ(two.value, cache.Get("Second")->second.value);
  cache.ShrinkToSize(1);
  EXPECT_EQ(two.value, cache.Get("Second")->second.value);
  EXPECT_TRUE(cache.Get("First") == cache.end());
}

TYPED_TEST(LRUCacheTest, Swap) {
  typedef typename TypeParam::template Type<int, CachedItem> Cache;
  Cache cache1(Cache::NO_AUTO_EVICT);

  // Insert two items into cache1.
  static const int kItem1Key = 1;
  CachedItem item1(2);
  auto inserted_item = cache1.Put(kItem1Key, item1);
  EXPECT_EQ(1U, cache1.size());

  static const int kItem2Key = 3;
  CachedItem item2(4);
  cache1.Put(kItem2Key, item2);
  EXPECT_EQ(2U, cache1.size());

  // Verify cache1's elements.
  {
    auto iter = cache1.begin();
    ASSERT_TRUE(iter != cache1.end());
    EXPECT_EQ(kItem2Key, iter->first);
    EXPECT_EQ(item2.value, iter->second.value);

    ++iter;
    ASSERT_TRUE(iter != cache1.end());
    EXPECT_EQ(kItem1Key, iter->first);
    EXPECT_EQ(item1.value, iter->second.value);
  }

  // Create another cache2.
  Cache cache2(Cache::NO_AUTO_EVICT);

  // Insert three items into cache2.
  static const int kItem3Key = 5;
  CachedItem item3(6);
  inserted_item = cache2.Put(kItem3Key, item3);
  EXPECT_EQ(1U, cache2.size());

  static const int kItem4Key = 7;
  CachedItem item4(8);
  cache2.Put(kItem4Key, item4);
  EXPECT_EQ(2U, cache2.size());

  static const int kItem5Key = 9;
  CachedItem item5(10);
  cache2.Put(kItem5Key, item5);
  EXPECT_EQ(3U, cache2.size());

  // Verify cache2's elements.
  {
    auto iter = cache2.begin();
    ASSERT_TRUE(iter != cache2.end());
    EXPECT_EQ(kItem5Key, iter->first);
    EXPECT_EQ(item5.value, iter->second.value);

    ++iter;
    ASSERT_TRUE(iter != cache2.end());
    EXPECT_EQ(kItem4Key, iter->first);
    EXPECT_EQ(item4.value, iter->second.value);

    ++iter;
    ASSERT_TRUE(iter != cache2.end());
    EXPECT_EQ(kItem3Key, iter->first);
    EXPECT_EQ(item3.value, iter->second.value);
  }

  // Swap cache1 and cache2 and verify cache2 has cache1's elements and cache1
  // has cache2's elements.
  cache2.Swap(cache1);

  EXPECT_EQ(3U, cache1.size());
  EXPECT_EQ(2U, cache2.size());

  // Verify cache1's elements.
  {
    auto iter = cache1.begin();
    ASSERT_TRUE(iter != cache1.end());
    EXPECT_EQ(kItem5Key, iter->first);
    EXPECT_EQ(item5.value, iter->second.value);

    ++iter;
    ASSERT_TRUE(iter != cache1.end());
    EXPECT_EQ(kItem4Key, iter->first);
    EXPECT_EQ(item4.value, iter->second.value);

    ++iter;
    ASSERT_TRUE(iter != cache1.end());
    EXPECT_EQ(kItem3Key, iter->first);
    EXPECT_EQ(item3.value, iter->second.value);
  }

  // Verify cache2's elements.
  {
    auto iter = cache2.begin();
    ASSERT_TRUE(iter != cache2.end());
    EXPECT_EQ(kItem2Key, iter->first);
    EXPECT_EQ(item2.value, iter->second.value);

    ++iter;
    ASSERT_TRUE(iter != cache2.end());
    EXPECT_EQ(kItem1Key, iter->first);
    EXPECT_EQ(item1.value, iter->second.value);
  }
}

TYPED_TEST(LRUCacheSetTest, SetTest) {
  typedef typename TypeParam::template Type<std::string> Cache;
  Cache cache(Cache::NO_AUTO_EVICT);

  cache.Put("Hello");
  cache.Put("world");
  cache.Put("foo");
  cache.Put("bar");

  // Insert a duplicate element
  cache.Put("foo");

  // Iterate from oldest to newest
  auto r_iter = cache.rbegin();
  EXPECT_EQ(*r_iter, "Hello");
  ++r_iter;
  EXPECT_EQ(*r_iter, "world");
  ++r_iter;
  EXPECT_EQ(*r_iter, "bar");
  ++r_iter;
  EXPECT_EQ(*r_iter, "foo");
  ++r_iter;
  EXPECT_EQ(r_iter, cache.rend());

  // Iterate from newest to oldest
  auto iter = cache.begin();
  EXPECT_EQ(*iter, "foo");
  ++iter;
  EXPECT_EQ(*iter, "bar");
  ++iter;
  EXPECT_EQ(*iter, "world");
  ++iter;
  EXPECT_EQ(*iter, "Hello");
  ++iter;
  EXPECT_EQ(iter, cache.end());
}

// Generalized dereference function. For the base case, this is the identity
// function.
template <typename T>
struct Deref {
  using Target = T;
  static const Target& deref(const T& x) { return x; }
};

// `RefCountedData` wraps a type in an interface that supports refcounting.
// Deref this as the wrapped type.
template <typename T>
struct Deref<RefCountedData<T>> {
  using Target = typename Deref<T>::Target;
  static const Target& deref(const RefCountedData<T>& x) {
    return Deref<T>::deref(x.data);
  }
};

// `scoped_refptr` is a smart pointer that implements reference counting.
// Deref this as the pointee.
template <typename T>
struct Deref<scoped_refptr<T>> {
  using Target = typename Deref<T>::Target;
  static const Target& deref(const scoped_refptr<T>& x) {
    return Deref<T>::deref(*x);
  }
};

// Implementation of a `std::less`-like type that dereferences the given values
// before comparison.
template <typename T>
struct DerefCompare {
  bool operator()(const T& lhs, const T& rhs) const {
    return Deref<T>::deref(lhs) < Deref<T>::deref(rhs);
  }
};

// Implementation of a `std::equal_to`-like type that dereferences the given
// values before comparison.
template <typename T>
struct DerefEqual {
  bool operator()(const T& lhs, const T& rhs) const {
    return Deref<T>::deref(lhs) == Deref<T>::deref(rhs);
  }
};

// Implementation of a `std::hash`-like type that dereferences the given value
// before calculating the hash.
template <typename T, template <class> typename HashT = std::hash>
struct DerefHash {
  size_t operator()(const T& x) const {
    return HashT<typename Deref<T>::Target>()(Deref<T>::deref(x));
  }
};

// This tests that upon replacing a duplicate element in the cache with `Put`,
// the element's identity is replaced as well.
TYPED_TEST(LRUCacheSetTest, ReplacementIdentity) {
  using Item = RefCountedData<std::string>;
  using Ptr = scoped_refptr<Item>;

  // Helper to create the correct type of base::*LRUCacheSet, since they have
  // different template arguments.
  constexpr auto kCreateCache = [] {
    if constexpr (std::is_same_v<TypeParam, LRUCacheSetTemplate>) {
      using Cache = typename TypeParam::template Type<Ptr, DerefCompare<Ptr>>;
      return Cache(Cache::NO_AUTO_EVICT);
    } else if constexpr (std::is_same_v<TypeParam,
                                        HashingLRUCacheSetTemplate>) {
      using Cache = typename TypeParam::template Type<Ptr, DerefHash<Ptr>,
                                                      DerefEqual<Ptr>>;
      return Cache(Cache::NO_AUTO_EVICT);
    } else {
      static_assert(!sizeof(TypeParam),
                    "This test was only written to support "
                    "`LRUCacheSetTemplate` and `HashingLRUCacheSetTemplate`");
    }
  };

  auto cache = kCreateCache();
  cache.Put(MakeRefCounted<Item>("Hello"));
  cache.Put(MakeRefCounted<Item>("world"));
  cache.Put(MakeRefCounted<Item>("foo"));
  cache.Put(MakeRefCounted<Item>("bar"));

  // Insert a duplicate element
  {
    auto foo = MakeRefCounted<Item>("foo");
    const auto* new_foo_addr = foo.get();
    const auto* old_foo_addr = cache.Peek(foo)->get();
    auto iter = cache.Put(std::move(foo));
    EXPECT_EQ(iter->get(), new_foo_addr);
    EXPECT_NE(iter->get(), old_foo_addr);
  }

  // Iterate from oldest to newest
  auto r_iter = cache.rbegin();
  EXPECT_EQ((*r_iter)->data, "Hello");
  ++r_iter;
  EXPECT_EQ((*r_iter)->data, "world");
  ++r_iter;
  EXPECT_EQ((*r_iter)->data, "bar");
  ++r_iter;
  EXPECT_EQ((*r_iter)->data, "foo");
  ++r_iter;
  EXPECT_EQ(r_iter, cache.rend());

  // Iterate from newest to oldest
  auto iter = cache.begin();
  EXPECT_EQ((*iter)->data, "foo");
  ++iter;
  EXPECT_EQ((*iter)->data, "bar");
  ++iter;
  EXPECT_EQ((*iter)->data, "world");
  ++iter;
  EXPECT_EQ((*iter)->data, "Hello");
  ++iter;
  EXPECT_EQ(iter, cache.end());
}

#if BUILDFLAG(ENABLE_BASE_TRACING)
TYPED_TEST(LRUCacheTest, EstimateMemory) {
  typedef typename TypeParam::template Type<std::string, int> Cache;
  Cache cache(10);

  const std::string key(100u, 'a');
  cache.Put(key, 1);

  EXPECT_GT(trace_event::EstimateMemoryUsage(cache),
            trace_event::EstimateMemoryUsage(key));
}
#endif  // BUILDFLAG(ENABLE_BASE_TRACING)

TEST(LRUCacheIndexOrderTest, IndexIteration) {
  using OrderedCache = LRUCache<int, CachedItem>;
  using UnorderedCache = HashingLRUCache<int, CachedItem>;

  OrderedCache ordered(OrderedCache::NO_AUTO_EVICT);
  UnorderedCache unordered(UnorderedCache::NO_AUTO_EVICT);

  // Add items in any order.
  static const int kItem1Key = 1;
  CachedItem item1(10);
  ordered.Put(kItem1Key, item1);
  unordered.Put(kItem1Key, item1);

  static const int kItem3Key = 3;
  CachedItem item3(30);
  ordered.Put(kItem3Key, item3);
  unordered.Put(kItem3Key, item3);

  static const int kItem2Key = 2;
  CachedItem item2(20);
  ordered.Put(kItem2Key, item2);
  unordered.Put(kItem2Key, item2);

  static const int kItem4Key = 4;
  CachedItem item4(40);
  ordered.Put(kItem4Key, item4);
  unordered.Put(kItem4Key, item4);

  // Ordered should be ordered, and unordered should at least have all elements.
  std::vector<int> ordered_keys;
  std::ranges::transform(
      ordered.index(), std::back_inserter(ordered_keys),
      [](const auto& key_value_pair) -> int { return key_value_pair.first; });
  EXPECT_THAT(ordered_keys,
              testing::ElementsAre(kItem1Key, kItem2Key, kItem3Key, kItem4Key));

  std::vector<int> unordered_keys;
  std::ranges::transform(
      unordered.index(), std::back_inserter(unordered_keys),
      [](const auto& key_value_pair) -> int { return key_value_pair.first; });
  EXPECT_THAT(unordered_keys, testing::UnorderedElementsAre(
                                  kItem1Key, kItem2Key, kItem3Key, kItem4Key));
}

}  // namespace base
