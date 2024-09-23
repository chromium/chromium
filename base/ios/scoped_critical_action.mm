// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ios/scoped_critical_action.h"

#import <UIKit/UIKit.h>
#include <float.h>

#include <atomic>
#include <string_view>

#include "base/ios/ios_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/lock.h"

namespace base::ios {

BASE_FEATURE(kScopedCriticalActionSkipOnShutdown,
             "ScopedCriticalActionSkipOnShutdown",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

constexpr base::TimeDelta kMaxTaskReuseDelay = base::Seconds(3);

// Used for unit-testing only.
std::atomic<int> g_num_active_background_tasks_for_test{0};

}  // namespace

ScopedCriticalAction::ScopedCriticalAction(std::string_view task_name)
    : task_handle_(ActiveBackgroundTaskCache::GetInstance()
                       ->EnsureBackgroundTaskExistsWithName(task_name)) {}

ScopedCriticalAction::~ScopedCriticalAction() {
  ActiveBackgroundTaskCache::GetInstance()->ReleaseHandle(task_handle_);
}

// static
void ScopedCriticalAction::ApplicationWillTerminate() {
  if (base::FeatureList::IsEnabled(kScopedCriticalActionSkipOnShutdown)) {
    ActiveBackgroundTaskCache::GetInstance()->ApplicationWillTerminate();
  }
}

// static
void ScopedCriticalAction::ClearNumActiveBackgroundTasksForTest() {
  g_num_active_background_tasks_for_test.store(0);
}

// static
void ScopedCriticalAction::ResetApplicationWillTerminateForTest() {
  ActiveBackgroundTaskCache::GetInstance()
      ->ResetApplicationWillTerminateForTest();  // IN-TEST
}

// static
int ScopedCriticalAction::GetNumActiveBackgroundTasksForTest() {
  return g_num_active_background_tasks_for_test.load();
}

ScopedCriticalAction::Core::Core()
    : background_task_id_(UIBackgroundTaskInvalid) {}

ScopedCriticalAction::Core::~Core() {
  DCHECK_EQ(background_task_id_, UIBackgroundTaskInvalid);
}

// This implementation calls |beginBackgroundTaskWithName:expirationHandler:|
// when instantiated and |endBackgroundTask:| when destroyed, creating a scope
// whose execution will continue (temporarily) even after the app is
// backgrounded.
// static
void ScopedCriticalAction::Core::StartBackgroundTask(
    scoped_refptr<Core> core,
    std::string_view task_name) {
  UIApplication* application = UIApplication.sharedApplication;
  if (!application) {
    return;
  }

  AutoLock lock_scope(core->background_task_id_lock_);
  if (core->background_task_id_ != UIBackgroundTaskInvalid) {
    // Already started.
    return;
  }

  NSString* task_string =
      !task_name.empty() ? base::SysUTF8ToNSString(task_name) : nil;
  core->background_task_id_ = [application
      beginBackgroundTaskWithName:task_string
                expirationHandler:^{
                  DLOG(WARNING)
                      << "Background task with name <"
                      << base::SysNSStringToUTF8(task_string) << "> and with "
                      << "id " << core->background_task_id_ << " expired.";
                  // Note if |endBackgroundTask:| is not called for each task
                  // before time expires, the system kills the application.
                  EndBackgroundTask(core);
                }];

  if (core->background_task_id_ == UIBackgroundTaskInvalid) {
    DLOG(WARNING) << "beginBackgroundTaskWithName:<" << task_name << "> "
                  << "expirationHandler: returned an invalid ID";
  } else {
    VLOG(3) << "Beginning background task <" << task_name << "> with id "
            << core->background_task_id_;
    g_num_active_background_tasks_for_test.fetch_add(1,
                                                     std::memory_order_relaxed);
  }
}

// static
void ScopedCriticalAction::Core::EndBackgroundTask(scoped_refptr<Core> core) {
  UIBackgroundTaskIdentifier task_id;
  {
    AutoLock lock_scope(core->background_task_id_lock_);
    if (core->background_task_id_ == UIBackgroundTaskInvalid) {
      // Never started successfully or already ended.
      return;
    }
    task_id =
        static_cast<UIBackgroundTaskIdentifier>(core->background_task_id_);
    core->background_task_id_ = UIBackgroundTaskInvalid;
  }

  VLOG(3) << "Ending background task with id " << task_id;
  [[UIApplication sharedApplication] endBackgroundTask:task_id];
  g_num_active_background_tasks_for_test.fetch_sub(1,
                                                   std::memory_order_relaxed);
}

ScopedCriticalAction::ActiveBackgroundTaskCache::InternalEntry::
    InternalEntry() = default;

ScopedCriticalAction::ActiveBackgroundTaskCache::InternalEntry::
    ~InternalEntry() = default;

ScopedCriticalAction::ActiveBackgroundTaskCache::InternalEntry::InternalEntry(
    InternalEntry&&) = default;

ScopedCriticalAction::ActiveBackgroundTaskCache::InternalEntry&
ScopedCriticalAction::ActiveBackgroundTaskCache::InternalEntry::operator=(
    InternalEntry&&) = default;

// static
ScopedCriticalAction::ActiveBackgroundTaskCache*
ScopedCriticalAction::ActiveBackgroundTaskCache::GetInstance() {
  return base::Singleton<
      ActiveBackgroundTaskCache,
      base::LeakySingletonTraits<ActiveBackgroundTaskCache>>::get();
}

ScopedCriticalAction::ActiveBackgroundTaskCache::ActiveBackgroundTaskCache() =
    default;

ScopedCriticalAction::ActiveBackgroundTaskCache::~ActiveBackgroundTaskCache() =
    default;

ScopedCriticalAction::ActiveBackgroundTaskCache::Handle ScopedCriticalAction::
    ActiveBackgroundTaskCache::EnsureBackgroundTaskExistsWithName(
        std::string_view task_name) {
  const base::TimeTicks now = base::TimeTicks::Now();
  const base::TimeTicks min_reusable_time = now - kMaxTaskReuseDelay;
  NameAndTime min_reusable_key{task_name, min_reusable_time};

  Handle handle;
  {
    AutoLock lock_scope(entries_map_lock_);
    auto lower_it = entries_map_.lower_bound(min_reusable_key);

    if (lower_it != entries_map_.end() && lower_it->first.first == task_name) {
      // A reusable Core instance exists, with the same name and created
      // recently enough to warrant reuse.
      DCHECK_GE(lower_it->first.second, min_reusable_time);
      handle = lower_it;
    } else {
      // No reusable entry exists, so a new entry needs to be created.
      auto it = entries_map_.emplace_hint(
          lower_it, NameAndTime{std::move(min_reusable_key.first), now},
          InternalEntry{});
      DCHECK_EQ(it->first.second, now);
      DCHECK(!it->second.core);
      handle = it;
      handle->second.core = MakeRefCounted<Core>();
    }

    // This guarantees a non-zero counter and hence the deletion of this map
    // entry during this function body, even after the lock is released.
    ++handle->second.num_active_handles;
  }

  // If this call didn't newly-create a Core instance, the call to
  // StartBackgroundTask() is almost certainly (barring race conditions)
  // unnecessary. It is however harmless to invoke it twice.
  if (!application_is_terminating_) {
    Core::StartBackgroundTask(handle->second.core, task_name);
  }

  return handle;
}

void ScopedCriticalAction::ActiveBackgroundTaskCache::ReleaseHandle(
    Handle handle) {
  scoped_refptr<Core> background_task_to_end;

  {
    AutoLock lock_scope(entries_map_lock_);
    --handle->second.num_active_handles;
    if (handle->second.num_active_handles == 0) {
      // Move to |background_task_to_end| so the global lock is released before
      // invoking EndBackgroundTask() which is expensive.
      background_task_to_end = std::move(handle->second.core);
      entries_map_.erase(handle);
    }
  }

  // Note that at this point another, since the global lock was released,
  // another task could have started with the same name, but this harmless.
  if (background_task_to_end != nullptr) {
    Core::EndBackgroundTask(std::move(background_task_to_end));
  }
}

void ScopedCriticalAction::ActiveBackgroundTaskCache::
    ApplicationWillTerminate() {
  application_is_terminating_ = true;
}

void ScopedCriticalAction::ActiveBackgroundTaskCache::
    ResetApplicationWillTerminateForTest() {
  application_is_terminating_ = false;
}

}  // namespace base::ios
