// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/thread_group_profiler.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/profiler/periodic_sampling_scheduler.h"
#include "base/profiler/sampling_profiler_thread_token.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/profiler/thread_group_profiler_client.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/bind_post_task.h"
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
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::string_view thread_group_label,
    std::unique_ptr<PeriodicSamplingScheduler> periodic_sampling_scheduler,
    ProfilerFactory profiler_factory)
    : thread_group_label_(thread_group_label),
      periodic_sampling_scheduler_(std::move(periodic_sampling_scheduler)),
      task_runner_(std::move(task_runner)),
      stack_sampling_profiler_factory_(std::move(profiler_factory)) {
  DETACH_FROM_SEQUENCE(task_runner_sequence_checker_);
  if (!periodic_sampling_scheduler_) {
    periodic_sampling_scheduler_ = std::make_unique<PeriodicSamplingScheduler>(
        GetClient()->GetSamplingParams().sampling_interval *
            GetClient()->GetSamplingParams().samples_per_profile,
        kFractionOfExecutionTimeToSample, TimeTicks::Now());
  }
  task_runner_->PostTask(
      FROM_HERE, BindOnce(&ThreadGroupProfiler::StartTask, Unretained(this)));
}

ThreadGroupProfiler::~ThreadGroupProfiler() {
  // Shutdown has been run before destruction.
  CHECK(!active_collection_);
}

void ThreadGroupProfiler::Shutdown() {
  // Must be destroyed from the same sequence as constructor.
  DCHECK_CALLED_ON_VALID_SEQUENCE(construction_sequence_checker_);
  // CHECK that the task runner has actually been shutdown.
  CHECK(!task_runner_->PostTask(FROM_HERE, DoNothing()));

  TS_UNCHECKED_READ(active_collection_).reset();
  thread_group_profiler_shutdown_.Signal();
}

void ThreadGroupProfiler::OnWorkerThreadStarted(
    internal::WorkerThread* worker_thread) {
  task_runner_->PostTask(
      FROM_HERE, BindOnce(&ThreadGroupProfiler::OnWorkerThreadStartedTask,
                          Unretained(this), Unretained(worker_thread),
                          GetSamplingProfilerCurrentThreadToken()));
}

void ThreadGroupProfiler::OnWorkerThreadActive(
    internal::WorkerThread* worker_thread) {
  task_runner_->PostTask(
      FROM_HERE, BindOnce(&ThreadGroupProfiler::OnWorkerThreadActiveTask,
                          Unretained(this), Unretained(worker_thread)));
}

void ThreadGroupProfiler::OnWorkerThreadIdle(
    internal::WorkerThread* worker_thread) {
  task_runner_->PostTask(FROM_HERE,
                         BindOnce(&ThreadGroupProfiler::OnWorkerThreadIdleTask,
                                  Unretained(this), Unretained(worker_thread)));
}

void ThreadGroupProfiler::OnWorkerThreadExiting(
    internal::WorkerThread* worker_thread) {
  WaitableEvent profiling_has_stopped;
  task_runner_->PostTask(
      FROM_HERE, BindOnce(&ThreadGroupProfiler::OnWorkerThreadExitingTask,
                          Unretained(this), Unretained(worker_thread),
                          Unretained(&profiling_has_stopped)));
  base::WaitableEvent* event_array[] = {&profiling_has_stopped,
                                        &thread_group_profiler_shutdown_};
  // During shutdown profiling_has_stopped may not get a chance to signal as
  // task runner is stopped, profiler_shutdown event will signal instead
  // indicating that clean up has finished and worker thread may safely exit.
  WaitableEvent::WaitMany(event_array, std::size(event_array));
}

// Production implementation that wraps an actual StackSamplingProfiler.
class ThreadGroupProfiler::ProfilerImpl : public ThreadGroupProfiler::Profiler {
 public:
  ProfilerImpl(SamplingProfilerThreadToken thread_token,
               const StackSamplingProfiler::SamplingParams& params,
               std::unique_ptr<ProfileBuilder> profile_builder,
               StackSamplingProfiler::UnwindersFactory unwinder_factory)
      : sampling_profiler_{thread_token, params, std::move(profile_builder),
                           std::move(unwinder_factory)} {}
  ~ProfilerImpl() override = default;

  // Profiler:
  void Start() override { sampling_profiler_.Start(); }

 private:
  StackSamplingProfiler sampling_profiler_;
};

