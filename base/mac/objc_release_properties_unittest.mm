// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/objc_release_properties.h"
#include "base/containers/contains.h"

#include "testing/gtest/include/gtest/gtest.h"

#import <objc/runtime.h>

// "When I'm alone, I count myself."
//   --Count von Count, http://www.youtube.com/watch?v=FKzszqa9WA4

namespace {

// The number of CountVonCounts outstanding.
int ah_ah_ah;

// NumberHolder exists to exercise the property attribute string parser by
// providing a named struct and an anonymous union.
struct NumberHolder {
  union {
    long long sixty_four;
    int thirty_two;
    short sixteen;
    char eight;
  } what;
  enum { SIXTY_FOUR, THIRTY_TWO, SIXTEEN, EIGHT } how;
};

}  // namespace

@interface CountVonCount : NSObject<NSCopying>

+ (CountVonCount*)countVonCount;

@end  // @interface CountVonCount

@implementation CountVonCount

+ (CountVonCount*)countVonCount {
  return [[[CountVonCount alloc] init] autorelease];
}

- (instancetype)init {
  ++ah_ah_ah;
  return [super init];
}

- (void)dealloc {
  --ah_ah_ah;
  [super dealloc];
}

- (id)copyWithZone:(NSZone*)zone {
  return [[CountVonCount allocWithZone:zone] init];
}

@end  // @implementation CountVonCount

@interface ObjCPropertyTestBase : NSObject {
 @private
  CountVonCount* _baseCvcRetain;
  CountVonCount* _baseCvcCopy;
  CountVonCount* _baseCvcAssign;
  CountVonCount* _baseCvcNotProperty;
  CountVonCount* _baseCvcNil;
  CountVonCount* _baseCvcCustom;
  int _baseInt;
  double _baseDouble;
  void* _basePointer;
  NumberHolder _baseStruct;
}

@property(retain, nonatomic) CountVonCount* baseCvcRetain;
@property(copy, nonatomic) CountVonCount* baseCvcCopy;
@property(assign, nonatomic) CountVonCount* baseCvcAssign;
@property(retain, nonatomic) CountVonCount* baseCvcNil;
@property(retain, nonatomic, getter=baseCustom, setter=setBaseCustom:)
    CountVonCount* baseCvcCustom;
@property(readonly, retain, nonatomic) CountVonCount* baseCvcReadOnly;
@property(retain, nonatomic) CountVonCount* baseCvcDynamic;
@property(assign, nonatomic) int baseInt;
@property(assign, nonatomic) double baseDouble;
@property(assign, nonatomic) void* basePointer;
@property(assign, nonatomic) NumberHolder baseStruct;

- (void)setBaseCvcNotProperty:(CountVonCount*)cvc;

@end  // @interface ObjCPropertyTestBase

@implementation ObjCPropertyTestBase

@synthesize baseCvcRetain = _baseCvcRetain;
@synthesize baseCvcCopy = _baseCvcCopy;
@synthesize baseCvcAssign = _baseCvcAssign;
@synthesize baseCvcNil = _baseCvcNil;
@synthesize baseCvcCustom = _baseCvcCustom;
@synthesize baseCvcReadOnly = _baseCvcReadOnly;
@dynamic baseCvcDynamic;
@synthesize baseInt = _baseInt;
@synthesize baseDouble = _baseDouble;
@synthesize basePointer = _basePointer;
@synthesize baseStruct = _baseStruct;

- (void)dealloc {
  [_baseCvcNotProperty release];
  base::mac::ReleaseProperties(self);
  [super dealloc];
}

- (void)setBaseCvcNotProperty:(CountVonCount*)cvc {
  if (cvc != _baseCvcNotProperty) {
    [_baseCvcNotProperty release];
    _baseCvcNotProperty = [cvc retain];
  }
}

- (void)setBaseCvcReadOnlyProperty:(CountVonCount*)cvc {
  if (cvc != _baseCvcReadOnly) {
    [_baseCvcReadOnly release];
    _baseCvcReadOnly = [cvc retain];
  }
}

@end  // @implementation ObjCPropertyTestBase

@protocol ObjCPropertyTestProtocol

@property(retain, nonatomic) CountVonCount* protoCvcRetain;
@property(copy, nonatomic) CountVonCount* protoCvcCopy;
@property(assign, nonatomic) CountVonCount* protoCvcAssign;
@property(retain, nonatomic) CountVonCount* protoCvcNil;
@property(retain, nonatomic, getter=protoCustom, setter=setProtoCustom:)
    CountVonCount* protoCvcCustom;
@property(retain, nonatomic) CountVonCount* protoCvcDynamic;
@property(assign, nonatomic) int protoInt;
@property(assign, nonatomic) double protoDouble;
@property(assign, nonatomic) void* protoPointer;
@property(assign, nonatomic) NumberHolder protoStruct;

