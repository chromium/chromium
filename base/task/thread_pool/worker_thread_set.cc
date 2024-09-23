// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/worker_thread_set.h"

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/task/thread_pool/worker_thread.h"

namespace base::internal {

bool WorkerThreadSet::Compare::operator()(const WorkerThread* a,
                                          const WorkerThread* b) const {
  return a->sequence_num() < b->sequence_num();
}

WorkerThreadSet::WorkerThreadSet() = default;

WorkerThreadSet::~WorkerThreadSet() = default;

void WorkerThreadSet::Insert(WorkerThread* worker) {
  DCHECK(!Contains(worker)) << "WorkerThread already on stack";
  auto old_first = set_.begin();
  set_.insert(worker);
  if (worker != *set_.begin())
    worker->BeginUnusedPeriod();
  else if (old_first != set_.end())
    (*old_first)->BeginUnusedPeriod();
}

WorkerThread* WorkerThreadSet::Take() {
  if (IsEmpty())
    return nullptr;
  WorkerThread* const worker = *set_.begin();
  set_.erase(set_.begin());
  if (!IsEmpty())
    (*set_.begin())->EndUnusedPeriod();
  return worker;
}

WorkerThread* WorkerThreadSet::Peek() const {
  if (IsEmpty())
    return nullptr;
  return *set_.begin();
}

bool WorkerThreadSet::Contains(const WorkerThread* worker) const {
  return set_.count(const_cast<WorkerThread*>(worker)) > 0;
}

void WorkerThreadSet::Remove(const WorkerThread* worker) {
  DCHECK(!IsEmpty());
  DCHECK_NE(worker, *set_.begin());
  auto it = set_.find(const_cast<WorkerThread*>(worker));
  CHECK(it != set_.end(), base::NotFatalUntil::M125);
  DCHECK_NE(TimeTicks(), (*it)->GetLastUsedTime());
  set_.erase(it);
}

}  // namespace base::internal
