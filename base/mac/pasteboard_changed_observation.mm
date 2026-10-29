// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/mac/pasteboard_changed_observation.h"

#include <AppKit/AppKit.h>
#include <dispatch/dispatch.h>
#include <objc/runtime.h>

#include <string_view>
#include <utility>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/thread_annotations.h"

// There is no notification API on macOS for changes to the pasteboard (unlike
// on iOS where there is UIPasteboardChangedNotification). However...
//
// Each app has a cache of pasteboard contents, and the pasteboard daemon keeps
// track of those caches and which of them are stale. When a pasteboard copy
// happens in one app, the daemon determines which apps have stale pasteboard
// caches, and sends them an XPC message ("com.apple.pboard.invalidate-cache")
// to clear their caches. (Breakpoint on __CFPasteboardHandleMessageFromDaemon
// to see this in action.)
//
// This invalidation eventually trickles down to the cache class,
// _CFPasteboardCache, whose -setChangeCount: method is called. If the
// pasteboard is set within the app then the cache's change count is set to a
// valid value, but if an invalidation message comes from the daemon, the change
// count is set to -1, and the daemon will not send any further invalidation
// messages for copies outside the app, as the cache is now not dirty.
//
// Therefore, intercept -setChangeCount: messages sent to _CFPasteboardCache.
// After notifying all interested parties, access the NSPasteboard changeCount
// property. This will mark the cache as being dirty again from the daemon's
// perspective, and then the next pasteboard change from outside of the app will
// result in another cache invalidation message. As long as the changeCount
// continues to be accessed in response to the pasteboard daemon's request to
// clean the cache, the cache will continue to be dirty and the app will
// continue to be called back for pasteboard changes outside the app.
//
// Playing this invalidation game is a bit silly, but until an API is provided
// to do this (requested in FB18125171), as the wise Rick Astley says, "we know
// the game, and we're gonna play it."

namespace base {

namespace {

RepeatingClosureList& GetCallbackList() {
  static NoDestructor<RepeatingClosureList> callbacks;
  return *callbacks;
}

// Captured at registration. The swizzled method can run after the main thread's
// default handle is gone at shutdown; posts to this runner are then dropped.
struct MainThreadTaskRunnerStorage {
  Lock lock;
  scoped_refptr<SingleThreadTaskRunner> task_runner GUARDED_BY(lock);
};

MainThreadTaskRunnerStorage& GetMainThreadTaskRunnerStorage() {
  static NoDestructor<MainThreadTaskRunnerStorage> storage;
  return *storage;
}

scoped_refptr<SingleThreadTaskRunner> GetMainThreadTaskRunner() {
  MainThreadTaskRunnerStorage& storage = GetMainThreadTaskRunnerStorage();
  AutoLock lock(storage.lock);
  return storage.task_runner;
}

void SetMainThreadTaskRunner(
    scoped_refptr<SingleThreadTaskRunner> task_runner) {
  MainThreadTaskRunnerStorage& storage = GetMainThreadTaskRunnerStorage();
  AutoLock lock(storage.lock);
  storage.task_runner = std::move(task_runner);
}

bool SwizzleInternalClass() {
  Class pasteboard_cache_class = objc_getClass("_CFPasteboardCache");
  if (!pasteboard_cache_class) {
    return false;
  }

  SEL selector = @selector(setChangeCount:);
  Method method = class_getInstanceMethod(pasteboard_cache_class, selector);
  if (!method) {
    return false;
  }

  std::string_view type_encoding(method_getTypeEncoding(method));
  if (type_encoding != "v20@0:8i16") {
    return false;
  }

  using ImpFunctionType = void (*)(id, SEL, int);
  static ImpFunctionType g_old_imp;

  IMP new_imp =
      imp_implementationWithBlock(^(id object_self, int change_count) {
        // Hop to the main thread, as the cache is processed on an internal
        // CFPasteboard dispatch queue.
        GetMainThreadTaskRunner()->PostTask(
            FROM_HERE, base::BindOnce([] {
              GetCallbackList().Notify();

              // Dirty the app's pasteboard cache to ensure an invalidation
              // callback for the next pasteboard change that occurs in other
              // apps.
              std::ignore = NSPasteboard.generalPasteboard.changeCount;
            }));

        g_old_imp(object_self, selector, change_count);
      });

  g_old_imp = reinterpret_cast<ImpFunctionType>(
      method_setImplementation(method, new_imp));

  return !!g_old_imp;
}

}  // namespace

CallbackListSubscription RegisterPasteboardChangedCallback(
    RepeatingClosure callback) {
  CHECK(base::SingleThreadTaskRunner::GetMainThreadDefault()
            ->BelongsToCurrentThread());

  SetMainThreadTaskRunner(SingleThreadTaskRunner::GetMainThreadDefault());

  static bool swizzle_internal_class [[maybe_unused]] = SwizzleInternalClass();
  // Intentionally DCHECK so that in the field it doesn't rely on that specific
  // internal class (as listening for pasteboard changes isn't critical), but
  // that it's noisy and noticeable on the beta bots so that if these internals
  // ever change it will be noticed.
  DCHECK(swizzle_internal_class);

  return GetCallbackList().Add(callback);
}

}  // namespace base
