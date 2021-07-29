// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#include <pthread.h>
#include <sched.h>
#include <zircon/syscalls.h>

#include <fuchsia/media/cpp/fidl.h>
#include <lib/fdio/directory.h>
#include <lib/sys/cpp/component_context.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/scheduler.h"
#include "base/no_destructor.h"
#include "base/threading/platform_thread_internal_posix.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/threading/thread_local_storage.h"

namespace base {

namespace {

fuchsia::media::ProfileProviderSyncPtr ConnectProfileProvider() {
  fuchsia::media::ProfileProviderSyncPtr profile_provider;
  base::ComponentContextForProcess()->svc()->Connect(
      profile_provider.NewRequest());
  return profile_provider;
}

void ScheduleAsMediaThread(StringPiece name, TimeDelta period, float capacity) {
  DCHECK(!period.is_zero());
  DCHECK_GT(capacity, 0.0);
  DCHECK_LT(capacity, 1.0);

  static const base::NoDestructor<fuchsia::media::ProfileProviderSyncPtr>
      profile_provider(ConnectProfileProvider());

  zx::thread dup_thread;
  zx_status_t status =
      zx::thread::self()->duplicate(ZX_RIGHT_SAME_RIGHTS, &dup_thread);
  ZX_CHECK(status == ZX_OK, status) << "zx_object_duplicate";

  int64_t out_period, out_capacity;
  status = (*profile_provider)
               ->RegisterHandlerWithCapacity(
                   std::move(dup_thread), std::string(name),
                   period.ToZxDuration(), capacity, &out_period, &out_capacity);

  if (status != ZX_OK) {
    ZX_LOG(WARNING, status)
        << "Failed to register a realtime thread. Is "
           "fuchsia.media.ProfileProvider in the component sandbox?";
  }
}

// Return ThreadLocalStorage slot used to store priority of the current thread.
// The value is stored as an integer value converted to a pointer. 1 is added to
// the integer value in order to distinguish the case when the TLS slot is not
// initialized.
base::ThreadLocalStorage::Slot* GetThreadPriorityTlsSlot() {
  static base::NoDestructor<base::ThreadLocalStorage::Slot> tls_slot;
  return tls_slot.get();
}

void SaveThreadPriorityToTls(ThreadPriority priority) {
  GetThreadPriorityTlsSlot()->Set(
      reinterpret_cast<void*>(static_cast<uintptr_t>(priority) + 1));
}

ThreadPriority GetThreadPriorityFromTls() {
  uintptr_t value =
      reinterpret_cast<uintptr_t>(GetThreadPriorityTlsSlot()->Get());

  // Thread priority is set to NORMAL by default.
  if (value == 0)
    return ThreadPriority::NORMAL;

  return static_cast<ThreadPriority>(value - 1);
}

}  // namespace

void InitThreading() {}

void TerminateOnThread() {}

size_t GetDefaultThreadStackSize(const pthread_attr_t& attributes) {
  return 0;
}

// static
void PlatformThread::SetName(const std::string& name) {
  zx_status_t status = zx_object_set_property(CurrentId(), ZX_PROP_NAME,
                                              name.data(), name.size());
  DCHECK_EQ(status, ZX_OK);

  ThreadIdNameManager::GetInstance()->SetName(name);
}

// static
bool PlatformThread::CanIncreaseThreadPriority(ThreadPriority priority) {
  return true;
}

// static
void PlatformThread::SetCurrentThreadPriorityImpl(ThreadPriority priority) {
  switch (priority) {
    case ThreadPriority::BACKGROUND:
    case ThreadPriority::NORMAL:
      DCHECK(GetThreadPriorityFromTls() != ThreadPriority::DISPLAY &&
             GetThreadPriorityFromTls() != ThreadPriority::REALTIME_AUDIO);
      break;

    case ThreadPriority::DISPLAY:
      ScheduleAsMediaThread("chromium.base.threading.display",
                            kDisplaySchedulingPeriod, kAudioSchedulingCapacity);
      break;

    case ThreadPriority::REALTIME_AUDIO:
      ScheduleAsMediaThread("chromium.base.threading.realtime-audio",
                            kAudioSchedulingPeriod, kDisplaySchedulingCapacity);
      break;
  }

  SaveThreadPriorityToTls(priority);
}

// static
ThreadPriority PlatformThread::GetCurrentThreadPriority() {
  return GetThreadPriorityFromTls();
}

}  // namespace base
