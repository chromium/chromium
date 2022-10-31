// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/tracing/perfetto_platform.h"

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/deferred_sequenced_task_runner.h"
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

PerfettoPlatform::PerfettoPlatform(TaskRunnerType task_runner_type)
    : task_runner_type_(task_runner_type),
      deferred_task_runner_(new DeferredSequencedTaskRunner()),
      thread_local_object_([](void* object) {
        delete static_cast<ThreadLocalObject*>(object);
      }) {}

PerfettoPlatform::~PerfettoPlatform() = default;

void PerfettoPlatform::StartTaskRunner(
    scoped_refptr<SequencedTaskRunner> task_runner) {
  DCHECK_EQ(task_runner_type_, TaskRunnerType::kThreadPool);
  DCHECK(!did_start_task_runner_);
  deferred_task_runner_->StartWithTaskRunner(task_runner);
  did_start_task_runner_ = true;
}

SequencedTaskRunner* PerfettoPlatform::task_runner() const {
  return deferred_task_runner_.get();
}

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
  switch (task_runner_type_) {
    case TaskRunnerType::kBuiltin:
#if !BUILDFLAG(IS_NACL)
      return std::make_unique<perfetto::base::ThreadTaskRunner>(
          perfetto::base::ThreadTaskRunner::CreateAndStart());
#else
      DCHECK(false);
      return nullptr;
#endif
    case TaskRunnerType::kThreadPool:
      // We can't create a real task runner yet because the ThreadPool may not
      // be initialized. Instead, we point Perfetto to a buffering task runner
      // which will become active as soon as the thread pool is up (see
      // StartTaskRunner).
      return std::make_unique<PerfettoTaskRunner>(deferred_task_runner_);
  }
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

}  // namespace tracing
}  // namespace base
