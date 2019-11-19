// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/task_manager/providers/service_worker_task_provider.h"
#include "chrome/browser/task_manager/providers/task_provider_observer.h"
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
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chromeos/constants/chromeos_switches.h"
#endif

namespace task_manager {

namespace {

void OnUnblockOnProfileCreation(base::RunLoop* run_loop,
                                Profile* profile,
                                Profile::CreateStatus status) {
  if (status == Profile::CREATE_STATUS_INITIALIZED)
    run_loop->Quit();
}

base::string16 ExpectedTaskTitle(const std::string& title) {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_SERVICE_WORKER_PREFIX,
                                    base::UTF8ToUTF16(title));
}

// Get the process id of the active WebContents for the passed |browser|.
int GetChildProcessID(Browser* browser) {
  return browser->tab_strip_model()
      ->GetActiveWebContents()
      ->GetMainFrame()
      ->GetProcess()
      ->GetID();
}

}  // namespace

class ServiceWorkerTaskProviderBrowserTest : public InProcessBrowserTest,
                                             public TaskProviderObserver {
 public:
  ServiceWorkerTaskProviderBrowserTest() = default;

  ~ServiceWorkerTaskProviderBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void StartUpdating() {
    task_provider_ = std::make_unique<ServiceWorkerTaskProvider>();
    task_provider_->SetObserver(this);
  }

  void StopUpdating() {
    task_provider_->ClearObserver();
    tasks_.clear();
    task_provider_.reset();
  }

  Browser* CreateNewProfileAndSwitch() {
    ProfileManager* profile_manager = g_browser_process->profile_manager();

    // Create an additional profile.
    base::FilePath new_path =
        profile_manager->GenerateNextProfileDirectoryPath();
    base::RunLoop run_loop;
    profile_manager->CreateProfileAsync(
        new_path, base::BindRepeating(&OnUnblockOnProfileCreation, &run_loop),
        base::string16(), std::string());
    run_loop.Run();

    profiles::SwitchToProfile(new_path, /* always_create = */ false,
                              base::DoNothing(),
                              ProfileMetrics::SWITCH_PROFILE_ICON);
    BrowserList* browser_list = BrowserList::GetInstance();
    return *browser_list->begin_last_active();
  }

  content::ServiceWorkerContext* GetServiceWorkerContext(Browser* browser) {
    return content::BrowserContext::GetDefaultStoragePartition(
               browser->profile())
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
    base::Erase(tasks_, task);

    if (expected_task_count_ == tasks_.size())
      StopWaiting();
  }

  const std::vector<Task*>& tasks() const { return tasks_; }
  TaskProvider* task_provider() const { return task_provider_.get(); }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
#if defined(OS_CHROMEOS)
    command_line->AppendSwitch(
        chromeos::switches::kIgnoreUserProfileMappingForTests);
#endif
  }

  void StopWaiting() {
    if (quit_closure_for_waiting_)
      std::move(quit_closure_for_waiting_).Run();
  }

 private:
  std::unique_ptr<ServiceWorkerTaskProvider> task_provider_;

  // Tasks created by |task_provider_|.
  std::vector<Task*> tasks_;

  base::OnceClosure quit_closure_for_waiting_;

  uint64_t expected_task_count_ = 0;
};

// Make sure that the ServiceWorkerTaskProvider can create/delete
// a ServiceWorkerTask based on the actual service worker status, and the task
// representing the service worker has the expected properties.
IN_PROC_BROWSER_TEST_F(ServiceWorkerTaskProviderBrowserTest,
                       CreateTasksForSingleProfile) {
  StartUpdating();

  EXPECT_TRUE(tasks().empty());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/service_worker/create_service_worker.html"));
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

  GetServiceWorkerContext(browser())->StopAllServiceWorkersForOrigin(
      embedded_test_server()->base_url());
  WaitUntilTaskCount(0);

  StopUpdating();
}

// If the profile is off the record, the ServiceWorkerTaskProvider can still
// grab the correct information and create/delete the task.
IN_PROC_BROWSER_TEST_F(ServiceWorkerTaskProviderBrowserTest,
                       CreateTasksForOffTheRecordProfile) {
  StartUpdating();

  EXPECT_TRUE(tasks().empty());
  Browser* incognito = CreateIncognitoBrowser();

  // Close the default browser.
  CloseBrowserSynchronously(browser());

  ui_test_utils::NavigateToURL(
      incognito, embedded_test_server()->GetURL(
                     "/service_worker/create_service_worker.html"));
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

  GetServiceWorkerContext(incognito)->StopAllServiceWorkersForOrigin(
      embedded_test_server()->base_url());
  WaitUntilTaskCount(0);

  StopUpdating();
  CloseBrowserSynchronously(incognito);
}

// If the profile are created dynamically and there is more than one profile
// simultaneously, the ServiceWorkerTaskProvider can still works.
IN_PROC_BROWSER_TEST_F(ServiceWorkerTaskProviderBrowserTest,
                       CreateTasksForMultiProfiles) {
  StartUpdating();

  EXPECT_TRUE(tasks().empty());
  Browser* browser_1 = CreateNewProfileAndSwitch();
  ui_test_utils::NavigateToURL(
      browser_1, embedded_test_server()->GetURL(
                     "/service_worker/create_service_worker.html"));
  EXPECT_EQ("DONE", EvalJs(browser_1->tab_strip_model()->GetActiveWebContents(),
                           "register('respond_with_fetch_worker.js');"));
  WaitUntilTaskCount(1);

  Browser* browser_2 = CreateNewProfileAndSwitch();
  ui_test_utils::NavigateToURL(
      browser_2, embedded_test_server()->GetURL(
                     "/service_worker/create_service_worker.html"));
  EXPECT_EQ("DONE", EvalJs(browser_2->tab_strip_model()->GetActiveWebContents(),
                           "register('respond_with_fetch_worker.js');"));
  WaitUntilTaskCount(2);

  const Task* task_1 = tasks()[0];
  EXPECT_EQ(task_1->GetChildProcessUniqueID(), GetChildProcessID(browser_1));
  EXPECT_EQ(Task::SERVICE_WORKER, task_1->GetType());
  EXPECT_TRUE(base::StartsWith(
      task_1->title(),
      ExpectedTaskTitle(
          embedded_test_server()
              ->GetURL("/service_worker/respond_with_fetch_worker.js")
              .spec()),
      base::CompareCase::INSENSITIVE_ASCII));

  const Task* task_2 = tasks()[1];
  EXPECT_EQ(task_2->GetChildProcessUniqueID(), GetChildProcessID(browser_2));
  EXPECT_EQ(Task::SERVICE_WORKER, task_2->GetType());
  EXPECT_TRUE(base::StartsWith(
      task_2->title(),
      ExpectedTaskTitle(
          embedded_test_server()
              ->GetURL("/service_worker/respond_with_fetch_worker.js")
              .spec()),
      base::CompareCase::INSENSITIVE_ASCII));

  GetServiceWorkerContext(browser_1)->StopAllServiceWorkersForOrigin(
      embedded_test_server()->base_url());
  WaitUntilTaskCount(1);
  EXPECT_EQ(task_2, tasks()[0]);

  GetServiceWorkerContext(browser_2)->StopAllServiceWorkersForOrigin(
      embedded_test_server()->base_url());
  WaitUntilTaskCount(0);

  StopUpdating();
  CloseBrowserSynchronously(browser_1);
  CloseBrowserSynchronously(browser_2);
}

}  // namespace task_manager
