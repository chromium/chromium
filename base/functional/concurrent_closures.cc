// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/concurrent_closures.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"

namespace base {

ConcurrentClosures::ConcurrentClosures() {
  auto info_owner = std::make_unique<Info>();
  info_ = info_owner.get();
  info_run_closure_ = BindRepeating(&Info::Run, std::move(info_owner));
}
ConcurrentClosures::~ConcurrentClosures() = default;

OnceClosure ConcurrentClosures::CreateClosure() {
  CHECK(info_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(info_->sequence_checker_);
  ++info_->pending_;
  return info_run_closure_;
}

void ConcurrentClosures::Done(OnceClosure done_closure,
                              const Location& location) && {
  CHECK(info_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(info_->sequence_checker_);
  info_->done_closure_ = BindPostTask(SequencedTaskRunner::GetCurrentDefault(),
                                      std::move(done_closure), location);
  if (info_->pending_ == 0u) {
    std::move(info_->done_closure_).Run();
  }
  info_ = nullptr;
}

ConcurrentClosures::Info::Info() = default;

ConcurrentClosures::Info::~Info() = default;

void ConcurrentClosures::Info::Run() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_GT(pending_, 0u);
  --pending_;
  if (done_closure_ && pending_ == 0u) {
    SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(done_closure_));
  }
}

}  // namespace base
