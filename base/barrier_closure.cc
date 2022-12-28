// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/barrier_closure.h"

#include <utility>

#include "base/atomic_ref_count.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"

#include "base/record_replay.h"

namespace base {
namespace {

// Maintains state for a BarrierClosure.
class BarrierInfo {
 public:
  BarrierInfo(size_t num_callbacks_left, OnceClosure done_closure);
  void Run();

 private:
  AtomicRefCount num_callbacks_left_;
  OnceClosure done_closure_;
  int record_replay_ordered_lock_;
};

BarrierInfo::BarrierInfo(size_t num_callbacks, OnceClosure done_closure)
    : num_callbacks_left_(checked_cast<int>(num_callbacks)),
      done_closure_(std::move(done_closure)),
      record_replay_ordered_lock_(recordreplay::CreateOrderedLock("BarrierInfo")) {}

void BarrierInfo::Run() {
  DCHECK(!num_callbacks_left_.IsZero());
  bool done;
  {
    recordreplay::AutoOrderedLock ordered(record_replay_ordered_lock_);
    done = !num_callbacks_left_.Decrement();
  }
  if (done)
    std::move(done_closure_).Run();
}

void ShouldNeverRun() {
  CHECK(false);
}

}  // namespace

RepeatingClosure BarrierClosure(size_t num_callbacks_left,
                                OnceClosure done_closure) {
  if (num_callbacks_left == 0) {
    std::move(done_closure).Run();
    return BindRepeating(&ShouldNeverRun);
  }

  return BindRepeating(&BarrierInfo::Run,
                       std::make_unique<BarrierInfo>(num_callbacks_left,
                                                     std::move(done_closure)));
}

}  // namespace base
