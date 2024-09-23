// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/task_manager/providers/task_provider_observer.h"
#include "chrome/browser/task_manager/providers/worker_task_provider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#endif

namespace task_manager {

namespace {

std::u16string ExpectedTaskTitle(const std::string& title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_SERVICE_WORKER_PREFIX,
                                    base::UTF8ToUTF16(title));
}

// Get the process id of the active WebContents for the passed |browser|.
int GetChildProcessID(Browser* browser) {
  return browser->tab_strip_model()
      ->GetActiveWebContents()
      ->GetPrimaryMainFrame()
      ->GetProcess()
      ->GetID();
}

}  // namespace

class WorkerTaskProviderBrowserTest : public InProcessBrowserTest,
                                      public TaskProviderObserver {
 public:
  WorkerTaskProviderBrowserTest() = default;

  ~WorkerTaskProviderBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void StartUpdating() {
    task_provider_ = std::make_unique<WorkerTaskProvider>();
    task_provider_->SetObserver(this);
  }

  void StopUpdating() {
    task_provider_->ClearObserver();
    tasks_.clear();
    task_provider_.reset();
  }

  Browser* CreateNewProfileAndSwitch() {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::FilePath new_path =
        profile_manager->GenerateNextProfileDirectoryPath();
    // Create an additional profile.
    profiles::testing::CreateProfileSync(profile_manager, new_path);

    profiles::SwitchToProfile(new_path, /* always_create = */ false,
                              base::DoNothing());
    BrowserList* browser_list = BrowserList::GetInstance();
    return *browser_list->begin_browsers_ordered_by_activation();
  }

  content::ServiceWorkerContext* GetServiceWorkerContext(Browser* browser) {
    return browser->profile()
        ->GetDefaultStoragePartition()
        ->GetServiceWorkerContext();
  }

  void WaitUntilTaskCount(uint64_t count) {
    if (tasks_.size() == count)
      return;

    expected_task_count_ = count;
    base::RunLoop loop;
    quit_closure_for_waiting_ = loop.QuitClosure();
    loop.Run();
  }

  // task_manager::TaskProviderObserver:
  void TaskAdded(Task* task) override {
    DCHECK(task);
    tasks_.push_back(task);

    if (expected_task_count_ == tasks_.size())
      StopWaiting();
  }

  void TaskRemoved(Task* task) override {
    DCHECK(task);
    std::erase(tasks_, task);

    if (expected_task_count_ == tasks_.size())
      StopWaiting();
  }

  const std::vector<raw_ptr<Task, VectorExperimental>>& tasks() const {
    return tasks_;
  }
  TaskProvider* task_provider() const { return task_provider_.get(); }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    command_line->AppendSwitch(
        ash::switches::kIgnoreUserProfileMappingForTests);
#endif
  }

  void StopWaiting() {
    if (quit_closure_for_waiting_)
      std::move(quit_closure_for_waiting_).Run();
  }

 private:
  std::unique_ptr<WorkerTaskProvider> task_provider_;

  // Tasks created by |task_provider_|.
  std::vector<raw_ptr<Task, VectorExperimental>> tasks_;

  base::OnceClosure quit_closure_for_waiting_;

  uint64_t expected_task_count_ = 0;
};

// Make sure that the WorkerTaskProvider can create/delete a WorkerTask of type
// SERVICE_WORKER based on the actual service worker status, and the task
// representing the service worker has the expected properties.
IN_PROC_BROWSER_TEST_F(WorkerTaskProviderBrowserTest,
                       CreateServiceWorkerTasksForSingleProfile) {
  StartUpdating();

  EXPECT_TRUE(tasks().empty());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                           "register('respond_with_fetch_worker.js');"));
  WaitUntilTaskCount(1);

  const Task* task = tasks()[0];
  EXPECT_EQ(task->GetChildProcessUniqueID(), GetChildProcessID(browser()));
  EXPECT_EQ(Task::SERVICE_WORKER, task->GetType());
  EXPECT_TRUE(base::StartsWith(
      task->title(),
      ExpectedTaskTitle(
          embedded_test_server()
              ->GetURL("/service_worker/respond_with_fetch_worker.js")
              .spec()),
      base::CompareCase::INSENSITIVE_ASCII));

  GetServiceWorkerContext(browser())->StopAllServiceWorkersForStorageKey(
      blink::StorageKey::CreateFirstParty(
          url::Origin::Create(embedded_test_server()->base_url())));
  WaitUntilTaskCount(0);

  StopUpdating();
}

