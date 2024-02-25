// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/apple/scoped_nsautorelease_pool.h"

#include "base/dcheck_is_on.h"

#if DCHECK_IS_ON()
#import <Foundation/Foundation.h>

#include "base/debug/crash_logging.h"
#include "base/debug/stack_trace.h"
#include "base/immediate_crash.h"
#include "base/strings/sys_string_conversions.h"
#endif

// Note that this uses the direct runtime interface to the autorelease pool.
// https://clang.llvm.org/docs/AutomaticReferenceCounting.html#runtime-support
// This is so this can work when compiled for ARC.
extern "C" {
void* objc_autoreleasePoolPush(void);
void objc_autoreleasePoolPop(void* pool);
}

namespace base::apple {

#if DCHECK_IS_ON()
namespace {

using BlockReturningStackTrace = debug::StackTrace (^)();

// Because //base is not allowed to define Objective-C classes, which would be
// the most reasonable way to wrap a C++ object like base::debug::StackTrace, do
// it in a much more absurd, yet not completely unreasonable, way.
//
// This uses a default argument for the stack trace so that the creation of the
// stack trace is attributed to the parent function.
BlockReturningStackTrace MakeBlockReturningStackTrace(
    debug::StackTrace stack_trace = debug::StackTrace()) {
  // Return a block that references the stack trace. That will cause a copy of
  // the stack trace to be made by the block, and because blocks are effectively
  // Objective-C objects, they can be used in the NSThread thread dictionary.
  return ^() {
    return stack_trace;
  };
}

// For each NSThread, maintain an array of stack traces, one for the state of
// the stack for each invocation of an autorelease pool push. Even though one is
// allowed to clear out an entire stack of autorelease pools by releasing one
// near the bottom, because the stack abstraction is mapped to C++ classes, this
// cannot be allowed.
NSMutableArray<BlockReturningStackTrace>* GetLevelStackTraces() {
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
  [GetLevelStackTraces() addObject:MakeBlockReturningStackTrace()];
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
    NSString* current_stack = SysUTF8ToNSString(debug::StackTrace().ToString());
    NSLog(@"Pop:\n%@", current_stack);
    [GetLevelStackTraces()
        enumerateObjectsWithOptions:NSEnumerationReverse
                         usingBlock:^(BlockReturningStackTrace obj,
                                      NSUInteger idx, BOOL* stop) {
                           NSLog(@"====================");
                           NSLog(@"Autorelease pool level %lu was pushed:\n%@",
                                 idx + 1, SysUTF8ToNSString(obj().ToString()));
                         }];
    // Assume an interactive use of Chromium where crashing immediately is
    // desirable, and die. When investigating a failing automated test that dies
    // here, remove these crash keys and call to ImmediateCrash() to reveal
    // where the abandoned ScopedNSAutoreleasePool was expected to be released.
    SCOPED_CRASH_KEY_NUMBER("ScopedNSAutoreleasePool", "currentlevel", level_);
    SCOPED_CRASH_KEY_NUMBER("ScopedNSAutoreleasePool", "levelcount",
                            level_count);
    SCOPED_CRASH_KEY_STRING1024("ScopedNSAutoreleasePool", "currentstack",
                                SysNSStringToUTF8(current_stack));
    SCOPED_CRASH_KEY_STRING1024("ScopedNSAutoreleasePool", "recentstack",
                                GetLevelStackTraces().lastObject().ToString());
    ImmediateCrash();
  }
  [GetLevelStackTraces() removeLastObject];
#endif
  objc_autoreleasePoolPop(autorelease_pool_);
}

}  // namespace base::apple
