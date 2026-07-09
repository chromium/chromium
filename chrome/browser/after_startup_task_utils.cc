// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/after_startup_task_utils.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/circular_deque.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/common/chrome_features.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/ash/login/login_display_host.h"
#endif

using content::BrowserThread;

namespace {

struct AfterStartupTask {
  AfterStartupTask(const base::Location& from_here,
                   const scoped_refptr<base::SequencedTaskRunner>& task_runner,
                   base::OnceClosure task)
      : from_here(from_here), task_runner(task_runner), task(std::move(task)) {}
  ~AfterStartupTask() = default;

  const base::Location from_here;
  const scoped_refptr<base::SequencedTaskRunner> task_runner;
  base::OnceClosure task;
};

// The flag may be read on any thread, but must only be set on the UI thread.
base::AtomicFlag& GetStartupCompleteFlag() {
  static base::NoDestructor<base::AtomicFlag> startup_complete_flag;
  return *startup_complete_flag;
}

// The queue may only be accessed on the UI thread.
base::circular_deque<AfterStartupTask*>& GetAfterStartupTasks() {
  static base::NoDestructor<base::circular_deque<AfterStartupTask*>>
      after_startup_tasks;
  return *after_startup_tasks;
}

bool IsBrowserStartupComplete() {
  return GetStartupCompleteFlag().IsSet();
}

void RunTask(std::unique_ptr<AfterStartupTask> queued_task) {
  // We're careful to delete the caller's |task| on the target runner's thread.
  DCHECK(queued_task->task_runner->RunsTasksInCurrentSequence());
  std::move(queued_task->task).Run();
}

void ScheduleTask(std::unique_ptr<AfterStartupTask> queued_task) {
  scoped_refptr<base::SequencedTaskRunner> target_runner =
      queued_task->task_runner;
  base::Location from_here = queued_task->from_here;
  target_runner->PostTask(from_here,
                          base::BindOnce(&RunTask, std::move(queued_task)));
}

void QueueTask(std::unique_ptr<AfterStartupTask> queued_task) {
  DCHECK(queued_task);

  // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/40515428
  // for details.
  CHECK(queued_task->task);

  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    // Posted with USER_VISIBLE priority to avoid this becoming an after startup
    // task itself.
    content::GetUIThreadTaskRunner({base::TaskPriority::USER_VISIBLE})
        ->PostTask(FROM_HERE,
                   base::BindOnce(QueueTask, std::move(queued_task)));
    return;
  }

  // The flag may have been set while the task to invoke this method
  // on the UI thread was inflight.
  if (IsBrowserStartupComplete()) {
    ScheduleTask(std::move(queued_task));
    return;
  }
  GetAfterStartupTasks().push_back(queued_task.release());
}

void SetBrowserStartupIsComplete() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (IsBrowserStartupComplete())
    return;

  size_t browser_count = 0;
#if !BUILDFLAG(IS_ANDROID)
  browser_count = GlobalBrowserCollection::GetInstance()->GetSize();
#endif  // !BUILDFLAG(IS_ANDROID)
  TRACE_EVENT_INSTANT1("startup", "Startup.StartupComplete",
                       TRACE_EVENT_SCOPE_GLOBAL, "BrowserCount", browser_count);
  GetStartupCompleteFlag().Set();
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  // Process::Current().CreationTime() is not available on all platforms.
  const base::Time process_creation_time =
      base::Process::Current().CreationTime();
  if (!process_creation_time.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES("Startup.AfterStartupTaskDelayedUntilTime",
                             base::Time::Now() - process_creation_time);
  }
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)
  UMA_HISTOGRAM_COUNTS_10000("Startup.AfterStartupTaskCount",
                             GetAfterStartupTasks().size());
  for (AfterStartupTask* queued_task : GetAfterStartupTasks()) {
    ScheduleTask(base::WrapUnique(queued_task));
  }
  GetAfterStartupTasks().clear();
  GetAfterStartupTasks().shrink_to_fit();
}

bool g_is_monitoring_started = false;

// For Android, startup completion is signaled via AfterStartupTaskUtils.java.
// We do not use the StartupObserver or startup refs on Android.
#if !BUILDFLAG(IS_ANDROID)
// We initialize `g_ref_count` to 1 to represent the startup sequence itself.
// This implicit reference is released in `BeginMonitoringStartupCompletion()`
// when the startup sequence finishes registering its initial tasks. This
// prevents startup from being marked complete prematurely if a registered
// reference is acquired and released synchronously before all references are
// registered.
int g_ref_count = 1;

void MaybeSignalStartupComplete() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (g_ref_count > 0) {
    return;
  }
  SetBrowserStartupIsComplete();
}

void ReleaseRef() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK_GT(g_ref_count, 0);
  g_ref_count--;
  MaybeSignalStartupComplete();
}

