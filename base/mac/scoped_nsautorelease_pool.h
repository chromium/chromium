// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MAC_SCOPED_NSAUTORELEASE_POOL_H_
#define BASE_MAC_SCOPED_NSAUTORELEASE_POOL_H_

#include "base/base_export.h"
#include "base/macros.h"

#if defined(__OBJC__)
@class NSAutoreleasePool;
#else  // __OBJC__
class NSAutoreleasePool;
#endif  // __OBJC__

namespace base {
namespace mac {

// ScopedNSAutoreleasePool allocates an NSAutoreleasePool when instantiated and
// sends it a -drain message when destroyed.  This allows an autorelease pool to
// be maintained in ordinary C++ code without bringing in any direct Objective-C
// dependency.
//
// Use only in C++ code; use @autoreleasepool in Obj-C(++) code.
// ScopedNSAutoreleasePool 在实例化时分配一个 NSAutoreleasePool 并在销毁时发送一个 -drain 消息。
// 这允许在普通 C++ 代码中维护自动释放池，而不会引入任何直接的 Objective-C 依赖项。
// 仅在 C++ 代码中使用； 在 Obj-C(++) 代码中使用 @autoreleasepool。

class BASE_EXPORT ScopedNSAutoreleasePool {
 public:
  ScopedNSAutoreleasePool();

  ScopedNSAutoreleasePool(const ScopedNSAutoreleasePool&) = delete;
  ScopedNSAutoreleasePool& operator=(const ScopedNSAutoreleasePool&) = delete;

  ~ScopedNSAutoreleasePool();

  // Clear out the pool in case its position on the stack causes it to be
  // alive for long periods of time (such as the entire length of the app).
  // Only use then when you're certain the items currently in the pool are
  // no longer needed.
  void Recycle();
 private:
  NSAutoreleasePool* autorelease_pool_;
};

}  // namespace mac
}  // namespace base

#endif  // BASE_MAC_SCOPED_NSAUTORELEASE_POOL_H_
