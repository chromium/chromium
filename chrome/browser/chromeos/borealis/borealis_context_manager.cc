// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/borealis/borealis_context_manager.h"

namespace borealis {

BorealisContextManager::Result::Result(const BorealisContext* ctx)
    : result_(BorealisStartupResult::kSuccess), failure_reason_(), ctx_(ctx) {
  DCHECK(ctx);
}

BorealisContextManager::Result::Result(BorealisStartupResult result,
                                       std::string failure_reason)
    : result_(result),
      failure_reason_(std::move(failure_reason)),
      ctx_(nullptr) {
  DCHECK(result != BorealisStartupResult::kSuccess);
}

BorealisContextManager::Result::~Result() = default;

bool BorealisContextManager::Result::Ok() const {
  return result_ == BorealisStartupResult::kSuccess;
}

BorealisStartupResult BorealisContextManager::Result::Failure() const {
  DCHECK(!Ok());
  return result_;
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
