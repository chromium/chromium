// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/promise/finally_executor.h"

namespace base {
namespace internal {

FinallyExecutorCommon::FinallyExecutorCommon(
    internal::CallbackBase&& callback) noexcept
    : callback_(std::move(callback)) {}

FinallyExecutorCommon::~FinallyExecutorCommon() = default;

bool FinallyExecutorCommon::IsCancelled() const {
  return callback_.IsCancelled();
}

}  // namespace internal
}  // namespace base
