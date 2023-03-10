// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#include "base/no_destructor.h"
#include "base/task/current_thread.h"
#include "base/threading/thread_local_storage.h"

#if BUILDFLAG(IS_FUCHSIA)
#include "base/fuchsia/scheduler.h"
#endif

namespace base {

namespace {

// Returns ThreadLocalStorage slot used to store type of the current thread.
// The value is stored as an integer value converted to a pointer. 1 is added to
// the integer value in order to distinguish the case when the TLS slot is not
// initialized.
base::ThreadLocalStorage::Slot* GetThreadTypeTlsSlot() {
  static base::NoDestructor<base::ThreadLocalStorage::Slot> tls_slot;
  return tls_slot.get();
}

void SaveThreadTypeToTls(ThreadType thread_type) {
  GetThreadTypeTlsSlot()->Set(
      reinterpret_cast<void*>(static_cast<uintptr_t>(thread_type) + 1));
}

ThreadType GetThreadTypeFromTls() {
  uintptr_t value = reinterpret_cast<uintptr_t>(GetThreadTypeTlsSlot()->Get());

  // Thread type is set to kNormal by default.
  if (value == 0)
    return ThreadType::kDefault;

  DCHECK_LE(value - 1, static_cast<uintptr_t>(ThreadType::kMaxValue));
  return static_cast<ThreadType>(value - 1);
}

}  // namespace

// static
void PlatformThread::SetCurrentThreadType(ThreadType thread_type) {
  MessagePumpType message_pump_type = MessagePumpType::DEFAULT;
  if (CurrentIOThread::IsSet()) {
    message_pump_type = MessagePumpType::IO;
  }
#if !BUILDFLAG(IS_NACL)
  else if (CurrentUIThread::IsSet()) {
    message_pump_type = MessagePumpType::UI;
  }
#endif
  internal::SetCurrentThreadType(thread_type, message_pump_type);
}

// static
ThreadType PlatformThread::GetCurrentThreadType() {
  return GetThreadTypeFromTls();
}

// static
absl::optional<TimeDelta> PlatformThread::GetThreadLeewayOverride() {
#if BUILDFLAG(IS_FUCHSIA)
  // On Fuchsia, all audio threads run with the CPU scheduling profile that uses
  // an interval of |kAudioSchedulingPeriod|. Using the default leeway may lead
  // to some tasks posted to audio threads to be executed too late (see
  // http://crbug.com/1368858).
  if (GetCurrentThreadType() == ThreadType::kRealtimeAudio)
    return kAudioSchedulingPeriod;
#endif
  return absl::nullopt;
}

namespace internal {

void SetCurrentThreadType(ThreadType thread_type,
                          MessagePumpType pump_type_hint) {
  SetCurrentThreadTypeImpl(thread_type, pump_type_hint);
  SaveThreadTypeToTls(thread_type);
}

}  // namespace internal

}  // namespace base
