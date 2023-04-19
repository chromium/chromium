// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <CoreFoundation/CoreFoundation.h>

#include <vector>

#import "base/mac/scoped_nsobject.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// A note about these tests.
//
// There are a lot of "autoreleasepool" scopes. Why? `ScopedTypeRef<>::get()` is
// a function returning an unretained Obj-C object, so ARC ends the call with
// `objc_retainAutoreleaseReturnValue()` hoping to make a hand-off to the caller
// using `objc_retainAutoreleasedReturnValue()` to maintain the reference count.
// Unfortunately, a simple call to `get()` won't take the hand-off, so the
// result is an extra retain and pending autorelease.
//
// For tests that aren't doing retain count testing, this doesn't matter. For
// tests that are doing retain count testing, all calls to `get()` have to be
// made within an autorelease pool to ensure that those pending autoreleases
// don't mess up the count.
//
// For further reading:
// https://www.mikeash.com/pyblog/friday-qa-2014-05-09-when-an-autorelease-isnt.html

namespace {

template <typename NST>
CFIndex GetRetainCount(const base::scoped_nsobject<NST>& nst) {
  @autoreleasepool {
    // -1 to compensate for the `get()` call creating an extra retain and
    // pending autorelease.
    return CFGetRetainCount((__bridge CFTypeRef)nst.get()) - 1;
  }
}

TEST(ScopedNSObjectTestARC, DefaultPolicyIsRetain) {
  id __weak o;
  @autoreleasepool {
    base::scoped_nsprotocol<id> p([[NSObject alloc] init]);
    o = p.get();
    ASSERT_EQ(o, p.get());
  }
  ASSERT_EQ(o, nil);
}

TEST(ScopedNSObjectTestARC, ScopedNSObject) {
  base::scoped_nsobject<NSObject> p1([[NSObject alloc] init]);
  @autoreleasepool {
    EXPECT_TRUE(p1.get());
    EXPECT_TRUE(p1.get());
  }
  EXPECT_EQ(1, GetRetainCount(p1));
  EXPECT_EQ(1, GetRetainCount(p1));
  base::scoped_nsobject<NSObject> p2(p1);
  @autoreleasepool {
    EXPECT_EQ(p1.get(), p2.get());
  }
  EXPECT_EQ(2, GetRetainCount(p1));
  p2.reset();
  EXPECT_EQ(nil, p2.get());
  EXPECT_EQ(1, GetRetainCount(p1));
  {
    base::scoped_nsobject<NSObject> p3 = p1;
    @autoreleasepool {
      EXPECT_EQ(p1.get(), p3.get());
    }
    EXPECT_EQ(2, GetRetainCount(p1));
    @autoreleasepool {
      p3 = p1;
      EXPECT_EQ(p1.get(), p3.get());
    }
    EXPECT_EQ(2, GetRetainCount(p1));
  }
  EXPECT_EQ(1, GetRetainCount(p1));
  base::scoped_nsobject<NSObject> p4;
  @autoreleasepool {
    p4 = base::scoped_nsobject<NSObject>(p1.get());
  }
  EXPECT_EQ(2, GetRetainCount(p1));
  @autoreleasepool {
    EXPECT_TRUE(p1 == p1.get());
    EXPECT_TRUE(p1 == p1);
    EXPECT_FALSE(p1 != p1);
    EXPECT_FALSE(p1 != p1.get());
  }
  base::scoped_nsobject<NSObject> p5([[NSObject alloc] init]);
  @autoreleasepool {
    EXPECT_TRUE(p1 != p5);
    EXPECT_TRUE(p1 != p5.get());
    EXPECT_FALSE(p1 == p5);
    EXPECT_FALSE(p1 == p5.get());
  }

  base::scoped_nsobject<NSObject> p6 = p1;
  EXPECT_EQ(3, GetRetainCount(p6));
  @autoreleasepool {
    p6.autorelease();
    EXPECT_EQ(nil, p6.get());
  }
  EXPECT_EQ(2, GetRetainCount(p1));
}

TEST(ScopedNSObjectTestARC, ScopedNSObjectInContainer) {
  base::scoped_nsobject<id> p([[NSObject alloc] init]);
  @autoreleasepool {
    EXPECT_TRUE(p.get());
  }
  EXPECT_EQ(1, GetRetainCount(p));
  @autoreleasepool {
    std::vector<base::scoped_nsobject<id>> objects;
    objects.push_back(p);
    EXPECT_EQ(2, GetRetainCount(p));
    @autoreleasepool {
      EXPECT_EQ(p.get(), objects[0].get());
    }
    objects.push_back(base::scoped_nsobject<id>([[NSObject alloc] init]));
    @autoreleasepool {
      EXPECT_TRUE(objects[1].get());
    }
    EXPECT_EQ(1, GetRetainCount(objects[1]));
  }
  EXPECT_EQ(1, GetRetainCount(p));
}

TEST(ScopedNSObjectTestARC, ScopedNSObjectFreeFunctions) {
  base::scoped_nsobject<id> p1([[NSObject alloc] init]);
  id o1 = p1.get();
  EXPECT_TRUE(o1 == p1);
  EXPECT_FALSE(o1 != p1);
  base::scoped_nsobject<id> p2([[NSObject alloc] init]);
  EXPECT_TRUE(o1 != p2);
  EXPECT_FALSE(o1 == p2);
  id o2 = p2.get();
  swap(p1, p2);
  EXPECT_EQ(o2, p1.get());
  EXPECT_EQ(o1, p2.get());
}

TEST(ScopedNSObjectTestARC, ResetWithAnotherScopedNSObject) {
  // This test uses __unsafe_unretained because it holds raw pointers to do
  // comparisons of them.

  base::scoped_nsobject<id> p1([[NSObject alloc] init]);
  id __unsafe_unretained o1;
  @autoreleasepool {
    o1 = p1.get();
  }

  id __unsafe_unretained o2;
  {
    base::scoped_nsobject<id> p2([[NSObject alloc] init]);
    @autoreleasepool {
      o2 = p2.get();
    }
    p1.reset(p2);
    EXPECT_EQ(2u, GetRetainCount(p1));
  }

  @autoreleasepool {
    EXPECT_NE(o1, p1.get());
    EXPECT_EQ(o2, p1.get());
    EXPECT_NE(p1.get(), nil);
  }

  EXPECT_EQ(1u, GetRetainCount(p1));
}

}  // namespace
