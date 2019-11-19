// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/after_startup_task_utils.h"

#include <memory>
#include <utility>

#include "base/containers/circular_deque.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/synchronization/atomic_flag.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/page_visibility_state.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "ui/views/linux_ui/linux_ui.h"
#endif

using content::BrowserThread;
using content::WebContents;
using content::WebContentsObserver;

namespace {

struct AfterStartupTask {
  AfterStartupTask(const base::Location& from_here,
                   const scoped_refptr<base::TaskRunner>& task_runner,
                   base::OnceClosure task)
      : from_here(from_here), task_runner(task_runner), task(std::move(task)) {}
  ~AfterStartupTask() {}

  const base::Location from_here;
  const scoped_refptr<base::TaskRunner> task_runner;
  base::OnceClosure task;
};

// The flag may be read on any thread, but must only be set on the UI thread.
base::LazyInstance<base::AtomicFlag>::Leaky g_startup_complete_flag;

// The queue may only be accessed on the UI thread.
base::LazyInstance<base::circular_deque<AfterStartupTask*>>::Leaky
    g_after_startup_tasks;

bool g_schedule_tasks_with_delay = true;

bool IsBrowserStartupComplete() {
  // Be sure to initialize the LazyInstance on the main thread since the flag
  // may only be set on it's initializing thread.
  if (!g_startup_complete_flag.IsCreated())
    return false;
  return g_startup_complete_flag.Get().IsSet();
}

void RunTask(std::unique_ptr<AfterStartupTask> queued_task) {
  // We're careful to delete the caller's |task| on the target runner's thread.
  DCHECK(queued_task->task_runner->RunsTasksInCurrentSequence());
  std::move(queued_task->task).Run();
}

void ScheduleTask(std::unique_ptr<AfterStartupTask> queued_task) {
  // Spread their execution over a brief time.
  constexpr int kMinDelaySec = 0;
  constexpr int kMaxDelaySec = 10;
  scoped_refptr<base::TaskRunner> target_runner = queued_task->task_runner;
  base::Location from_here = queued_task->from_here;
  int delay_in_seconds = g_schedule_tasks_with_delay
                             ? base::RandInt(kMinDelaySec, kMaxDelaySec)
                             : 0;
  target_runner->PostDelayedTask(
      from_here, base::BindOnce(&RunTask, std::move(queued_task)),
      base::TimeDelta::FromSeconds(delay_in_seconds));
}

void QueueTask(std::unique_ptr<AfterStartupTask> queued_task) {
  DCHECK(queued_task);

  // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/711167
  // for details.
  CHECK(queued_task->task);

  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    // Posted with USER_VISIBLE priority to avoid this becoming an after startup
    // task itself.
    base::PostTask(FROM_HERE,
                   {BrowserThread::UI, base::TaskPriority::USER_VISIBLE},
                   base::BindOnce(QueueTask, std::move(queued_task)));
    return;
  }

  // The flag may have been set while the task to invoke this method
  // on the UI thread was inflight.
  if (IsBrowserStartupComplete()) {
    ScheduleTask(std::move(queued_task));
    return;
  }
  g_after_startup_tasks.Get().push_back(queued_task.release());
}

void SetBrowserStartupIsComplete() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if defined(OS_MACOSX) || defined(OS_WIN) || defined(OS_LINUX)
  // Process::Current().CreationTime() is not available on all platforms.
  const base::Time process_creation_time =
      base::Process::Current().CreationTime();
  if (!process_creation_time.is_null()) {
    UMA_HISTOGRAM_LONG_TIMES("Startup.AfterStartupTaskDelayedUntilTime",
                             base::Time::Now() - process_creation_time);
  }
#endif  // defined(OS_MACOSX) || defined(OS_WIN) || defined(OS_LINUX)
  UMA_HISTOGRAM_COUNTS_10000("Startup.AfterStartupTaskCount",
                             g_after_startup_tasks.Get().size());
  g_startup_complete_flag.Get().Set();
  for (AfterStartupTask* queued_task : g_after_startup_tasks.Get())
    ScheduleTask(base::WrapUnique(queued_task));
  g_after_startup_tasks.Get().clear();
  g_after_startup_tasks.Get().shrink_to_fit();

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // Make sure we complete the startup notification sequence, or launchers will
  // get confused by not receiving the expected message from the main process.
  views::LinuxUI* linux_ui = views::LinuxUI::instance();
  if (linux_ui)
    linux_ui->NotifyWindowManagerStartupComplete();
