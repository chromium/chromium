// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/execution_fence.h"

#include "base/check.h"
#include "base/not_fatal_until.h"
#include "base/task/thread_pool/thread_pool_instance.h"

namespace base {

ScopedThreadPoolExecutionFence::ScopedThreadPoolExecutionFence() {
  auto* thread_pool = ThreadPoolInstance::Get();
  CHECK(thread_pool, NotFatalUntil::M145);
  thread_pool->BeginFence();
}

ScopedThreadPoolExecutionFence::~ScopedThreadPoolExecutionFence() {
  auto* thread_pool = ThreadPoolInstance::Get();
  CHECK(thread_pool, NotFatalUntil::M145);
  thread_pool->EndFence();
}

ScopedBestEffortExecutionFence::ScopedBestEffortExecutionFence() {
  auto* thread_pool = ThreadPoolInstance::Get();
  CHECK(thread_pool, NotFatalUntil::M145);
  thread_pool->BeginBestEffortFence();
}

ScopedBestEffortExecutionFence::~ScopedBestEffortExecutionFence() {
  auto* thread_pool = ThreadPoolInstance::Get();
  CHECK(thread_pool, NotFatalUntil::M145);
  thread_pool->EndBestEffortFence();
}

}  // namespace base
