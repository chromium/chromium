// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#include <fidl/fuchsia.scheduler/cpp/fidl.h>
#include <pthread.h>
#include <sched.h>

#include <atomic>
#include <string_view>

#include "base/fuchsia/fuchsia_component_connect.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/scheduler.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/threading/platform_thread_internal_posix.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/threading/thread_local_storage.h"

namespace base {

namespace {

std::atomic<SchedulerRoles> g_scheduler_roles{SchedulerRoles::kUnused};

fidl::SyncClient<fuchsia_scheduler::RoleManager> ConnectRoleManager() {
  auto client_end =
      base::fuchsia_component::Connect<fuchsia_scheduler::RoleManager>();
  if (client_end.is_error()) {
    LOG(FATAL) << "Failed to connect to fuchsia.scheduler.RoleManager: "
               << base::FidlConnectionErrorMessage(client_end);
  }
  return fidl::SyncClient(std::move(client_end.value()));
}

// Sets the current thread to the given scheduling role via
// fuchsia.scheduler.RoleManager.
void SetThreadRole(std::string_view role_name) {
  const SchedulerRoles roles = GetSchedulerRoles();
  if (roles == SchedulerRoles::kUnused) {
    return;
  }

  static const base::NoDestructor<
      fidl::SyncClient<fuchsia_scheduler::RoleManager>>
      role_manager(ConnectRoleManager());

  zx::thread dup_thread;
  zx_status_t status =
      zx::thread::self()->duplicate(ZX_RIGHT_SAME_RIGHTS, &dup_thread);
  ZX_CHECK(status == ZX_OK, status) << "zx_object_duplicate";

  fuchsia_scheduler::RoleManagerSetRoleRequest request;
  request.target(
      fuchsia_scheduler::RoleTarget::WithThread(std::move(dup_thread)));
  request.role(fuchsia_scheduler::RoleName(std::string(role_name)));

  auto result = (*role_manager)->SetRole(std::move(request));
  if (result.is_error()) {
    if (result.error_value().is_framework_error()) {
      LOG(FATAL) << "fuchsia.scheduler.RoleManager channel error while "
                 << "applying role '" << role_name
                 << "': " << result.error_value().FormatDescription();
    }
    switch (roles) {
      case SchedulerRoles::kUnused:
        NOTREACHED();
      case SchedulerRoles::kIgnoreMissing:
        // Missing role definitions and other errors are intentionally ignored.
        break;
      case SchedulerRoles::kErrorMissing:
        LOG(ERROR) << "Failed to apply scheduler role '" << role_name
                   << "': " << result.error_value().FormatDescription();
        break;
      case SchedulerRoles::kRequire:
        LOG(FATAL) << "Failed to apply scheduler role '" << role_name
                   << "': " << result.error_value().FormatDescription();
    }
  }
}

}  // namespace

void SetSchedulerRoles(SchedulerRoles roles) {
  g_scheduler_roles.store(roles, std::memory_order_relaxed);
}

SchedulerRoles GetSchedulerRoles() {
  return g_scheduler_roles.load(std::memory_order_relaxed);
}

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
  return from == to || to == ThreadType::kPresentation ||
         to == ThreadType::kAudioProcessing || to == ThreadType::kRealtimeAudio;
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

    case ThreadType::kPresentation:
    case ThreadType::kAudioProcessing:
      SetThreadRole("chromium.base.threading.display");
      break;

    case ThreadType::kRealtimeAudio:
      SetThreadRole("chromium.base.threading.realtime-audio");
      break;
  }
}

PlatformPriorityOverride SetThreadTypeOverride(
    PlatformThreadHandle thread_handle,
    ThreadType thread_type) {
  return false;
}

void RemoveThreadTypeOverride(
    PlatformThreadHandle thread_handle,
    const PlatformPriorityOverride& priority_override_handle,
    ThreadType initial_thread_type) {}

}  // namespace internal

// static
ThreadType PlatformThread::GetCurrentEffectiveThreadTypeForTest() {
  // Fuchsia doesn't provide a way to get the current thread's priority.
  // Use ThreadType stored in TLS as a proxy.
  const ThreadType thread_type = PlatformThread::GetCurrentThreadType();
  if (thread_type == ThreadType::kAudioProcessing) {
    return ThreadType::kPresentation;
  }
  return thread_type;
}

}  // namespace base