#endif
}

// Observes the first visible page load and sets the startup complete
// flag accordingly.
class StartupObserver : public WebContentsObserver {
 public:
  StartupObserver() {}
  ~StartupObserver() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(IsBrowserStartupComplete());
  }

  void Start();

 private:
  void OnStartupComplete() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    SetBrowserStartupIsComplete();
    delete this;
  }

  void OnFailsafeTimeout() { OnStartupComplete(); }

  // WebContentsObserver overrides
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    if (!render_frame_host->GetParent())
      OnStartupComplete();
  }

  void DidFailLoad(content::RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code,
                   const base::string16& error_description) override {
    if (!render_frame_host->GetParent())
      OnStartupComplete();
  }

  // Starting the browser with a file download url will not result in
  // DidFinishLoad firing, so watch for this case too. crbug.com/1006954
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->IsInMainFrame() && navigation_handle->IsDownload())
      OnStartupComplete();
  }

  void WebContentsDestroyed() override { OnStartupComplete(); }

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<StartupObserver> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(StartupObserver);
};

void StartupObserver::Start() {
  // Signal completion quickly when there is no first page to load.
  const int kShortDelaySecs = 3;
  base::TimeDelta delay = base::TimeDelta::FromSeconds(kShortDelaySecs);

#if !defined(OS_ANDROID)
  WebContents* contents = nullptr;
  for (auto* browser : *BrowserList::GetInstance()) {
    contents = browser->tab_strip_model()->GetActiveWebContents();
    if (contents && contents->GetMainFrame() &&
        contents->GetMainFrame()->GetVisibilityState() ==
            content::PageVisibilityState::kVisible) {
      break;
    }
  }

  if (contents) {
    // Give the page time to finish loading.
    const int kLongerDelayMins = 3;
    Observe(contents);
    delay = base::TimeDelta::FromMinutes(kLongerDelayMins);
  }
#else
  // Startup completion is signaled via AfterStartupTaskUtils.java,
  // this is just a failsafe timeout.
  const int kLongerDelayMins = 3;
  delay = base::TimeDelta::FromMinutes(kLongerDelayMins);
#endif  // !defined(OS_ANDROID)

  base::PostDelayedTask(FROM_HERE, {BrowserThread::UI},
                        base::BindOnce(&StartupObserver::OnFailsafeTimeout,
                                       weak_factory_.GetWeakPtr()),
                        delay);
}

}  // namespace

AfterStartupTaskUtils::Runner::Runner(
    scoped_refptr<base::TaskRunner> destination_runner)
    : destination_runner_(std::move(destination_runner)) {
  DCHECK(destination_runner_);
}

AfterStartupTaskUtils::Runner::~Runner() = default;

bool AfterStartupTaskUtils::Runner::PostDelayedTask(
    const base::Location& from_here,
    base::OnceClosure task,
    base::TimeDelta delay) {
  DCHECK(delay.is_zero());
  AfterStartupTaskUtils::PostTask(from_here, destination_runner_,
                                  std::move(task));
  return true;
}

bool AfterStartupTaskUtils::Runner::RunsTasksInCurrentSequence() const {
  return destination_runner_->RunsTasksInCurrentSequence();
}

void AfterStartupTaskUtils::StartMonitoringStartup() {
  // The observer is self-deleting.
  (new StartupObserver)->Start();
}

void AfterStartupTaskUtils::PostTask(
    const base::Location& from_here,
    const scoped_refptr<base::TaskRunner>& destination_runner,
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
  DCHECK(g_after_startup_tasks.Get().empty());
  if (!IsBrowserStartupComplete())
    return;
  g_startup_complete_flag.Get().UnsafeResetForTesting();
  DCHECK(!IsBrowserStartupComplete());
}

// static
void AfterStartupTaskUtils::DisableScheduleTaskDelayForTesting() {
  g_schedule_tasks_with_delay = false;
}
