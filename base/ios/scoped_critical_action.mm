// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ios/scoped_critical_action.h"

#import <UIKit/UIKit.h>

#include <float.h>
#include "base/ios/ios_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/sys_string_conversions.h"
#include "base/synchronization/lock.h"

namespace base {
namespace ios {

ScopedCriticalAction::ScopedCriticalAction(StringPiece task_name)
    : core_(MakeRefCounted<ScopedCriticalAction::Core>()) {
  ScopedCriticalAction::Core::StartBackgroundTask(core_, task_name);
}

ScopedCriticalAction::~ScopedCriticalAction() {
  ScopedCriticalAction::Core::EndBackgroundTask(core_);
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
void ScopedCriticalAction::Core::StartBackgroundTask(scoped_refptr<Core> core,
                                                     StringPiece task_name) {
  UIApplication* application = [UIApplication sharedApplication];
  if (!application) {
    return;
  }

  AutoLock lock_scope(core->background_task_id_lock_);
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
  }
}

// static
void ScopedCriticalAction::Core::EndBackgroundTask(scoped_refptr<Core> core) {
  UIBackgroundTaskIdentifier task_id;
  {
    AutoLock lock_scope(core->background_task_id_lock_);
    if (core->background_task_id_ == UIBackgroundTaskInvalid) {
      return;
    }
    task_id = core->background_task_id_;
    core->background_task_id_ = UIBackgroundTaskInvalid;
  }

  VLOG(3) << "Ending background task with id " << task_id;
  [[UIApplication sharedApplication] endBackgroundTask:task_id];
}

}  // namespace ios
}  // namespace base