// If the profile is off the record, the WorkerTaskProvider can still grab the
// correct information and create/delete the task.
IN_PROC_BROWSER_TEST_F(WorkerTaskProviderBrowserTest,
                       CreateServiceWorkerTasksForOffTheRecordProfile) {
  StartUpdating();

  EXPECT_TRUE(tasks().empty());
  Browser* incognito = CreateIncognitoBrowser();

  // Close the default browser.
  CloseBrowserSynchronously(browser());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      incognito, embedded_test_server()->GetURL(
                     "/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", EvalJs(incognito->tab_strip_model()->GetActiveWebContents(),
                           "register('respond_with_fetch_worker.js');"));
  WaitUntilTaskCount(1);

  const Task* task = tasks()[0];
  EXPECT_EQ(task->GetChildProcessUniqueID(), GetChildProcessID(incognito));
  EXPECT_EQ(Task::SERVICE_WORKER, task->GetType());
  EXPECT_TRUE(base::StartsWith(
      task->title(),
      ExpectedTaskTitle(
          embedded_test_server()
              ->GetURL("/service_worker/respond_with_fetch_worker.js")
              .spec()),
      base::CompareCase::INSENSITIVE_ASCII));

  GetServiceWorkerContext(incognito)->StopAllServiceWorkersForStorageKey(
      blink::StorageKey::CreateFirstParty(
          url::Origin::Create(embedded_test_server()->base_url())));
  WaitUntilTaskCount(0);

  StopUpdating();
  CloseBrowserSynchronously(incognito);
}

// If the profile are created dynamically and there is more than one profile
// simultaneously, the WorkerTaskProvider can still works.
// Flaky on all platforms. https://crrev.com/1244009.
IN_PROC_BROWSER_TEST_F(WorkerTaskProviderBrowserTest,
                       DISABLED_CreateTasksForMultiProfiles) {
  StartUpdating();

  EXPECT_TRUE(tasks().empty());

  const GURL kCreateServiceWorkerURL = embedded_test_server()->GetURL(
      "/service_worker/create_service_worker.html");

  Browser* browser_1 = CreateNewProfileAndSwitch();
  content::RenderFrameHost* render_frame_host_1 =
      ui_test_utils::NavigateToURL(browser_1, kCreateServiceWorkerURL);
  ASSERT_EQ(render_frame_host_1->GetLastCommittedURL(),
            kCreateServiceWorkerURL);
  EXPECT_EQ("DONE", EvalJs(render_frame_host_1,
                           "register('respond_with_fetch_worker.js');"));
  WaitUntilTaskCount(1);

  Browser* browser_2 = CreateNewProfileAndSwitch();
  content::RenderFrameHost* render_frame_host_2 =
      ui_test_utils::NavigateToURL(browser_2, kCreateServiceWorkerURL);
  ASSERT_EQ(render_frame_host_2->GetLastCommittedURL(),
            kCreateServiceWorkerURL);
  EXPECT_EQ("DONE", EvalJs(render_frame_host_2,
                           "register('respond_with_fetch_worker.js');"));
  WaitUntilTaskCount(2);

  const GURL kServiceWorkerURL = embedded_test_server()->GetURL(
      "/service_worker/respond_with_fetch_worker.js");

  const Task* task_1 = tasks()[0];
  EXPECT_EQ(task_1->GetChildProcessUniqueID(), GetChildProcessID(browser_1));
  EXPECT_EQ(Task::SERVICE_WORKER, task_1->GetType());
  EXPECT_TRUE(base::StartsWith(task_1->title(),
                               ExpectedTaskTitle(kServiceWorkerURL.spec()),
                               base::CompareCase::INSENSITIVE_ASCII));

  const Task* task_2 = tasks()[1];
  EXPECT_EQ(task_2->GetChildProcessUniqueID(), GetChildProcessID(browser_2));
  EXPECT_EQ(Task::SERVICE_WORKER, task_2->GetType());
  EXPECT_TRUE(base::StartsWith(task_2->title(),
                               ExpectedTaskTitle(kServiceWorkerURL.spec()),
                               base::CompareCase::INSENSITIVE_ASCII));

  GetServiceWorkerContext(browser_1)->StopAllServiceWorkersForStorageKey(
      blink::StorageKey::CreateFirstParty(
          url::Origin::Create(embedded_test_server()->base_url())));
  WaitUntilTaskCount(1);
  EXPECT_EQ(task_2, tasks()[0]);

  GetServiceWorkerContext(browser_2)->StopAllServiceWorkersForStorageKey(
      blink::StorageKey::CreateFirstParty(
          url::Origin::Create(embedded_test_server()->base_url())));
  WaitUntilTaskCount(0);

  StopUpdating();
  CloseBrowserSynchronously(browser_1);
  CloseBrowserSynchronously(browser_2);
}

