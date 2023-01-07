// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_waitable_event.h"

#include <utility>

#include "build/build_config.h"

namespace base {

TestWaitableEvent::TestWaitableEvent(ResetPolicy reset_policy,
                                     InitialState initial_state)
    : WaitableEvent(reset_policy, initial_state) {
  // Pretending this is only used while idle ensures this WaitableEvent is not
  // instantiating a ScopedBlockingCallWithBaseSyncPrimitives in Wait(). In
  // other words, test logic is considered "idle" work (not part of the tested
  // logic).
  declare_only_used_while_idle();
}

#if BUILDFLAG(IS_WIN)
TestWaitableEvent::TestWaitableEvent(win::ScopedHandle event_handle)
    : WaitableEvent(std::move(event_handle)) {
  declare_only_used_while_idle();
}
#endif

}  // namespace base
