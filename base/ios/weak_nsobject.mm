// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ios/weak_nsobject.h"

#include "base/mac/scoped_nsobject.h"

namespace {
// The key needed by objc_setAssociatedObject.
char sentinelObserverKey_;
}

namespace base {

WeakContainer::WeakContainer(id object) : object_(object) {}

WeakContainer::~WeakContainer() {}

}  // namespace base

@interface CRBWeakNSProtocolSentinel ()
// Container to notify on dealloc.
@property(readonly, assign) scoped_refptr<base::WeakContainer> container;
// Designed initializer.
- (id)initWithContainer:(scoped_refptr<base::WeakContainer>)container;
@end

@implementation CRBWeakNSProtocolSentinel

@synthesize container = _container;

+ (scoped_refptr<base::WeakContainer>)containerForObject:(id)object {
  if (object == nil)
    return nullptr;
  // The autoreleasepool is needed here as the call to objc_getAssociatedObject
  // returns an autoreleased object which is better released sooner than later.
  @autoreleasepool {
    CRBWeakNSProtocolSentinel* sentinel =
        objc_getAssociatedObject(object, &sentinelObserverKey_);
    if (!sentinel) {
      base::scoped_nsobject<CRBWeakNSProtocolSentinel> newSentinel(
          [[CRBWeakNSProtocolSentinel alloc]
              initWithContainer:new base::WeakContainer(object)]);
      sentinel = newSentinel;
      objc_setAssociatedObject(object, &sentinelObserverKey_, sentinel,
                               OBJC_ASSOCIATION_RETAIN);
      // The retain count is 2. One retain is due to the alloc, the other to the
      // association with the weak object.
      DCHECK_EQ(2u, [sentinel retainCount]);
    }
    return [sentinel container];
  }
}

- (id)initWithContainer:(scoped_refptr<base::WeakContainer>)container {
  DCHECK(container.get());
  self = [super init];
  if (self)
    _container = container;
  return self;
}

- (void)dealloc {
  self.container->nullify();
  [super dealloc];
}

@end
