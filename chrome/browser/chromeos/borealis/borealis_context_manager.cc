// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_context_manager.h"

namespace borealis {

BorealisContextManager::Result::Result(const BorealisContext* ctx)
    : status_(Status::kSuccess), failure_reason_(), ctx_(ctx) {
  DCHECK(ctx);
}

BorealisContextManager::Result::Result(Status status,
                                       std::string failure_reason)
    : status_(status),
      failure_reason_(std::move(failure_reason)),
      ctx_(nullptr) {
  DCHECK(status != Status::kSuccess);
}

BorealisContextManager::Result::~Result() = default;

bool BorealisContextManager::Result::Ok() const {
  return status_ == Status::kSuccess;
}

BorealisContextManager::Status BorealisContextManager::Result::Failure() const {
  DCHECK(!Ok());
  return status_;
}

const std::string& BorealisContextManager::Result::FailureReason() const {
  DCHECK(!Ok());
  return failure_reason_;
}

const BorealisContext& BorealisContextManager::Result::Success() const {
  DCHECK(Ok());
  return *ctx_;
}

}  // namespace borealis
