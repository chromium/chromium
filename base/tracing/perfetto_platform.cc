// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/tracing/perfetto_platform.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing/perfetto_task_runner.h"
#include "base/tracing_buildflags.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_NACL)
#include "third_party/perfetto/include/perfetto/ext/base/thread_task_runner.h"
#endif

namespace base {
namespace tracing {

namespace {
constexpr char kProcessNamePrefix[] = "org.chromium-";
}  // namespace

PerfettoPlatform::PerfettoPlatform(PerfettoTaskRunner* task_runner)
    : task_runner_(task_runner), thread_local_object_([](void* object) {
        delete static_cast<ThreadLocalObject*>(object);
      }) {}

PerfettoPlatform::~PerfettoPlatform() = default;

PerfettoPlatform::ThreadLocalObject*
PerfettoPlatform::GetOrCreateThreadLocalObject() {
  auto* object = static_cast<ThreadLocalObject*>(thread_local_object_.Get());
  if (!object) {
    object = ThreadLocalObject::CreateInstance().release();
    thread_local_object_.Set(object);
  }
  return object;
}

std::unique_ptr<perfetto::base::TaskRunner> PerfettoPlatform::CreateTaskRunner(
    const CreateTaskRunnerArgs&) {
  // TODO(b/242965112): Add support for the builtin task runner
  return std::make_unique<PerfettoTaskRunner>(
      task_runner_->GetOrCreateTaskRunner());
}

// This method is used by the SDK to determine the producer name.
// Note that we override the producer name for the mojo backend in ProducerHost,
// and thus this only affects the producer name for the system backend.
std::string PerfettoPlatform::GetCurrentProcessName() {
  const char* host_package_name = nullptr;
#if BUILDFLAG(IS_ANDROID)
  host_package_name = android::BuildInfo::GetInstance()->host_package_name();
#endif  // BUILDFLAG(IS_ANDROID)

  // On Android we want to include if this is webview inside of an app or
  // Android Chrome. To aid this we add the host_package_name to differentiate
  // the various apps and sources.
  std::string process_name;
  if (host_package_name) {
    process_name = StrCat(
        {kProcessNamePrefix, host_package_name, "-",
         NumberToString(trace_event::TraceLog::GetInstance()->process_id())});
  } else {
    process_name = StrCat(
        {kProcessNamePrefix,
         NumberToString(trace_event::TraceLog::GetInstance()->process_id())});
  }
  return process_name;
}

perfetto::base::PlatformThreadId PerfettoPlatform::GetCurrentThreadId() {
  return base::PlatformThread::CurrentId();
}

}  // namespace tracing
}  // namespace base