ThreadGroupProfiler::ActiveCollection::ActiveCollection(
    const flat_map<internal::WorkerThread*, WorkerThreadContext>&
        worker_thread_context_set,
    const TimeDelta& sampling_duration,
    SequencedTaskRunner* task_runner,
    ProfilerFactory factory,
    OnceClosure collection_complete_callback)
    : task_runner_(task_runner),
      stack_sampling_profiler_factory_(factory),
      collection_complete_callback_(std::move(collection_complete_callback)),
      sampling_duration_(sampling_duration),
      collection_end_time_(TimeTicks::Now() + sampling_duration),
      empty_collection_closure_{
          BindOnce(&ActiveCollection::OnEmptyCollectionCompleted,
                   Unretained(this))} {
  decltype(profilers_)::container_type new_profilers;
  for (auto& [worker_thread, context] : worker_thread_context_set) {
    // Only create profilers for active threads.
    if (!context.is_idle) {
      std::unique_ptr<Profiler> profiler = CreateSamplingProfilerForThread(
          worker_thread, context.token, GetClient()->GetSamplingParams());
      profiler->Start();
      new_profilers.emplace_back(worker_thread, std::move(profiler));
    }
  }
  // More efficient to construct flat_map from containers then adding each
  // profiler in a loop.
  profilers_ = flat_map(std::move(new_profilers));
  if (profilers_.empty()) {
    // Queue a delayed empty collection callback to run after the sampling
    // duration if there are no active threads to sample.
    task_runner_->PostDelayedTask(
        FROM_HERE, empty_collection_closure_.callback(), sampling_duration_);
  } else {
    empty_collection_closure_.Cancel();
  }
}

void ThreadGroupProfiler::ActiveCollection::MaybeAddWorkerThread(
    internal::WorkerThread* worker_thread,
    const SamplingProfilerThreadToken& token) {
  // Skip if the remaining time of current sampling session is less than the
  // threshold.
  if ((collection_end_time_ - TimeTicks::Now()) <
      kMinRemainingTimeForNewThreadSampling) {
    return;
  }
  // Skip if there's already a profiler for this thread. A worker thread can
  // flip between idle and active anytime during the collection but profiler
  // should only be created for it the first time it becomes active.
  if (profilers_.find(worker_thread) != profilers_.end()) {
    return;
  }
  StackSamplingProfiler::SamplingParams sampling_params =
      GetClient()->GetSamplingParams();
  // Calculate remaining samples until end of collection period.
  sampling_params.samples_per_profile =
      ClampFloor((collection_end_time_ - TimeTicks::Now()) /
                 sampling_params.sampling_interval);
  std::unique_ptr<Profiler> profiler =
      CreateSamplingProfilerForThread(worker_thread, token, sampling_params);
  profiler->Start();
  profilers_.emplace(worker_thread, std::move(profiler));
  // Cancel empty callback since there is a profiler running now.
  empty_collection_closure_.Cancel();
}

void ThreadGroupProfiler::ActiveCollection::RemoveWorkerThread(
    internal::WorkerThread* worker_thread) {
  // If there's a profiler associated, remove it. Will block until profiler
  // destructor finishes but it should be a rare case (during shutdown or
  // ThreadGroup::JoinForTesting) as we only sample active threads; they should
  // not get reclaimed during sampling session.
  const bool was_present = profilers_.erase(worker_thread) == 1;
  if (!was_present || !profilers_.empty()) {
    return;
  }
  // Queue a delayed empty collection callback to run after the sampling
  // duration if there are no active threads to sample.
  empty_collection_closure_.Reset(BindOnce(
      &ActiveCollection::OnEmptyCollectionCompleted, Unretained(this)));
  task_runner_->PostDelayedTask(FROM_HERE, empty_collection_closure_.callback(),
                                collection_end_time_ - TimeTicks::Now());
}

std::unique_ptr<ThreadGroupProfiler::Profiler>
ThreadGroupProfiler::ActiveCollection::CreateSamplingProfilerForThread(
    internal::WorkerThread* worker_thread,
    const SamplingProfilerThreadToken& token,
    const StackSamplingProfiler::SamplingParams& sampling_params) {
  ThreadGroupProfilerClient* client = ThreadGroupProfiler::GetClient();
  return stack_sampling_profiler_factory_.Run(
      token, sampling_params,
      client->CreateProfileBuilder(BindPostTask(
          task_runner_,
          BindOnce(&ActiveCollection::OnProfilerCollectionCompleted,
                   Unretained(this), Unretained(worker_thread)))),
      client->GetUnwindersFactory());
}

