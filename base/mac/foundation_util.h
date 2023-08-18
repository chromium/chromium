// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_FOUNDATION_UTIL_H_
#define BASE_MAC_FOUNDATION_UTIL_H_

#include "base/apple/foundation_util.h"

#if defined(__OBJC__)
#import <Foundation/Foundation.h>
#endif  // __OBJC__

// This is a forwarding header so that Crashpad can continue to build correctly
// until mini_chromium and then it are updated and rolled.

// TODO(https://crbug.com/1444927): Update mini_chromium, update Crashpad, roll
// Crashpad, and then delete this forwarding header.

namespace base::mac {

template <typename T>
T CFCast(const CFTypeRef& cf_val) {
  return base::apple::CFCast<T>(cf_val);
}

template <typename T>
T CFCastStrict(const CFTypeRef& cf_val) {
  return base::apple::CFCastStrict<T>(cf_val);
}

#if defined(__OBJC__)

template <typename T>
T* ObjCCast(id objc_val) {
  return base::apple::ObjCCast<T>(objc_val);
}

#endif  // __OBJC__

}  // namespace base::mac

#endif  // BASE_MAC_FOUNDATION_UTIL_H_
