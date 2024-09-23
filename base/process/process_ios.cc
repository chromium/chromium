// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process.h"

#include "base/threading/thread_restrictions.h"

namespace base {

namespace {

static Process::TerminateCallback g_terminate_callback = nullptr;
static Process::WaitForExitCallback g_wait_for_exit_callback = nullptr;

}  // namespace

bool WaitForExitWithTimeoutImpl(base::ProcessHandle handle,
                                int* exit_code,
                                base::TimeDelta timeout);

void Process::SetTerminationHooks(TerminateCallback terminate_callback,
                                  WaitForExitCallback wait_callback) {
  CHECK(!g_terminate_callback);
  CHECK(!g_wait_for_exit_callback);
  g_terminate_callback = terminate_callback;
  g_wait_for_exit_callback = wait_callback;
}

#if TARGET_OS_SIMULATOR
void Process::SetIsContentProcess() {
  content_process_ = true;
}

bool Process::IsContentProcess() const {
  return content_process_;
}
#endif

bool Process::Terminate(int exit_code, bool wait) const {
  // exit_code isn't supportable.
  DCHECK(IsValid());
  CHECK_GT(process_, 0);
#if TARGET_OS_SIMULATOR
  if (!content_process_) {
    return TerminateInternal(exit_code, wait);
  }
#endif
  CHECK(g_terminate_callback);
  return (*g_terminate_callback)(process_);
}

bool Process::WaitForExitWithTimeout(TimeDelta timeout, int* exit_code) const {
  if (!timeout.is_zero()) {
    // Assert that this thread is allowed to wait below. This intentionally
    // doesn't use ScopedBlockingCallWithBaseSyncPrimitives because the process
    // being waited upon tends to itself be using the CPU and considering this
    // thread non-busy causes more issue than it fixes: http://crbug.com/905788
    internal::AssertBaseSyncPrimitivesAllowed();
  }

#if TARGET_OS_SIMULATOR
  if (!content_process_) {
    return WaitForExitWithTimeoutImpl(Handle(), exit_code, timeout);
  }
#endif
  return (*g_wait_for_exit_callback)(process_, exit_code, timeout);
}

}  // namespace base