IN_PROC_BROWSER_TEST_F(WorkerTaskProviderBrowserTest, CreateExistingTasks) {
  EXPECT_TRUE(tasks().empty());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                           "register('respond_with_fetch_worker.js');"));

  // No tasks yet as StartUpdating() wasn't called.
  EXPECT_TRUE(tasks().empty());

  StartUpdating();

  ASSERT_EQ(tasks().size(), 1u);
  const Task* task = tasks()[0];
  EXPECT_EQ(task->GetChildProcessUniqueID(), GetChildProcessID(browser()));
  EXPECT_EQ(Task::SERVICE_WORKER, task->GetType());
  EXPECT_TRUE(base::StartsWith(
      task->title(),
      ExpectedTaskTitle(
          embedded_test_server()
              ->GetURL("/service_worker/respond_with_fetch_worker.js")
              .spec()),
      base::CompareCase::INSENSITIVE_ASCII));

  GetServiceWorkerContext(browser())->StopAllServiceWorkersForStorageKey(
      blink::StorageKey::CreateFirstParty(
          url::Origin::Create(embedded_test_server()->base_url())));
  WaitUntilTaskCount(0);

  StopUpdating();
}

// Tests that destroying a profile while updating will correctly remove the
// existing tasks. An incognito browser is used because a regular profile is
// never truly destroyed until browser shutdown (See https://crbug.com/88586).
// TODO(crbug.com/40743320): Fix the flakiness and re-enable this.
IN_PROC_BROWSER_TEST_F(WorkerTaskProviderBrowserTest,
                       DISABLED_DestroyedProfile) {
  StartUpdating();

  EXPECT_TRUE(tasks().empty());
  Browser* browser = CreateIncognitoBrowser();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser, embedded_test_server()->GetURL(
                   "/service_worker/create_service_worker.html")));
  EXPECT_EQ("DONE", EvalJs(browser->tab_strip_model()->GetActiveWebContents(),
                           "register('respond_with_fetch_worker.js');"));
  WaitUntilTaskCount(1);

  const Task* task_1 = tasks()[0];
  EXPECT_EQ(task_1->GetChildProcessUniqueID(), GetChildProcessID(browser));
  EXPECT_EQ(Task::SERVICE_WORKER, task_1->GetType());
  EXPECT_TRUE(base::StartsWith(
      task_1->title(),
      ExpectedTaskTitle(
          embedded_test_server()
              ->GetURL("/service_worker/respond_with_fetch_worker.js")
              .spec()),
      base::CompareCase::INSENSITIVE_ASCII));

  CloseBrowserSynchronously(browser);

  WaitUntilTaskCount(0);

  StopUpdating();
}

}  // namespace task_manager
