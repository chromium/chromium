// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/thread_group_profiler.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/numerics/safe_conversions.h"
#include "base/profiler/periodic_sampling_scheduler.h"
#include "base/profiler/sample_metadata.h"
#include "base/profiler/sampling_profiler_thread_token.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/profiler/thread_group_profiler_client.h"
#include "base/task/sequenced_task_runner.h"
// Required solely to avoid complaints on incomplete type for
// Unretained(worker_thread) invocations. This code otherwise treats
// WorkerThread pointers as opaque.
#include "base/task/thread_pool/worker_thread.h"
#include "base/time/time.h"

// Periodic sampling collection is done in CollectProfilesTask(). The function
// is scheduled based on PeriodicSamplingScheduler timing and will start
// profiling all active worker threads.
//
// During a sampling session, new worker threads and worker threads that become
// active (being signalled for work while idle) will call OnWorkerThreadActive
// so profiling can be started for them. If at any point the worker thread is
// shutdown (this should only happen in test as we only sample active threads
// and the thread reclaim time after idle is longer than sampling duration), the
// profiler for that thread is stopped and worker thread blocked until profiler
// is destroyed. This should guarantee a uniform sampling for all worker thread
// executions as all the work happening inside a sampling session is collected
// regardless of which thread the work is scheduled.
//
// Thread group shutdown happens after task runner shutdown so no more sampling
// can be scheduled. All existing profilers will be cleared on the main thread
// during shutdown and a profiler shutdown event will signal. Note that after
// ThreadGroup shutdown is started worker threads may still execute
// CONTINUE_ON_SHUTDOWN tasks and these tasks will never be sampled. This is
// acceptable as these profiles are unlikely to be uploaded anyway.

// ThreadGroupProfiler will only be destructed in test through
// ThreadGroupImpl::JoinForTesting. This also happens after task runner shutdown
// so same logic applies as normal shutdown. In prod the thread pool (which
// holds thread group) is always leaked during shutdown.

namespace base {
namespace {
// Pointer to the embedder-specific client implementation.
// |g_thread_group_profiler_client| is intentionally leaked on shutdown.
ThreadGroupProfilerClient* g_thread_group_profiler_client = nullptr;

// Run continuous profiling 2% of the time.
constexpr double kFractionOfExecutionTimeToSample = 0.02;

constexpr char kProfilerMetadataThreadGroupType[] = "ThreadGroupType";

// Keep sampling new worker thread until last second of sampling duration.
// This is intended as an performance optimization, i.e. it's not worth it to do
// the whole StackSamplingProfiler set up just to get less than 10 samples. And
// since this treats all threads equally it does not affect the unbiased nature
// of sampling.
const TimeDelta kMinRemainingTimeForNewThreadSampling = Seconds(1);
}  // namespace

// static
void ThreadGroupProfiler::SetClient(
    std::unique_ptr<ThreadGroupProfilerClient> client) {
  // Generally, the client should only be set once, at process startup. However,
  // some test infrastructure causes initialization to happen more than once.
  delete g_thread_group_profiler_client;
  g_thread_group_profiler_client = client.release();
}

// static
bool ThreadGroupProfiler::IsProfilingEnabled() {
  // TODO(crbug.com/40226611): Remove GetClient() check once client is set on
  // all embedders. This is to temporarily support testing with mock client when
  // real clients aren't set on embedders.
  return GetClient() && GetClient()->IsProfilerEnabledForCurrentProcess();
}

ThreadGroupProfiler::ThreadGroupProfiler(
    int64_t thread_group_type,
    Delegate* delegate,
    std::unique_ptr<PeriodicSamplingScheduler> periodic_sampling_scheduler,
    ProfilerFactory profiler_factory)
    : thread_group_type_(thread_group_type),
      delegate_(delegate),
      periodic_sampling_scheduler_(
          periodic_sampling_scheduler
              ? std::move(periodic_sampling_scheduler)
              : std::make_unique<PeriodicSamplingScheduler>(
                    GetSamplingDuration(),
                    kFractionOfExecutionTimeToSample,
                    TimeTicks::Now())),
      stack_sampling_profiler_factory_(std::move(profiler_factory)) {
  CHECK(delegate_);
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ThreadGroupProfiler::~ThreadGroupProfiler() = default;

void ThreadGroupProfiler::Start(
    scoped_refptr<SequencedTaskRunner> service_thread_task_runner) {
  service_thread_task_runner_ = std::move(service_thread_task_runner);
  ScheduleNextCollection();
}

void ThreadGroupProfiler::ScheduleNextCollection() {
  // It is safe to call `GetTimeToNextCollection()` here: the first call occurs
  // during `Start()` (thread pool initialization) before any other tasks run,
  // and every subsequent call runs on `sequence_checker_` in
  // `OnEndProfilingSessionTask()`.
  service_thread_task_runner_->PostDelayedTask(
      FROM_HERE,
      BindOnce(&ThreadGroupProfiler::OnStartProfilingSessionTask,
               weak_ptr_factory_.GetWeakPtr()),
      periodic_sampling_scheduler_->GetTimeToNextCollection());
}

void ThreadGroupProfiler::OnStartProfilingSessionTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->OnStartProfilingSession(CreateActiveCollection());
  service_thread_task_runner_->PostDelayedTask(
      FROM_HERE,
      BindOnce(&ThreadGroupProfiler::OnEndProfilingSessionTask,
               weak_ptr_factory_.GetWeakPtr()),
      GetSamplingDuration());
}

void ThreadGroupProfiler::OnEndProfilingSessionTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->OnEndProfilingSession();
  ScheduleNextCollection();
}

