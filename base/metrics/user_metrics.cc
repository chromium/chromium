// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/user_metrics.h"

#include <stddef.h>

#include <vector>

#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/ranges/algorithm.h"
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
  const auto i = ranges::find(*callbacks, callback);
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