// Observes the first visible page load and releases the page load reference.
//
// This is useful even though we have `FirstWebContentsProfiler` (which tracks
// the first paint) for cases where `FirstWebContents` observation is abandoned
// (e.g., early paint or if paint tracking is disabled). It serves as the second
// best proxy for having loaded, or at least attempted to load, something in the
// foreground.
//
// In most cases, this won't be the last reference released (since paint or
// session restore will usually finish later), and that's okay.
//
// Ownership is passed to the Performance Manager after creation.
class StartupObserver : public performance_manager::GraphOwned,
                        public performance_manager::PageNodeObserver {
 public:
  StartupObserver(const StartupObserver&) = delete;
  StartupObserver& operator=(const StartupObserver&) = delete;

  ~StartupObserver() override = default;

  static void Start();

 private:
  using LoadingState = performance_manager::PageNode::LoadingState;

  StartupObserver() {
    startup_ref_ = AfterStartupTaskUtils::RegisterStartupInProgressRef();
  }

  void OnFirstVisiblePageLoadComplete() {
    startup_ref_.reset();
    // This will result in delete getting called.
    TakeFromGraph();
  }

  // GraphOwned overrides
  void OnPassedToGraph(performance_manager::Graph* graph) override {
    graph->AddPageNodeObserver(this);
  }

  void OnTakenFromGraph(performance_manager::Graph* graph) override {
    graph->RemovePageNodeObserver(this);
  }

  // PageNodeObserver overrides
  void OnLoadingStateChanged(const performance_manager::PageNode* page_node,
                             LoadingState previous_state) override {
    // Only interested in visible tabs when feature is enabled, or any visible
    // page node when disabled.
    if (page_node->IsVisible() &&
        (page_node->GetType() == performance_manager::PageType::kTab ||
         !base::FeatureList::IsEnabled(
             features::kImprovedStartupBestEffortDelay))) {
      LoadingState state = page_node->GetLoadingState();
      if (state == LoadingState::kLoadedIdle ||
          state == LoadingState::kLoadingTimedOut) {
        OnFirstVisiblePageLoadComplete();
      }
    }
  }

  void TakeFromGraph() {
    CHECK(performance_manager::PerformanceManager::IsAvailable());
    performance_manager::PerformanceManager::GetGraph()->TakeFromGraph(this);
  }

  std::unique_ptr<AfterStartupTaskUtils::StartupInProgressRef> startup_ref_;
};

// static
void StartupObserver::Start() {
  CHECK(performance_manager::PerformanceManager::IsAvailable());

  // Pass a new StartupObserver to the performance manager so we can get
  // notified when loading completes. The performance manager takes ownership.
  performance_manager::PerformanceManager::GetGraph()->PassToGraph(
      base::WrapUnique(new StartupObserver()));
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

void AfterStartupTaskUtils::BeginMonitoringStartupCompletion() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(!g_is_monitoring_started);
  g_is_monitoring_started = true;
#if BUILDFLAG(IS_CHROMEOS)
  // If we are on a login screen which does not expect WebUI to be loaded,
  // Browser won't be created at startup.
  if (ash::LoginDisplayHost::default_host() &&
      !ash::LoginDisplayHost::default_host()->IsWebUIStarted()) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&SetBrowserStartupIsComplete));
    return;
  }
#endif

#if !BUILDFLAG(IS_ANDROID)
  StartupObserver::Start();
  // Release the implicit reference representing the startup sequence. This
  // enables considering startup complete once all other registered references
  // (e.g., paint, idle, restore) are released.
  ReleaseRef();
#endif  // !BUILDFLAG(IS_ANDROID)

  // Add failsafe timeout
  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE, base::BindOnce(&SetBrowserStartupIsComplete),
      base::Minutes(3));
}

void AfterStartupTaskUtils::BeginMonitoringStartupCompletionForTesting() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (g_is_monitoring_started) {
    return;
  }
  AfterStartupTaskUtils::BeginMonitoringStartupCompletion();
}

#if !BUILDFLAG(IS_ANDROID)
AfterStartupTaskUtils::StartupInProgressRef::StartupInProgressRef(
    base::OnceClosure release_callback)
    : release_runner_(std::move(release_callback)) {}

AfterStartupTaskUtils::StartupInProgressRef::~StartupInProgressRef() = default;

// static
std::unique_ptr<AfterStartupTaskUtils::StartupInProgressRef>
AfterStartupTaskUtils::RegisterStartupInProgressRef() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (IsBrowserStartupComplete()) {
    return nullptr;
  }
  g_ref_count++;
  return std::make_unique<AfterStartupTaskUtils::StartupInProgressRef>(
      base::BindOnce(&ReleaseRef));
}
#endif  // !BUILDFLAG(IS_ANDROID)

void AfterStartupTaskUtils::PostTask(
    const base::Location& from_here,
    const scoped_refptr<base::SequencedTaskRunner>& destination_runner,
    base::OnceClosure task) {
  if (IsBrowserStartupComplete()) {
    destination_runner->PostTask(from_here, std::move(task));
    return;
  }

  std::unique_ptr<AfterStartupTask> queued_task(
      new AfterStartupTask(from_here, destination_runner, std::move(task)));
  QueueTask(std::move(queued_task));
}

void AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting() {
  ::SetBrowserStartupIsComplete();
}

void AfterStartupTaskUtils::SetBrowserStartupIsComplete() {
  ::SetBrowserStartupIsComplete();
}

bool AfterStartupTaskUtils::IsBrowserStartupComplete() {
  return ::IsBrowserStartupComplete();
}

void AfterStartupTaskUtils::UnsafeResetForTesting() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(GetAfterStartupTasks().empty());
  if (!IsBrowserStartupComplete())
    return;
  GetStartupCompleteFlag().UnsafeResetForTesting();  // IN-TEST
#if !BUILDFLAG(IS_ANDROID)
  g_ref_count = 1;
#endif
  g_is_monitoring_started = false;
  DCHECK(!IsBrowserStartupComplete());
}
