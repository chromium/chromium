// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FUCHSIA_SCHEDULER_H_
#define BASE_FUCHSIA_SCHEDULER_H_

#include "base/base_export.h"
#include "base/time/time.h"

namespace base {

// Scheduling interval to use for realtime audio threads.
// TODO(crbug.com/42050308): Add scheduling period to Thread::Options and remove
// this constant.
constexpr TimeDelta kAudioSchedulingPeriod = Milliseconds(10);

// Controls how fuchsia.scheduler.RoleManager is used to apply thread roles.
enum class SchedulerRoles {
  // Do not connect to or call fuchsia.scheduler.RoleManager.
  kUnused,

  // Require fuchsia.scheduler.RoleManager to be connected (fatal on
  // framework/connection errors), and silently ignore missing role
  // definitions and other errors.
  kIgnoreMissing,

  // Require fuchsia.scheduler.RoleManager to be connected (fatal on
  // framework/connection errors), and log an ERROR on missing role
  // definitions and other errors.
  kErrorMissing,

  // Require both fuchsia.scheduler.RoleManager and all requested role
  // definitions to succeed (fatal on any error).
  kRequire,
};

// Sets the mode used to apply scheduler roles. Defaults to kUnused.
BASE_EXPORT void SetSchedulerRoles(SchedulerRoles roles);

// Returns the current scheduler role mode.
BASE_EXPORT SchedulerRoles GetSchedulerRoles();

}  // namespace base

#endif  // BASE_FUCHSIA_SCHEDULER_H_
