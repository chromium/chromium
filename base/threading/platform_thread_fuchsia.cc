// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#include <fidl/fuchsia.media/cpp/fidl.h>
#include <lib/fdio/directory.h>
#include <lib/sys/cpp/component_context.h>
#include <pthread.h>
#include <sched.h>
#include <zircon/syscalls.h>

#include <mutex>
#include <string_view>

#include "base/fuchsia/fuchsia_component_connect.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/scheduler.h"
#include "base/no_destructor.h"
#include "base/threading/platform_thread_internal_posix.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/threading/thread_local_storage.h"

namespace base {

namespace {

fidl::SyncClient<fuchsia_media::ProfileProvider> ConnectProfileProvider() {
  auto profile_provider_client_end =
      base::fuchsia_component::Connect<fuchsia_media::ProfileProvider>();
  if (profile_provider_client_end.is_error()) {
    LOG(ERROR) << base::FidlConnectionErrorMessage(profile_provider_client_end);
    return {};
  }
  return fidl::SyncClient(std::move(profile_provider_client_end.value()));
}

// Sets the current thread to the given scheduling role, optionally including
// hints about the workload period and max CPU runtime (capacity * period) in
// that period.
// TODO(crbug.com/42050523): Migrate to the new
// fuchsia.scheduler.ProfileProvider API when available.
void SetThreadRole(std::string_view role_name,
                   TimeDelta period = {},
                   float capacity = 0.0f) {
  DCHECK_GE(capacity, 0.0);
  DCHECK_LE(capacity, 1.0);

  static const base::NoDestructor<
      fidl::SyncClient<fuchsia_media::ProfileProvider>>
      profile_provider(ConnectProfileProvider());

  if (!profile_provider->is_valid()) {
    return;
  }

  zx::thread dup_thread;
  zx_status_t status =
      zx::thread::self()->duplicate(ZX_RIGHT_SAME_RIGHTS, &dup_thread);
  ZX_CHECK(status == ZX_OK, status) << "zx_object_duplicate";

  std::string role_selector{role_name};
  auto result = (*profile_provider)
                    ->RegisterHandlerWithCapacity(
                        {{.thread_handle = std::move(dup_thread),
                          .name = role_selector,
                          .period = period.ToZxDuration(),
                          .capacity = capacity}});
  if (result.is_error()) {
    ZX_DLOG(ERROR, result.error_value().status())
        << "Failed call to RegisterHandlerWithCapacity";
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
  zx_status_t status =
      zx::thread::self()->set_property(ZX_PROP_NAME, name.data(), name.size());
  DCHECK_EQ(status, ZX_OK);

  SetNameCommon(name);
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
    case ThreadType::kDefault:
      SetThreadRole("chromium.base.threading.default");

      break;

    case ThreadType::kBackground:
      SetThreadRole("chromium.base.threading.background");
      break;

    case ThreadType::kUtility:
      SetThreadRole("chromium.base.threading.utility");
      break;

    case ThreadType::kResourceEfficient:
      SetThreadRole("chromium.base.threading.resource-efficient");
      break;

    case ThreadType::kDisplayCritical:
      SetThreadRole("chromium.base.threading.display", kDisplaySchedulingPeriod,
                    kDisplaySchedulingCapacity);
      break;

    case ThreadType::kRealtimeAudio:
      SetThreadRole("chromium.base.threading.realtime-audio",
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
    case ThreadType::kUtility:
    case ThreadType::kResourceEfficient:
    case ThreadType::kDefault:
      return ThreadPriorityForTest::kNormal;
    case ThreadType::kDisplayCritical:
      return ThreadPriorityForTest::kDisplay;
    case ThreadType::kRealtimeAudio:
      return ThreadPriorityForTest::kRealtimeAudio;
  }
}

}  // namespace base
