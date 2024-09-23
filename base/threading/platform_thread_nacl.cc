// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Description: These are stubs for NACL.

#include "base/threading/platform_thread.h"

namespace base {
namespace internal {

bool CanSetThreadTypeToRealtimeAudio() {
  return false;
}

bool SetCurrentThreadTypeForPlatform(ThreadType thread_type,
                                MessagePumpType pump_type_hint) {
  return false;
}

std::optional<ThreadPriorityForTest>
GetCurrentThreadPriorityForPlatformForTest() {
  return std::nullopt;
}
}  // namespace internal

// static
void PlatformThreadBase::SetName(const std::string& name) {
  SetNameCommon(name);
}

}  // namespace base