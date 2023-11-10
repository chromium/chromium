// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/worker_thread_set.h"

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/task/thread_pool/worker_thread_waitable_event.h"

namespace base::internal {

bool WorkerThreadSet::Compare::operator()(
    const WorkerThreadWaitableEvent* a,
    const WorkerThreadWaitableEvent* b) const {
  return a->sequence_num() < b->sequence_num();
}

WorkerThreadSet::WorkerThreadSet() = default;

WorkerThreadSet::~WorkerThreadSet() = default;

void WorkerThreadSet::Insert(WorkerThreadWaitableEvent* worker) {
  DCHECK(!Contains(worker)) << "WorkerThread already on stack";
  auto old_first = set_.begin();
  set_.insert(worker);
  if (worker != *set_.begin())
    worker->BeginUnusedPeriod();
  else if (old_first != set_.end())
    (*old_first)->BeginUnusedPeriod();
}

WorkerThreadWaitableEvent* WorkerThreadSet::Take() {
  if (IsEmpty())
    return nullptr;
  WorkerThreadWaitableEvent* const worker = *set_.begin();
  set_.erase(set_.begin());
  if (!IsEmpty())
    (*set_.begin())->EndUnusedPeriod();
  return worker;
}

WorkerThreadWaitableEvent* WorkerThreadSet::Peek() const {
  if (IsEmpty())
    return nullptr;
  return *set_.begin();
}

bool WorkerThreadSet::Contains(const WorkerThreadWaitableEvent* worker) const {
  return set_.count(const_cast<WorkerThreadWaitableEvent*>(worker)) > 0;
}

void WorkerThreadSet::Remove(const WorkerThreadWaitableEvent* worker) {
  DCHECK(!IsEmpty());
  DCHECK_NE(worker, *set_.begin());
  auto it = set_.find(const_cast<WorkerThreadWaitableEvent*>(worker));
  DCHECK(it != set_.end());
  DCHECK_NE(TimeTicks(), (*it)->GetLastUsedTime());
  set_.erase(it);
}

}  // namespace base::internal
