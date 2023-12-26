// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/apple/scoped_nsobject.h"
#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"

#if HAS_FEATURE(objc_arc)
#error "This file must not be compiled with ARC."
#endif

namespace {

TEST(ScopedNSObjectTest, ScopedNSObject) {
  base::apple::scoped_nsobject<NSObject> p1([[NSObject alloc] init]);
  ASSERT_TRUE(p1.get());
  ASSERT_EQ(1u, [p1.get() retainCount]);
  base::apple::scoped_nsobject<NSObject> p2(p1);
  ASSERT_EQ(p1.get(), p2.get());
  ASSERT_EQ(2u, [p1.get() retainCount]);
  p2.reset();
  ASSERT_EQ(nil, p2.get());
  ASSERT_EQ(1u, [p1.get() retainCount]);
  {
    base::apple::scoped_nsobject<NSObject> p3 = p1;
    ASSERT_EQ(p1.get(), p3.get());
    ASSERT_EQ(2u, [p1.get() retainCount]);
    @autoreleasepool {
      p3 = p1;
    }
    ASSERT_EQ(p1.get(), p3.get());
    ASSERT_EQ(2u, [p1.get() retainCount]);
  }
  ASSERT_EQ(1u, [p1.get() retainCount]);
  base::apple::scoped_nsobject<NSObject> p4([p1.get() retain]);
  ASSERT_EQ(2u, [p1.get() retainCount]);
  ASSERT_TRUE(p1 == p1);
  ASSERT_FALSE(p1 != p1);
  base::apple::scoped_nsobject<NSObject> p5([[NSObject alloc] init]);
  ASSERT_TRUE(p1 != p5);
  ASSERT_FALSE(p1 == p5);

  base::apple::scoped_nsobject<NSObject> p6 = p1;
  ASSERT_EQ(3u, [p6.get() retainCount]);
  @autoreleasepool {
    p6.autorelease();
    ASSERT_EQ(nil, p6.get());
    ASSERT_EQ(3u, [p1.get() retainCount]);
  }
  ASSERT_EQ(2u, [p1.get() retainCount]);

  base::apple::scoped_nsobject<NSObject> p7([[NSObject alloc] init]);
  base::apple::scoped_nsobject<NSObject> p8(std::move(p7));
  ASSERT_TRUE(p8);
  ASSERT_EQ(1u, [p8.get() retainCount]);
  ASSERT_FALSE(p7.get());
}

// Instantiating scoped_nsobject<> with T=NSAutoreleasePool should trip a
// static_assert.
#if 0
TEST(ScopedNSObjectTest, FailToCreateScopedNSObjectAutoreleasePool) {
  base::apple::scoped_nsobject<NSAutoreleasePool> pool;
}
#endif

TEST(ScopedNSObjectTest, ScopedNSObjectInContainer) {
  base::apple::scoped_nsobject<id> p([[NSObject alloc] init]);
  ASSERT_TRUE(p.get());
  ASSERT_EQ(1u, [p.get() retainCount]);
  {
    std::vector<base::apple::scoped_nsobject<id>> objects;
    objects.push_back(p);
    ASSERT_EQ(2u, [p.get() retainCount]);
    ASSERT_EQ(p.get(), objects[0].get());
    objects.push_back(
        base::apple::scoped_nsobject<id>([[NSObject alloc] init]));
    ASSERT_TRUE(objects[1].get());
    ASSERT_EQ(1u, [objects[1].get() retainCount]);
  }
  ASSERT_EQ(1u, [p.get() retainCount]);
}

TEST(ScopedNSObjectTest, ScopedNSObjectFreeFunctions) {
  base::apple::scoped_nsobject<id> p1([[NSObject alloc] init]);
  id o1 = p1.get();
  ASSERT_TRUE(o1 == p1);
  ASSERT_FALSE(o1 != p1);
  base::apple::scoped_nsobject<id> p2([[NSObject alloc] init]);
  ASSERT_TRUE(o1 != p2);
  ASSERT_FALSE(o1 == p2);
  id o2 = p2.get();
  swap(p1, p2);
  ASSERT_EQ(o2, p1.get());
  ASSERT_EQ(o1, p2.get());
}

TEST(ScopedNSObjectTest, ResetWithAnotherScopedNSObject) {
  base::apple::scoped_nsobject<id> p1([[NSObject alloc] init]);
  id o1 = p1.get();

  id o2 = nil;
  {
    base::apple::scoped_nsobject<id> p2([[NSObject alloc] init]);
    o2 = p2.get();
    p1.reset(p2);
    EXPECT_EQ(2u, [p1.get() retainCount]);
  }

  EXPECT_NE(o1, p1.get());
  EXPECT_EQ(o2, p1.get());
  EXPECT_NE(p1.get(), nil);

  EXPECT_EQ(1u, [p1.get() retainCount]);
}

}  // namespace
