// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(__has_feature) && __has_feature(objc_arc)
#error "ARC manages properties, so base::mac::ReleaseProperties isn't needed."
#endif

#ifndef BASE_MAC_OBJC_RELEASE_PROPERTIES_H_
#define BASE_MAC_OBJC_RELEASE_PROPERTIES_H_

#import <Foundation/Foundation.h>

#include "base/base_export.h"

// base::mac::ReleaseProperties(self) can be used in a class's -dealloc method
// to release all properties marked "retain" or "copy" and backed by instance
// variables. It only affects properties defined by the calling class, not
// sub/superclass properties.
//
// Example usage:
//
//     @interface AllaysIBF : NSObject
//
//     @property(retain, nonatomic) NSString* string;
//     @property(copy, nonatomic) NSMutableDictionary* dictionary;
//     @property(assign, nonatomic) IBFDelegate* delegate;
//
//     @end  // @interface AllaysIBF
//
//     @implementation AllaysIBF
//
//     - (void)dealloc {
//       base::mac::ReleaseProperties(self);
//       [super dealloc];
//     }
//
//     @end  // @implementation AllaysIBF
//
// self.string and self.dictionary will each be released, but self.delegate
// will not because it is marked "assign", not "retain" or "copy".
//
// Another approach would be to provide a base class to inherit from whose
// -dealloc walks the property lists of all subclasses to release their
// properties. Distant subclasses might not expect it and over-release their
// properties, so don't do that.

namespace base::mac {

namespace details {

BASE_EXPORT void ReleaseProperties(id, Class);

}  // namespace details

template <typename Self>
void ReleaseProperties(Self* self) {
  details::ReleaseProperties(self, [Self class]);
}

}  // namespace base::mac

#endif  // BASE_MAC_OBJC_RELEASE_PROPERTIES_H_
