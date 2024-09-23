// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/processes/processes_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/extensions/api/processes.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"

class ProcessesApiTest : public extensions::ExtensionApiTest {
 public:
  ProcessesApiTest() {}

  ProcessesApiTest(const ProcessesApiTest&) = delete;
  ProcessesApiTest& operator=(const ProcessesApiTest&) = delete;

  ~ProcessesApiTest() override {}

  int GetListenersCount() {
    return extensions::ProcessesAPI::Get(profile())->
        processes_event_router()->listeners_;
  }
};


// This test is flaky. https://crbug.com/598445
IN_PROC_BROWSER_TEST_F(ProcessesApiTest, DISABLED_Processes) {
  ASSERT_TRUE(RunExtensionTest("processes/api")) << message_;
}

IN_PROC_BROWSER_TEST_F(ProcessesApiTest, DISABLED_ProcessesApiListeners) {
  EXPECT_EQ(0, GetListenersCount());

  // Load extension that adds a listener in background page
  ExtensionTestMessageListener listener1("ready");
  const extensions::Extension* extension1 = LoadExtension(
      test_data_dir_.AppendASCII("processes").AppendASCII("onupdated"));
  ASSERT_TRUE(extension1);
  ASSERT_TRUE(listener1.WaitUntilSatisfied());

  // The memory refresh type of the task manager may or may not be enabled by
  // now depending on the presence of other task manager observers.
  // Ensure the listeners count has changed.
  EXPECT_EQ(1, GetListenersCount());

  // Load another extension that listen to the onUpdatedWithMemory.
  ExtensionTestMessageListener listener2("ready");
  const extensions::Extension* extension2 = LoadExtension(
      test_data_dir_.AppendASCII("processes").AppendASCII(
          "onupdated_with_memory"));
  ASSERT_TRUE(extension2);
  ASSERT_TRUE(listener2.WaitUntilSatisfied());

  // The memory refresh type must be enabled now.
  const task_manager::TaskManagerInterface* task_manager =
      task_manager::TaskManagerInterface::GetTaskManager();
  EXPECT_EQ(2, GetListenersCount());
  EXPECT_TRUE(task_manager->IsResourceRefreshEnabled(
      task_manager::REFRESH_TYPE_MEMORY_FOOTPRINT));

  // Unload the extensions and make sure the listeners count is updated.
  UnloadExtension(extension2->id());
  EXPECT_EQ(1, GetListenersCount());
  UnloadExtension(extension1->id());
  EXPECT_EQ(0, GetListenersCount());
}

IN_PROC_BROWSER_TEST_F(ProcessesApiTest, OnUpdatedWithMemoryRefreshTypes) {
  EXPECT_EQ(0, GetListenersCount());

  // Load an extension that listen to the onUpdatedWithMemory.
  ExtensionTestMessageListener listener("ready");
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("processes")
                        .AppendASCII("onupdated_with_memory"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // The memory refresh type must be enabled now.
  const task_manager::TaskManagerInterface* task_manager =
      task_manager::TaskManagerInterface::GetTaskManager();
  EXPECT_EQ(1, GetListenersCount());
  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(profile());
  EXPECT_TRUE(event_router->HasEventListener(
      extensions::api::processes::OnUpdatedWithMemory::kEventName));
  EXPECT_FALSE(event_router->HasEventListener(
      extensions::api::processes::OnUpdated::kEventName));
  EXPECT_TRUE(task_manager->IsResourceRefreshEnabled(
      task_manager::REFRESH_TYPE_MEMORY_FOOTPRINT));

  // Despite the fact that there are no onUpdated listeners, refresh types for
  // CPU, Network, SQLite, V8 memory, and webcache stats should be enabled.
  constexpr task_manager::RefreshType kOnUpdatedRefreshTypes[] = {
      task_manager::REFRESH_TYPE_CPU,
      task_manager::REFRESH_TYPE_NETWORK_USAGE,
      task_manager::REFRESH_TYPE_SQLITE_MEMORY,
      task_manager::REFRESH_TYPE_V8_MEMORY,
      task_manager::REFRESH_TYPE_WEBCACHE_STATS,
  };

  for (const auto& type : kOnUpdatedRefreshTypes)
    EXPECT_TRUE(task_manager->IsResourceRefreshEnabled(type));

  // Unload the extensions and make sure the listeners count is updated.
  UnloadExtension(extension->id());
  EXPECT_EQ(0, GetListenersCount());
}

// This test is flaky on Linux and ChromeOS ASan LSan Tests bot. https://crbug.com/1028778
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && \
    defined(ADDRESS_SANITIZER)
#define MAYBE_CannotTerminateBrowserProcess \
  DISABLED_CannotTerminateBrowserProcess
#else
#define MAYBE_CannotTerminateBrowserProcess CannotTerminateBrowserProcess
#endif
IN_PROC_BROWSER_TEST_F(ProcessesApiTest, MAYBE_CannotTerminateBrowserProcess) {
  ASSERT_TRUE(RunExtensionTest("processes/terminate-browser-process"))
      << message_;
}
