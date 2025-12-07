// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#include "base/task/current_thread.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/trace_event/trace_event.h"

#if BUILDFLAG(IS_FUCHSIA)
#include "base/fuchsia/scheduler.h"
#endif

namespace base {

namespace {

constinit thread_local ThreadType current_thread_type = ThreadType::kDefault;

}  // namespace

void PlatformThreadId::WriteIntoTrace(perfetto::TracedValue&& context) const {
  perfetto::WriteIntoTracedValue(std::move(context), value_);
}

// static
void PlatformThreadBase::SetCurrentThreadType(ThreadType thread_type) {
  MessagePumpType message_pump_type = MessagePumpType::DEFAULT;
  if (CurrentIOThread::IsSet()) {
    message_pump_type = MessagePumpType::IO;
  } else if (CurrentUIThread::IsSet()) {
    message_pump_type = MessagePumpType::UI;
  }
  CHECK_LE(thread_type, ThreadType::kMaxValue);
  internal::SetCurrentThreadTypeImpl(thread_type, message_pump_type);
  current_thread_type = thread_type;
}

// static
ThreadType PlatformThreadBase::GetCurrentThreadType() {
  return current_thread_type;
}

// static
std::optional<TimeDelta> PlatformThreadBase::GetThreadLeewayOverride() {
#if BUILDFLAG(IS_FUCHSIA)
  // On Fuchsia, all audio threads run with the CPU scheduling profile that uses
  // an interval of |kAudioSchedulingPeriod|. Using the default leeway may lead
  // to some tasks posted to audio threads to be executed too late (see
  // http://crbug.com/1368858).
  if (GetCurrentThreadType() == ThreadType::kRealtimeAudio) {
    return kAudioSchedulingPeriod;
  }
#endif
  return std::nullopt;
}

// static
void PlatformThreadBase::SetNameCommon(const std::string& name) {
  ThreadIdNameManager::GetInstance()->SetName(name);
}

namespace internal {

void RemoveThreadTypeOverride(
    const PlatformPriorityOverride& priority_override_handle) {
  RemoveThreadTypeOverrideImpl(priority_override_handle, current_thread_type);
}

}  // namespace internal
}  // namespace base