@end  // @protocol ObjCPropertyTestProtocol

// @protocol(NSObject) declares some (copy, readonly) properties (superclass,
// description, debugDescription, and hash), but we're not expected to release
// them. The current implementation only releases properties backed by instance
// variables, and this makes sure that doesn't change in a breaking way.
@interface ObjCPropertyTestDerived
    : ObjCPropertyTestBase<ObjCPropertyTestProtocol, NSObject> {
 @private
  CountVonCount* _derivedCvcRetain;
  CountVonCount* _derivedCvcCopy;
  CountVonCount* _derivedCvcAssign;
  CountVonCount* _derivedCvcNotProperty;
  CountVonCount* _derivedCvcNil;
  CountVonCount* _derivedCvcCustom;
  int _derivedInt;
  double _derivedDouble;
  void* _derivedPointer;
  NumberHolder _derivedStruct;

  CountVonCount* _protoCvcRetain;
  CountVonCount* _protoCvcCopy;
  CountVonCount* _protoCvcAssign;
  CountVonCount* _protoCvcNil;
  CountVonCount* _protoCvcCustom;
  int _protoInt;
  double _protoDouble;
  void* _protoPointer;
  NumberHolder _protoStruct;
}

@property(retain, nonatomic) CountVonCount* derivedCvcRetain;
@property(copy, nonatomic) CountVonCount* derivedCvcCopy;
@property(assign, nonatomic) CountVonCount* derivedCvcAssign;
@property(retain, nonatomic) CountVonCount* derivedCvcNil;
@property(retain, nonatomic, getter=derivedCustom, setter=setDerivedCustom:)
    CountVonCount* derivedCvcCustom;
@property(retain, nonatomic) CountVonCount* derivedCvcDynamic;
@property(assign, nonatomic) int derivedInt;
@property(assign, nonatomic) double derivedDouble;
@property(assign, nonatomic) void* derivedPointer;
@property(assign, nonatomic) NumberHolder derivedStruct;

- (void)setDerivedCvcNotProperty:(CountVonCount*)cvc;

@end  // @interface ObjCPropertyTestDerived

@implementation ObjCPropertyTestDerived

@synthesize derivedCvcRetain = _derivedCvcRetain;
@synthesize derivedCvcCopy = _derivedCvcCopy;
@synthesize derivedCvcAssign = _derivedCvcAssign;
@synthesize derivedCvcNil = _derivedCvcNil;
@synthesize derivedCvcCustom = _derivedCvcCustom;
@dynamic derivedCvcDynamic;
@synthesize derivedInt = _derivedInt;
@synthesize derivedDouble = _derivedDouble;
@synthesize derivedPointer = _derivedPointer;
@synthesize derivedStruct = _derivedStruct;

@synthesize protoCvcRetain = _protoCvcRetain;
@synthesize protoCvcCopy = _protoCvcCopy;
@synthesize protoCvcAssign = _protoCvcAssign;
@synthesize protoCvcNil = _protoCvcNil;
@synthesize protoCvcCustom = _protoCvcCustom;
@dynamic protoCvcDynamic;
@synthesize protoInt = _protoInt;
@synthesize protoDouble = _protoDouble;
@synthesize protoPointer = _protoPointer;
@synthesize protoStruct = _protoStruct;

+ (BOOL)resolveInstanceMethod:(SEL)sel {
  static const std::vector<SEL> dynamicMethods {
    @selector(baseCvcDynamic), @selector(derivedCvcDynamic),
        @selector(protoCvcDynamic),
  };
  if (!base::Contains(dynamicMethods, sel)) {
    return NO;
  }
  id (*imp)() = []() -> id { return nil; };
  class_addMethod([self class], sel, reinterpret_cast<IMP>(imp), "@@:");
  return YES;
}

- (void)dealloc {
  base::mac::ReleaseProperties(self);
  [_derivedCvcNotProperty release];
  [super dealloc];
}

- (void)setDerivedCvcNotProperty:(CountVonCount*)cvc {
  if (cvc != _derivedCvcNotProperty) {
    [_derivedCvcNotProperty release];
    _derivedCvcNotProperty = [cvc retain];
  }
}

@end  // @implementation ObjCPropertyTestDerived

@interface ObjcPropertyTestEmpty : NSObject
@end

@implementation ObjcPropertyTestEmpty

- (void)dealloc {
  base::mac::ReleaseProperties(self);
  [super dealloc];
}

@end  // @implementation ObjcPropertyTestEmpty

