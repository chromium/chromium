// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/service_thread.h"

#include "base/debug/alias.h"

namespace base {
namespace internal {

ServiceThread::ServiceThread() : Thread("ThreadPoolServiceThread") {}

NOINLINE void ServiceThread::Run(RunLoop* run_loop) {
  Thread::Run(run_loop);
  // Inhibit tail calls of Run and inhibit code folding.
  const int line_number = __LINE__;
  base::debug::Alias(&line_number);
}

}  // namespace internal
}  // namespace base
