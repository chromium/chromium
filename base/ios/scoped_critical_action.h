// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_IOS_SCOPED_CRITICAL_ACTION_H_
#define BASE_IOS_SCOPED_CRITICAL_ACTION_H_

#include "base/memory/ref_counted.h"
#include "base/strings/string_piece_forward.h"
#include "base/synchronization/lock.h"

namespace base {
namespace ios {

// This class attempts to allow the application to continue to run for a period
// of time after it transitions to the background. The construction of an
// instance of this class marks the beginning of a task that needs background
// running time when the application is moved to the background and the
// destruction marks the end of such a task.
//
// Note there is no guarantee that the task will continue to finish when the
// application is moved to the background.
//
// This class should be used at times where leaving a task unfinished might be
// detrimental to user experience. For example, it should be used to ensure that
// the application has enough time to save important data or at least attempt to
// save such data.
class ScopedCriticalAction {
 public:
  ScopedCriticalAction(StringPiece task_name);

  ScopedCriticalAction(const ScopedCriticalAction&) = delete;
  ScopedCriticalAction& operator=(const ScopedCriticalAction&) = delete;

  ~ScopedCriticalAction();

 private:
  // Core logic; ScopedCriticalAction should not be reference counted so
  // that it follows the normal pattern of stack-allocating ScopedFoo objects,
  // but the expiration handler needs to have a reference counted object to
  // refer to.
  class Core : public base::RefCountedThreadSafe<Core> {
   public:
    Core();

    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;

    // Informs the OS that the background task has started. This is a
    // static method to ensure that the instance has a non-zero refcount.
    // |task_name| is used by the OS to log any leaked background tasks.
    static void StartBackgroundTask(scoped_refptr<Core> core,
                                    StringPiece task_name);
    // Informs the OS that the background task has completed. This is a
    // static method to ensure that the instance has a non-zero refcount.
    static void EndBackgroundTask(scoped_refptr<Core> core);

   private:
    friend base::RefCountedThreadSafe<Core>;
    ~Core();

    // |UIBackgroundTaskIdentifier| returned by
    // |beginBackgroundTaskWithName:expirationHandler:| when marking the
    // beginning of a long-running background task. It is defined as a uint64_t
    // instead of a |UIBackgroundTaskIdentifier| so this class can be used in
    // .cc files.
    uint64_t background_task_id_ GUARDED_BY(background_task_id_lock_);
    Lock background_task_id_lock_;
  };

  // The instance of the core that drives the background task.
  scoped_refptr<Core> core_;
};

}  // namespace ios
}  // namespace base

#endif  // BASE_IOS_SCOPED_CRITICAL_ACTION_H_