ThreadGroupProfiler::ActiveCollection
ThreadGroupProfiler::CreateActiveCollection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ActiveCollection(thread_group_type_, GetSamplingDuration(),
                          stack_sampling_profiler_factory_);
}

// Production implementation that wraps an actual StackSamplingProfiler.
class ThreadGroupProfiler::ProfilerImpl : public ThreadGroupProfiler::Profiler {
 public:
  ProfilerImpl(int64_t thread_group_type,
               SamplingProfilerThreadToken thread_token,
               const StackSamplingProfiler::SamplingParams& params,
               std::unique_ptr<ProfileBuilder> profile_builder,
               StackSamplingProfiler::UnwindersFactory unwinder_factory)
      : thread_group_type_(thread_group_type),
        thread_token_(thread_token),
        sampling_profiler_{thread_token, params, std::move(profile_builder),
                           std::move(unwinder_factory)} {}

  // Profiler:
  void Start() override {
    AddProfileMetadataForThread(kProfilerMetadataThreadGroupType,
                                thread_group_type_, thread_token_.id);
    sampling_profiler_.Start();
  }

 protected:
  ~ProfilerImpl() override = default;

 private:
  const int64_t thread_group_type_;
  const SamplingProfilerThreadToken thread_token_;
  StackSamplingProfiler sampling_profiler_;
};

ThreadGroupProfiler::ActiveCollection::ActiveCollection(
    int64_t thread_group_type,
    TimeDelta sampling_duration,
    ProfilerFactory factory)
    : thread_group_type_(thread_group_type),
      stack_sampling_profiler_factory_(factory),
      sampling_duration_(sampling_duration),
      collection_end_time_(TimeTicks::Now() + sampling_duration) {}

scoped_refptr<ThreadGroupProfiler::Profiler>
ThreadGroupProfiler::ActiveCollection::MaybeAddWorkerThread(
    internal::WorkerThread* worker_thread,
    SamplingProfilerThreadToken token) {
  // Skip if the remaining time of current sampling session is less than the
  // threshold.
  if ((collection_end_time_ - TimeTicks::Now()) <
      kMinRemainingTimeForNewThreadSampling) {
    return nullptr;
  }
  // Skip if there's already a profiler for this thread. A worker thread can
  // flip between idle and active anytime during the collection but profiler
  // should only be created for it the first time it becomes active.
  if (profilers_.find(worker_thread) != profilers_.end()) {
    return nullptr;
  }
  StackSamplingProfiler::SamplingParams sampling_params =
      GetClient()->GetSamplingParams();
  // Calculate remaining samples until end of collection period.
  const TimeDelta remaining = collection_end_time_ - TimeTicks::Now();
  sampling_params.samples_per_profile =
      ClampFloor(remaining / sampling_params.sampling_interval);
  scoped_refptr<Profiler> profiler =
      CreateSamplingProfilerForThread(worker_thread, token, sampling_params);
  profilers_.emplace(worker_thread, profiler);
  return profiler;
}

scoped_refptr<ThreadGroupProfiler::Profiler>
ThreadGroupProfiler::ActiveCollection::RemoveWorkerThread(
    internal::WorkerThread* worker_thread) {
  auto it = profilers_.find(worker_thread);
  if (it == profilers_.end()) {
    return nullptr;
  }
  scoped_refptr<Profiler> profiler = std::move(it->second);
  profilers_.erase(it);
  return profiler;
}

scoped_refptr<ThreadGroupProfiler::Profiler>
ThreadGroupProfiler::ActiveCollection::CreateSamplingProfilerForThread(
    internal::WorkerThread* worker_thread,
    const SamplingProfilerThreadToken& token,
    const StackSamplingProfiler::SamplingParams& sampling_params) {
  ThreadGroupProfilerClient* client = GetClient();
  return stack_sampling_profiler_factory_.Run(
      thread_group_type_, token, sampling_params,
      client->CreateProfileBuilder(DoNothing()), client->GetUnwindersFactory());
}

ThreadGroupProfiler::ActiveCollection::~ActiveCollection() = default;

ThreadGroupProfiler::ActiveCollection::ActiveCollection(ActiveCollection&&) =
    default;

ThreadGroupProfiler::ActiveCollection&
ThreadGroupProfiler::ActiveCollection::operator=(ActiveCollection&&) = default;

// static
ThreadGroupProfilerClient* ThreadGroupProfiler::GetClient() {
  // TODO(crbug.com/40226611): Add check once client is set on all embedders.
  // CHECK(g_thread_group_profiler_client);
  return g_thread_group_profiler_client;
}

// static
ThreadGroupProfiler::ProfilerFactory
ThreadGroupProfiler::GetDefaultProfilerFactory() {
  return BindRepeating(
      [](int64_t thread_group_type, SamplingProfilerThreadToken thread_token,
         const StackSamplingProfiler::SamplingParams& params,
         std::unique_ptr<ProfileBuilder> profile_builder,
         StackSamplingProfiler::UnwindersFactory unwinder_factory)
          -> scoped_refptr<Profiler> {
        return MakeRefCounted<ProfilerImpl>(thread_group_type, thread_token,
                                            params, std::move(profile_builder),
                                            std::move(unwinder_factory));
      });
}

// static
TimeDelta ThreadGroupProfiler::GetSamplingDuration() {
  StackSamplingProfiler::SamplingParams params =
      GetClient()->GetSamplingParams();
  return params.sampling_interval * params.samples_per_profile;
}

}  // namespace base
