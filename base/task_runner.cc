// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task_runner.h"

#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/record_replay.h"
#include "base/threading/post_task_and_reply_impl.h"

#include <dlfcn.h>

static void (*gRecordReplayRegisterPointerFn)(const char*, void*);

static bool RecordReplayRegisterPointer(const char* name, void* ptr) {
  if (!gRecordReplayRegisterPointerFn) {
    void* fnptr = dlsym(RTLD_DEFAULT, "RecordReplayRegisterPointerWithName");
    if (!fnptr) {
      return false;
    }
    gRecordReplayRegisterPointerFn = reinterpret_cast<void(*)(const char*, void*)>(fnptr);
  }

  gRecordReplayRegisterPointerFn(name, ptr);
  return true;
}

static void (*gRecordReplayUnregisterPointerFn)(void*);

static void RecordReplayUnregisterPointer(void* ptr) {
  if (!gRecordReplayUnregisterPointerFn) {
    void* fnptr = dlsym(RTLD_DEFAULT, "RecordReplayUnregisterPointer");
    if (!fnptr) {
      return;
    }
    gRecordReplayUnregisterPointerFn = reinterpret_cast<void(*)(void*)>(fnptr);
  }

  gRecordReplayUnregisterPointerFn(ptr);
}

namespace base {

namespace {

// TODO(akalin): There's only one other implementation of
// PostTaskAndReplyImpl in post_task.cc.  Investigate whether it'll be
// possible to merge the two.
class PostTaskAndReplyTaskRunner : public internal::PostTaskAndReplyImpl {
 public:
  explicit PostTaskAndReplyTaskRunner(TaskRunner* destination);

 private:
  bool PostTask(const Location& from_here, OnceClosure task) override;

  // Non-owning.
  TaskRunner* destination_;
};

PostTaskAndReplyTaskRunner::PostTaskAndReplyTaskRunner(
    TaskRunner* destination) : destination_(destination) {
  DCHECK(destination_);
}

bool PostTaskAndReplyTaskRunner::PostTask(const Location& from_here,
                                          OnceClosure task) {
  return destination_->PostTask(from_here, std::move(task));
}

}  // namespace

bool TaskRunner::PostTask(const Location& from_here, OnceClosure task) {
  return PostDelayedTask(from_here, std::move(task), base::TimeDelta());
}

bool TaskRunner::PostTaskAndReply(const Location& from_here,
                                  OnceClosure task,
                                  OnceClosure reply) {
  return PostTaskAndReplyTaskRunner(this).PostTaskAndReply(
      from_here, std::move(task), std::move(reply));
}

TaskRunner::TaskRunner() {
  // Pointer registration is needed for sorting in various places.
  if (RecordReplayRegisterPointer("TaskRunner", this)) {
    // Leak task runners when recording/replaying, as they can be destroyed at
    // non-deterministic points and this is simpler than ordering uses of their
    // reference count.
    AddRef();
  }
}

TaskRunner::~TaskRunner() {
  RecordReplayUnregisterPointer(this);
}

void TaskRunner::OnDestruct() const {
  delete this;
}

void TaskRunnerTraits::Destruct(const TaskRunner* task_runner) {
  task_runner->OnDestruct();
}

}  // namespace base