void ThreadGroupProfiler::ActiveCollection::OnProfilerCollectionCompleted(
    internal::WorkerThread* worker_thread) {
  DCHECK(!profilers_.empty());
  profilers_.erase(worker_thread);
  // Notify the collection is complete when there's no outstanding profilers.
  if (profilers_.empty()) {
    std::move(collection_complete_callback_).Run();
  }
}

void ThreadGroupProfiler::ActiveCollection::OnEmptyCollectionCompleted() {
  DCHECK(profilers_.empty());
  std::move(collection_complete_callback_).Run();
}

ThreadGroupProfiler::ActiveCollection::~ActiveCollection() = default;

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
      [](SamplingProfilerThreadToken thread_token,
         const StackSamplingProfiler::SamplingParams& params,
         std::unique_ptr<ProfileBuilder> profile_builder,
         StackSamplingProfiler::UnwindersFactory unwinder_factory)
          -> std::unique_ptr<Profiler> {
        return std::make_unique<ProfilerImpl>(thread_token, params,
                                              std::move(profile_builder),
                                              std::move(unwinder_factory));
      });
}

// static
TimeDelta ThreadGroupProfiler::GetSamplingDuration() {
  StackSamplingProfiler::SamplingParams params =
      GetClient()->GetSamplingParams();
  return params.sampling_interval * params.samples_per_profile;
}

void ThreadGroupProfiler::ThreadGroupProfiler::StartTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(task_runner_sequence_checker_);
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ThreadGroupProfiler::CollectProfilesTask,
                     Unretained(this)),
      periodic_sampling_scheduler_->GetTimeToNextCollection());
}

void ThreadGroupProfiler::OnWorkerThreadStartedTask(
    internal::WorkerThread* worker_thread,
    SamplingProfilerThreadToken token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(task_runner_sequence_checker_);
  const bool inserted =
      worker_thread_context_set_
          .emplace(worker_thread, WorkerThreadContext{token,
                                                      /*is_idle=*/true})
          .second;
  // Worker thread should not be present before this call.
  DCHECK(inserted);
}

// A worker thread starts out on the idle set when it's created. On its
// ThreadMain it will call Delegate::GetWork() and when it does obtain a task
// source it will be removed from idle set and becomes active.
// OnWorkerThreadActive() will be called at that point. When it exhausted the
// task source, it will be placed on idle set and nullptr returned from
// GetWork()/ProcessSwappedTask(). The worker thread will then enter a
// TimedWait until it's either wake up or reaches its reclaim time.
void ThreadGroupProfiler::OnWorkerThreadActiveTask(
    internal::WorkerThread* worker_thread) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(task_runner_sequence_checker_);
  auto it = worker_thread_context_set_.find(worker_thread);
  // Profiler token should already be set since OnWorkerThreadActive will
  // be called strictly after worker thread creation.
  DCHECK(it != worker_thread_context_set_.end());
  // Mark worker thread as active.
  it->second.is_idle = false;
  if (active_collection_) {
    active_collection_->MaybeAddWorkerThread(worker_thread, it->second.token);
  }
}

void ThreadGroupProfiler::OnWorkerThreadIdleTask(
    internal::WorkerThread* worker_thread) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(task_runner_sequence_checker_);
  auto it = worker_thread_context_set_.find(worker_thread);
  DCHECK(it != worker_thread_context_set_.end());
  // Mark worker thread as idle.
  it->second.is_idle = true;
}

void ThreadGroupProfiler::OnWorkerThreadExitingTask(
    internal::WorkerThread* worker_thread,
    WaitableEvent* profiling_has_stopped) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(task_runner_sequence_checker_);
  if (active_collection_) {
    active_collection_->RemoveWorkerThread(worker_thread);
  }
  worker_thread_context_set_.erase(worker_thread);
  profiling_has_stopped->Signal();
}

void ThreadGroupProfiler::CollectProfilesTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(task_runner_sequence_checker_);
  DCHECK(!active_collection_);
  active_collection_.emplace(
      worker_thread_context_set_, GetSamplingDuration(), task_runner_.get(),
      stack_sampling_profiler_factory_,
      BindOnce(&ThreadGroupProfiler::EndActiveCollectionTask,
               Unretained(this)));
}

void ThreadGroupProfiler::EndActiveCollectionTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(task_runner_sequence_checker_);
  DCHECK(active_collection_);
  active_collection_.reset();
  // Schedule the next collection.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ThreadGroupProfiler::CollectProfilesTask,
                     Unretained(this)),
      periodic_sampling_scheduler_->GetTimeToNextCollection());
}

}  // namespace base
