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
bool PlatformThread::CanChangeThreadType(ThreadType from, ThreadType to) {
  return from == to || to == ThreadType::kDisplayCritical ||
         to == ThreadType::kRealtimeAudio;
}

namespace internal {

void SetCurrentThreadTypeImpl(ThreadType thread_type,
                              MessagePumpType pump_type_hint) {
  switch (thread_type) {
    case ThreadType::kBackground:
    case ThreadType::kResourceEfficient:
    case ThreadType::kDefault:
    case ThreadType::kCompositing:
      break;

    case ThreadType::kDisplayCritical:
      ScheduleAsMediaThread("chromium.base.threading.display",
                            kDisplaySchedulingPeriod,
                            kDisplaySchedulingCapacity);
      break;

    case ThreadType::kRealtimeAudio:
      ScheduleAsMediaThread("chromium.base.threading.realtime-audio",
                            kAudioSchedulingPeriod, kAudioSchedulingCapacity);
      break;
  }
}

}  // namespace internal

// static
ThreadPriorityForTest PlatformThread::GetCurrentThreadPriorityForTest() {
  // Fuchsia doesn't provide a way to get the current thread's priority.
  // Use ThreadType stored in TLS as a proxy.
  const ThreadType thread_type = PlatformThread::GetCurrentThreadType();
  switch (thread_type) {
    case ThreadType::kBackground:
    case ThreadType::kResourceEfficient:
    case ThreadType::kDefault:
    case ThreadType::kCompositing:
      return ThreadPriorityForTest::kNormal;
    case ThreadType::kDisplayCritical:
      return ThreadPriorityForTest::kDisplay;
    case ThreadType::kRealtimeAudio:
      return ThreadPriorityForTest::kRealtimeAudio;
  }
}

}  // namespace base
