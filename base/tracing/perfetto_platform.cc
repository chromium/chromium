// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/tracing/perfetto_platform.h"

#include "base/deferred_sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "base/tracing/perfetto_task_runner.h"
#include "base/tracing_buildflags.h"
#include "build/build_config.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/thread_descriptor.gen.h"

#if !defined(OS_NACL)
#include "third_party/perfetto/include/perfetto/ext/base/thread_task_runner.h"
#endif

namespace base {
namespace tracing {

PerfettoPlatform::PerfettoPlatform(TaskRunnerType task_runner_type)
    : task_runner_type_(task_runner_type),
      deferred_task_runner_(new DeferredSequencedTaskRunner()),
      thread_local_object_([](void* object) {
        delete static_cast<ThreadLocalObject*>(object);
      }) {
  ThreadIdNameManager::GetInstance()->AddObserver(this);
}

PerfettoPlatform::~PerfettoPlatform() {
  ThreadIdNameManager::GetInstance()->RemoveObserver(this);
}

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
#if !defined(OS_NACL)
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

std::string PerfettoPlatform::GetCurrentProcessName() {
  // TODO(skyostil): Unimplemented since we're not registering producers through
  // the client API yet.
  return "";
}

void PerfettoPlatform::OnThreadNameChanged(const char* name) {
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  // TODO(skyostil): Also capture names for threads which predate tracing being
  // initialized.
  if (perfetto::Tracing::IsInitialized()) {
    auto track = perfetto::ThreadTrack::Current();
    auto desc = track.Serialize();
    desc.mutable_thread()->set_thread_name(name);
    perfetto::TrackEvent::SetTrackDescriptor(track, std::move(desc));
  }
#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
}

}  // namespace tracing
}  // namespace base