namespace {

TEST(ObjCReleasePropertiesTest, SesameStreet) {
  ObjCPropertyTestDerived* test_object = [[ObjCPropertyTestDerived alloc] init];

  // Assure a clean slate.
  EXPECT_EQ(0, ah_ah_ah);
  EXPECT_EQ(1U, [test_object retainCount]);

  CountVonCount* baseAssign = [[CountVonCount alloc] init];
  CountVonCount* derivedAssign = [[CountVonCount alloc] init];
  CountVonCount* protoAssign = [[CountVonCount alloc] init];

  // Make sure that worked before things get more involved.
  EXPECT_EQ(3, ah_ah_ah);

  @autoreleasepool {
    test_object.baseCvcRetain = [CountVonCount countVonCount];
    test_object.baseCvcCopy = [CountVonCount countVonCount];
    test_object.baseCvcAssign = baseAssign;
    test_object.baseCvcCustom = [CountVonCount countVonCount];
    [test_object setBaseCvcReadOnlyProperty:[CountVonCount countVonCount]];
    [test_object setBaseCvcNotProperty:[CountVonCount countVonCount]];

    // That added 5 objects, plus 1 more that was copied.
    EXPECT_EQ(9, ah_ah_ah);

    test_object.derivedCvcRetain = [CountVonCount countVonCount];
    test_object.derivedCvcCopy = [CountVonCount countVonCount];
    test_object.derivedCvcAssign = derivedAssign;
    test_object.derivedCvcCustom = [CountVonCount countVonCount];
    [test_object setDerivedCvcNotProperty:[CountVonCount countVonCount]];

    // That added 4 objects, plus 1 more that was copied.
    EXPECT_EQ(14, ah_ah_ah);

    test_object.protoCvcRetain = [CountVonCount countVonCount];
    test_object.protoCvcCopy = [CountVonCount countVonCount];
    test_object.protoCvcAssign = protoAssign;
    test_object.protoCvcCustom = [CountVonCount countVonCount];

    // That added 3 objects, plus 1 more that was copied.
    EXPECT_EQ(18, ah_ah_ah);
  }

  // Now that the autorelease pool has been popped, the 3 objects that were
  // copied when placed into the test object will have been deallocated.
  EXPECT_EQ(15, ah_ah_ah);

  // Make sure that the setters wo/rk and have the expected semantics.
  test_object.baseCvcRetain = nil;
  test_object.baseCvcCopy = nil;
  test_object.baseCvcAssign = nil;
  test_object.baseCvcCustom = nil;
  test_object.derivedCvcRetain = nil;
  test_object.derivedCvcCopy = nil;
  test_object.derivedCvcAssign = nil;
  test_object.derivedCvcCustom = nil;
  test_object.protoCvcRetain = nil;
  test_object.protoCvcCopy = nil;
  test_object.protoCvcAssign = nil;
  test_object.protoCvcCustom = nil;

  // The CountVonCounts marked "retain" and "copy" should have been
  // deallocated. Those marked assign should not have been. The only ones that
  // should exist now are the ones marked "assign", the ones held in
  // non-property instance variables, and the ones held in properties marked
  // readonly.
  EXPECT_EQ(6, ah_ah_ah);

  @autoreleasepool {
    // Put things back to how they were.
    test_object.baseCvcRetain = [CountVonCount countVonCount];
    test_object.baseCvcCopy = [CountVonCount countVonCount];
    test_object.baseCvcAssign = baseAssign;
    test_object.baseCvcCustom = [CountVonCount countVonCount];
    test_object.derivedCvcRetain = [CountVonCount countVonCount];
    test_object.derivedCvcCopy = [CountVonCount countVonCount];
    test_object.derivedCvcAssign = derivedAssign;
    test_object.derivedCvcCustom = [CountVonCount countVonCount];
    test_object.protoCvcRetain = [CountVonCount countVonCount];
    test_object.protoCvcCopy = [CountVonCount countVonCount];
    test_object.protoCvcAssign = protoAssign;
    test_object.protoCvcCustom = [CountVonCount countVonCount];

    // 9 more CountVonCounts, 3 of which were copied.
    EXPECT_EQ(18, ah_ah_ah);
  }

  // Now that the autorelease pool has been popped, the 3 copies are gone.
  EXPECT_EQ(15, ah_ah_ah);

  // Releasing the test object should get rid of everything that it owns.
  [test_object release];

  // base::mac::ReleaseProperties(self) should have released all of the
  // CountVonCounts associated with properties marked "retain" or "copy". The
  // -dealloc methods in each should have released the single non-property
  // objects in each. Only the CountVonCounts assigned to the properties marked
  // "assign" should remain.
  EXPECT_EQ(3, ah_ah_ah);

  [baseAssign release];
  [derivedAssign release];
  [protoAssign release];

  // Zero! Zero counts! Ah, ah, ah.
  EXPECT_EQ(0, ah_ah_ah);
}

TEST(ObjCReleasePropertiesTest, EmptyObject) {
  // Test that ReleaseProperties doesn't do anything unexpected to a class
  // with no properties.
  [[[ObjcPropertyTestEmpty alloc] init] release];
}

}  // namespace
