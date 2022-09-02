// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/user_metrics.h"

#include <stddef.h>

#include <vector>

#include "base/bind.h"
#include "base/debug/dump_without_crashing.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/metrics/metrics_hashes.h"
#include "base/rand_util.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"

namespace base {
namespace {

LazyInstance<std::vector<ActionCallback>>::DestructorAtExit g_callbacks =
    LAZY_INSTANCE_INITIALIZER;
LazyInstance<scoped_refptr<SingleThreadTaskRunner>>::DestructorAtExit
    g_task_runner = LAZY_INSTANCE_INITIALIZER;

}  // namespace

void RecordAction(const UserMetricsAction& action) {
  RecordComputedAction(action.str_);
}

void RecordComputedAction(const std::string& action) {
  RecordComputedActionAt(action, TimeTicks::Now());
}

void RecordComputedActionSince(const std::string& action,
                               TimeDelta time_since) {
  RecordComputedActionAt(action, TimeTicks::Now() - time_since);
}

void RecordComputedActionAt(const std::string& action, TimeTicks action_time) {
  // This is used to detect two unknown user actions (see crbug.com/1346741),
  // and will be deleted soon.
  // Only report 1 out 250 events to make sure a reasonable amount of crash
  // reports are created.
  uint64_t hashed_action = base::HashMetricName(action) & 0x7fffffff;
  if (hashed_action == 73600854 || hashed_action == 1198301198) {
    if (base::RandInt(0, 249) == 0) {
      base::debug::DumpWithoutCrashing();
    }
  }
  TRACE_EVENT_INSTANT1("ui", "UserEvent", TRACE_EVENT_SCOPE_GLOBAL, "action",
                       action);
  if (!g_task_runner.Get()) {
    DCHECK(g_callbacks.Get().empty());
    return;
  }

  if (!g_task_runner.Get()->BelongsToCurrentThread()) {
    g_task_runner.Get()->PostTask(
        FROM_HERE, BindOnce(&RecordComputedActionAt, action, action_time));
    return;
  }

  for (const ActionCallback& callback : g_callbacks.Get()) {
    callback.Run(action, action_time);
  }
}

void AddActionCallback(const ActionCallback& callback) {
  // Only allow adding a callback if the task runner is set.
  DCHECK(g_task_runner.Get());
  DCHECK(g_task_runner.Get()->BelongsToCurrentThread());
  g_callbacks.Get().push_back(callback);
}

void RemoveActionCallback(const ActionCallback& callback) {
  DCHECK(g_task_runner.Get());
  DCHECK(g_task_runner.Get()->BelongsToCurrentThread());
  std::vector<ActionCallback>* callbacks = g_callbacks.Pointer();
  const auto i = std::find(callbacks->begin(), callbacks->end(), callback);
  if (i != callbacks->end())
    callbacks->erase(i);
}

void SetRecordActionTaskRunner(
    scoped_refptr<SingleThreadTaskRunner> task_runner) {
  DCHECK(task_runner->BelongsToCurrentThread());
  DCHECK(!g_task_runner.Get() || g_task_runner.Get()->BelongsToCurrentThread());
  g_task_runner.Get() = task_runner;
}

scoped_refptr<SingleThreadTaskRunner> GetRecordActionTaskRunner() {
  if (g_task_runner.IsCreated())
    return g_task_runner.Get();
  return nullptr;
}

}  // namespace base
