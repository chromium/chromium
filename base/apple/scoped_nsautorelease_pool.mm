// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/apple/scoped_nsautorelease_pool.h"

#include "base/dcheck_is_on.h"
#include "base/strings/sys_string_conversions.h"

#if DCHECK_IS_ON()
#import <Foundation/Foundation.h>

#include "base/debug/crash_logging.h"
#include "base/immediate_crash.h"
#endif

// Note that this uses the direct runtime interface to the autorelease pool.
// https://clang.llvm.org/docs/AutomaticReferenceCounting.html#runtime-support
// This is so this can work when compiled for ARC.
extern "C" {
void* objc_autoreleasePoolPush(void);
void objc_autoreleasePoolPop(void* pool);
}

#if DCHECK_IS_ON()
namespace {
// For each NSThread, maintain an array of stack traces (NSArray<NSString>), one
// for the state of the stack for each invocation of an autorelease pool push.
// Even though one is allowed to clear out an entire stack of autorelease pools
// by releasing one near the bottom, because the stack abstraction is mapped to
// C++ classes, this cannot be allowed.
NSMutableArray<NSArray<NSString*>*>* GetLevelStackTraces() {
  NSMutableArray* traces =
      NSThread.currentThread
          .threadDictionary[@"CrScopedNSAutoreleasePoolTraces"];
  if (traces) {
    return traces;
  }

  traces = [NSMutableArray array];
  NSThread.currentThread.threadDictionary[@"CrScopedNSAutoreleasePoolTraces"] =
      traces;
  return traces;
}
}  // namespace
#endif

namespace base::apple {

ScopedNSAutoreleasePool::ScopedNSAutoreleasePool() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PushImpl();
}

ScopedNSAutoreleasePool::~ScopedNSAutoreleasePool() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PopImpl();
}

void ScopedNSAutoreleasePool::Recycle() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Cycle the internal pool, allowing everything there to get cleaned up and
  // start anew.
  PopImpl();
  PushImpl();
}

void ScopedNSAutoreleasePool::PushImpl() {
#if DCHECK_IS_ON()
  [GetLevelStackTraces() addObject:NSThread.callStackSymbols];
  level_ = GetLevelStackTraces().count;
#endif
  autorelease_pool_ = objc_autoreleasePoolPush();
}

void ScopedNSAutoreleasePool::PopImpl() {
#if DCHECK_IS_ON()
  auto level_count = GetLevelStackTraces().count;
  if (level_ != level_count) {
    NSLog(@"Popping autorelease pool at level %lu while pools exist through "
          @"level %lu",
          level_, level_count);
    if (level_ < level_count) {
      NSLog(@"WARNING: This abandons ScopedNSAutoreleasePool objects which now "
            @"have no corresponding implementation.");
    } else {
      NSLog(@"ERROR: This is an abandoned ScopedNSAutoreleasePool that cannot "
            @"release; expect the autorelease machinery to crash.");
    }
    NSLog(@"====================");
    NSString* current_stack =
        [NSThread.callStackSymbols componentsJoinedByString:@"\n"];
    NSLog(@"Pop:\n%@", current_stack);
    [GetLevelStackTraces()
        enumerateObjectsWithOptions:NSEnumerationReverse
                         usingBlock:^(NSArray<NSString*>* obj, NSUInteger idx,
                                      BOOL* stop) {
                           NSLog(@"====================");
                           NSLog(@"Autorelease pool level %lu was pushed:\n%@",
                                 idx + 1, [obj componentsJoinedByString:@"\n"]);
                         }];
    // Assume an interactive use of Chromium where crashing immediately is
    // desirable, and die. When investigating a failing automated test that dies
    // here, remove this call to ImmediateCrash(), to reveal where the abandoned
    // ScopedNSAutoreleasePool was expected to be released.
    SCOPED_CRASH_KEY_NUMBER("ScopedNSAutoreleasePool", "currentlevel", level_);
    SCOPED_CRASH_KEY_NUMBER("ScopedNSAutoreleasePool", "levelcount",
                            level_count);
    SCOPED_CRASH_KEY_STRING1024("ScopedNSAutoreleasePool", "currentstack",
                                SysNSStringToUTF8(current_stack));
    SCOPED_CRASH_KEY_STRING1024(
        "ScopedNSAutoreleasePool", "recentstack",
        SysNSStringToUTF8(
            [GetLevelStackTraces().lastObject componentsJoinedByString:@"\n"]));
    base::ImmediateCrash();
  }
  [GetLevelStackTraces() removeLastObject];
#endif
  objc_autoreleasePoolPop(autorelease_pool_);
}

}  // namespace base::apple
