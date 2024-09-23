// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/containers/id_map.h"

#include <stdint.h>

#include <functional>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::test::id_map {
struct RepeatingKeyType {
  explicit RepeatingKeyType(int i) : i(i) {}

  constexpr void operator++() {}
  constexpr RepeatingKeyType& operator++(int) { return *this; }

  constexpr bool operator==(const RepeatingKeyType& o) const {
    return i == o.i;
  }
  constexpr bool operator<(const RepeatingKeyType& o) const { return i < o.i; }

  int i = 0;
};
}  // namespace base::test::id_map

namespace std {
template <>
struct hash<::base::test::id_map::RepeatingKeyType> {
  size_t operator()(
      const ::base::test::id_map::RepeatingKeyType& k) const noexcept {
    return std::hash<int>()(k.i);
  }
};
}  // namespace std

namespace base {

namespace {

class TestObject {};

class DestructorCounter {
 public:
  explicit DestructorCounter(int* counter) : counter_(counter) {}
  ~DestructorCounter() { ++(*counter_); }

 private:
  raw_ptr<int> counter_;
};

}  // namespace

TEST(IDMapTest, Basic) {
  IDMap<TestObject*> map;
  EXPECT_TRUE(map.IsEmpty());
  EXPECT_EQ(0U, map.size());

  TestObject obj1;
  TestObject obj2;

  int32_t id1 = map.Add(&obj1);
  EXPECT_FALSE(map.IsEmpty());
  EXPECT_EQ(1U, map.size());
  EXPECT_EQ(&obj1, map.Lookup(id1));

  int32_t id2 = map.Add(&obj2);
  EXPECT_FALSE(map.IsEmpty());
  EXPECT_EQ(2U, map.size());

  EXPECT_EQ(&obj1, map.Lookup(id1));
  EXPECT_EQ(&obj2, map.Lookup(id2));

  map.Remove(id1);
  EXPECT_FALSE(map.IsEmpty());
  EXPECT_EQ(1U, map.size());

  map.Remove(id2);
  EXPECT_TRUE(map.IsEmpty());
  EXPECT_EQ(0U, map.size());

  map.AddWithID(&obj1, 1);
  map.AddWithID(&obj2, 2);
  EXPECT_EQ(&obj1, map.Lookup(1));
  EXPECT_EQ(&obj2, map.Lookup(2));

  EXPECT_EQ(&obj2, map.Replace(2, &obj1));
  EXPECT_EQ(&obj1, map.Lookup(2));

  EXPECT_EQ(0, map.iteration_depth());
}

TEST(IDMapTest, IteratorRemainsValidWhenRemovingCurrentElement) {
  IDMap<TestObject*> map;

  TestObject obj1;
  TestObject obj2;
  TestObject obj3;

  map.Add(&obj1);
  map.Add(&obj2);
  map.Add(&obj3);

  {
    IDMap<TestObject*>::const_iterator iter(&map);

    EXPECT_EQ(1, map.iteration_depth());

    while (!iter.IsAtEnd()) {
      map.Remove(iter.GetCurrentKey());
      iter.Advance();
    }

    // Test that while an iterator is still in scope, we get the map emptiness
    // right (http://crbug.com/35571).
    EXPECT_TRUE(map.IsEmpty());
    EXPECT_EQ(0U, map.size());
  }

  EXPECT_TRUE(map.IsEmpty());
  EXPECT_EQ(0U, map.size());

  EXPECT_EQ(0, map.iteration_depth());
}

TEST(IDMapTest, IteratorRemainsValidWhenRemovingOtherElements) {
  IDMap<TestObject*> map;

  const int kCount = 5;
  TestObject obj[kCount];

  for (auto& i : obj) {
    map.Add(&i);
  }

  // IDMap has no predictable iteration order.
  int32_t ids_in_iteration_order[kCount];
  const TestObject* objs_in_iteration_order[kCount];
  int counter = 0;
  for (IDMap<TestObject*>::const_iterator iter(&map); !iter.IsAtEnd();
       iter.Advance()) {
    ids_in_iteration_order[counter] = iter.GetCurrentKey();
    objs_in_iteration_order[counter] = iter.GetCurrentValue();
    counter++;
  }

  counter = 0;
  for (IDMap<TestObject*>::const_iterator iter(&map); !iter.IsAtEnd();
       iter.Advance()) {
    EXPECT_EQ(1, map.iteration_depth());

    switch (counter) {
      case 0:
        EXPECT_EQ(ids_in_iteration_order[0], iter.GetCurrentKey());
        EXPECT_EQ(objs_in_iteration_order[0], iter.GetCurrentValue());
        map.Remove(ids_in_iteration_order[1]);
        break;
      case 1:
        EXPECT_EQ(ids_in_iteration_order[2], iter.GetCurrentKey());
        EXPECT_EQ(objs_in_iteration_order[2], iter.GetCurrentValue());
        map.Remove(ids_in_iteration_order[3]);
        break;
      case 2:
        EXPECT_EQ(ids_in_iteration_order[4], iter.GetCurrentKey());
        EXPECT_EQ(objs_in_iteration_order[4], iter.GetCurrentValue());
        map.Remove(ids_in_iteration_order[0]);
        break;
      default:
        FAIL() << "should not have that many elements";
    }

    counter++;
  }

  EXPECT_EQ(0, map.iteration_depth());
}

TEST(IDMapTest, CopyIterator) {
  IDMap<TestObject*> map;

  TestObject obj1;
  TestObject obj2;
  TestObject obj3;

  map.Add(&obj1);
  map.Add(&obj2);
  map.Add(&obj3);

  EXPECT_EQ(0, map.iteration_depth());

  {
    IDMap<TestObject*>::const_iterator iter1(&map);
    EXPECT_EQ(1, map.iteration_depth());

    // Make sure that copying the iterator correctly increments
    // map's iteration depth.
    IDMap<TestObject*>::const_iterator iter2(iter1);
    EXPECT_EQ(2, map.iteration_depth());
  }

  // Make sure after destroying all iterators the map's iteration depth
  // returns to initial state.
  EXPECT_EQ(0, map.iteration_depth());
}

TEST(IDMapTest, AssignIterator) {
  IDMap<TestObject*> map;

  TestObject obj1;
  TestObject obj2;
  TestObject obj3;

  map.Add(&obj1);
  map.Add(&obj2);
  map.Add(&obj3);

  EXPECT_EQ(0, map.iteration_depth());

  {
    IDMap<TestObject*>::const_iterator iter1(&map);
    EXPECT_EQ(1, map.iteration_depth());

    IDMap<TestObject*>::const_iterator iter2(&map);
    EXPECT_EQ(2, map.iteration_depth());

    // Make sure that assigning the iterator correctly updates
    // map's iteration depth (-1 for destruction, +1 for assignment).
    EXPECT_EQ(2, map.iteration_depth());
  }

  // Make sure after destroying all iterators the map's iteration depth
  // returns to initial state.
  EXPECT_EQ(0, map.iteration_depth());
}

TEST(IDMapTest, IteratorRemainsValidWhenClearing) {
  IDMap<TestObject*> map;

  const int kCount = 5;
  TestObject obj[kCount];

  for (auto& i : obj) {
    map.Add(&i);
  }

  // IDMap has no predictable iteration order.
  int32_t ids_in_iteration_order[kCount];
  const TestObject* objs_in_iteration_order[kCount];
  int counter = 0;
  for (IDMap<TestObject*>::const_iterator iter(&map); !iter.IsAtEnd();
       iter.Advance()) {
    ids_in_iteration_order[counter] = iter.GetCurrentKey();
    objs_in_iteration_order[counter] = iter.GetCurrentValue();
    counter++;
  }

  counter = 0;
  for (IDMap<TestObject*>::const_iterator iter(&map); !iter.IsAtEnd();
       iter.Advance()) {
    switch (counter) {
      case 0:
        EXPECT_EQ(ids_in_iteration_order[0], iter.GetCurrentKey());
        EXPECT_EQ(objs_in_iteration_order[0], iter.GetCurrentValue());
        break;
      case 1:
        EXPECT_EQ(ids_in_iteration_order[1], iter.GetCurrentKey());
        EXPECT_EQ(objs_in_iteration_order[1], iter.GetCurrentValue());
        map.Clear();
        EXPECT_TRUE(map.IsEmpty());
        EXPECT_EQ(0U, map.size());
        break;
      default:
        FAIL() << "should not have that many elements";
    }
    counter++;
  }

  EXPECT_TRUE(map.IsEmpty());
  EXPECT_EQ(0U, map.size());
}

TEST(IDMapTest, OwningPointersDeletesThemOnRemove) {
  const int kCount = 3;

  int external_del_count = 0;
  DestructorCounter* external_obj[kCount];
  int map_external_ids[kCount];

  int owned_del_count = 0;
  int map_owned_ids[kCount];

  IDMap<DestructorCounter*> map_external;
  IDMap<std::unique_ptr<DestructorCounter>> map_owned;

  for (int i = 0; i < kCount; ++i) {
    external_obj[i] = new DestructorCounter(&external_del_count);
    map_external_ids[i] = map_external.Add(external_obj[i]);

    map_owned_ids[i] =
        map_owned.Add(std::make_unique<DestructorCounter>(&owned_del_count));
  }

  for (int i = 0; i < kCount; ++i) {
    EXPECT_EQ(external_del_count, 0);
    EXPECT_EQ(owned_del_count, i);

    map_external.Remove(map_external_ids[i]);
    map_owned.Remove(map_owned_ids[i]);
  }

  for (auto* i : external_obj) {
    delete i;
  }

  EXPECT_EQ(external_del_count, kCount);
  EXPECT_EQ(owned_del_count, kCount);
}

TEST(IDMapTest, OwningPointersDeletesThemOnClear) {
  const int kCount = 3;

  int external_del_count = 0;
  DestructorCounter* external_obj[kCount];

  int owned_del_count = 0;

  IDMap<DestructorCounter*> map_external;
  IDMap<std::unique_ptr<DestructorCounter>> map_owned;

  for (auto*& i : external_obj) {
    i = new DestructorCounter(&external_del_count);
    map_external.Add(i);

    map_owned.Add(std::make_unique<DestructorCounter>(&owned_del_count));
  }

  EXPECT_EQ(external_del_count, 0);
  EXPECT_EQ(owned_del_count, 0);

  map_external.Clear();
  map_owned.Clear();

  EXPECT_EQ(external_del_count, 0);
  EXPECT_EQ(owned_del_count, kCount);

  for (auto* i : external_obj) {
    delete i;
  }

  EXPECT_EQ(external_del_count, kCount);
  EXPECT_EQ(owned_del_count, kCount);
}

TEST(IDMapTest, OwningPointersDeletesThemOnDestruct) {
  const int kCount = 3;

  int external_del_count = 0;
  DestructorCounter* external_obj[kCount];

  int owned_del_count = 0;

  {
    IDMap<DestructorCounter*> map_external;
    IDMap<std::unique_ptr<DestructorCounter>> map_owned;

    for (auto*& i : external_obj) {
      i = new DestructorCounter(&external_del_count);
      map_external.Add(i);

      map_owned.Add(std::make_unique<DestructorCounter>(&owned_del_count));
    }
  }

  EXPECT_EQ(external_del_count, 0);

  for (auto* i : external_obj) {
    delete i;
  }

  EXPECT_EQ(external_del_count, kCount);
  EXPECT_EQ(owned_del_count, kCount);
}

TEST(IDMapTest, Int64KeyType) {
  IDMap<TestObject*, int64_t> map;
  TestObject obj1;
  const int64_t kId1 = 999999999999999999;

  map.AddWithID(&obj1, kId1);
  EXPECT_EQ(&obj1, map.Lookup(kId1));

  IDMap<TestObject*, int64_t>::const_iterator iter(&map);
  ASSERT_FALSE(iter.IsAtEnd());
  EXPECT_EQ(kId1, iter.GetCurrentKey());
  EXPECT_EQ(&obj1, iter.GetCurrentValue());
  iter.Advance();
  ASSERT_TRUE(iter.IsAtEnd());

  map.Remove(kId1);
  EXPECT_TRUE(map.IsEmpty());
}

TEST(IDMapTest, RemovedValueHandling) {
  TestObject obj;
  {
    IDMap<TestObject*> map;
    int key = map.Add(&obj);

    IDMap<TestObject*>::iterator itr(&map);
    // Queues the `key` for removal.
    map.Clear();
    // Removes nothing, already queued.
    map.Remove(key);
    // Can not replace a key that is not present. If it's queued for removal
    // it's not present.
    EXPECT_CHECK_DEATH(map.Replace(key, &obj));
    EXPECT_FALSE(map.Lookup(key));
    EXPECT_FALSE(itr.IsAtEnd());
    EXPECT_FALSE(itr.GetCurrentValue());

    EXPECT_TRUE(map.IsEmpty());
    // Replaces the element that's queued for removal when `itr` is destroyed.
    map.AddWithID(&obj, key);
    EXPECT_EQ(1u, map.size());
  }

  {
    using base::test::id_map::RepeatingKeyType;

    IDMap<TestObject*, RepeatingKeyType> map;
    RepeatingKeyType key = map.Add(&obj);
    IDMap<TestObject*, RepeatingKeyType>::iterator itr(&map);
    map.Remove(key);  // Queues it for removal.

    // The RepeatingKeyType's operator++ does not always return a unique id. The
    // Add() method does not make extra assumptions about this, and can replace
    // a queued-for-removal item just like AddWithID().

    // Replaces the element that's queued for removal when `itr` is destroyed.
    RepeatingKeyType key2 = map.Add(&obj);
    EXPECT_EQ(key, key2);
  }
}

}  // namespace base
