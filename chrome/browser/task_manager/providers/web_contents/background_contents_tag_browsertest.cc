// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/task_manager/mock_web_contents_task_manager.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_tags_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "components/embedder_support/switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/switches.h"
#include "ui/base/l10n/l10n_util.h"

namespace task_manager {

// Defines a browser test for testing that BackgroundContents are tagged
// properly and the TagsManager records these tags. It is also used to test that
// the WebContentsTaskProvider will be able to provide the appropriate
// BackgroundContentsTask.
class BackgroundContentsTagTest : public extensions::ExtensionBrowserTest {
 public:
  BackgroundContentsTagTest() = default;
  BackgroundContentsTagTest(const BackgroundContentsTagTest&) = delete;
  BackgroundContentsTagTest& operator=(const BackgroundContentsTagTest&) =
      delete;
  ~BackgroundContentsTagTest() override = default;

  const extensions::Extension* LoadBackgroundExtension() {
    auto* extension = LoadExtension(
        test_data_dir_.AppendASCII("app_process_background_instances"));
    return extension;
  }

  std::u16string GetBackgroundTaskExpectedName(
      const extensions::Extension* extension) {
    return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_BACKGROUND_APP_PREFIX,
                                      base::UTF8ToUTF16(extension->name()));
  }

  WebContentsTagsManager* tags_manager() const {
    return WebContentsTagsManager::GetInstance();
  }

 protected:
  // extensions::ExtensionBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Pass flags to make testing apps easier.
    extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);
    test_data_dir_ = test_data_dir_.AppendASCII("api_test");
    command_line->AppendSwitch(switches::kDisableRendererBackgrounding);
    command_line->AppendSwitch(embedder_support::kDisablePopupBlocking);
    command_line->AppendSwitch(extensions::switches::kAllowHTTPBackgroundPage);
  }
};

// Tests that loading an extension that has a background contents will result in
// the tags manager recording a WebContentsTag.
IN_PROC_BROWSER_TEST_F(BackgroundContentsTagTest, TagsManagerRecordsATag) {
  // Browser tests start with only one tab available.
  EXPECT_EQ(1U, tags_manager()->tracked_tags().size());
  auto* extension = LoadBackgroundExtension();
  ASSERT_NE(nullptr, extension);
  EXPECT_EQ(2U, tags_manager()->tracked_tags().size());

  // Unload the extension.
  UnloadExtension(extension->id());
  EXPECT_EQ(1U, tags_manager()->tracked_tags().size());
}

// Tests that background contents creation while the provider is being observed
// will also provide tasks.
IN_PROC_BROWSER_TEST_F(BackgroundContentsTagTest, TasksProvidedWhileObserving) {
  MockWebContentsTaskManager task_manager;
  EXPECT_TRUE(task_manager.tasks().empty());
  // Browser tests start with only one tab available.
  EXPECT_EQ(1U, tags_manager()->tracked_tags().size());

  task_manager.StartObserving();

  // The pre-existing tab is provided.
  EXPECT_EQ(1U, task_manager.tasks().size());

  auto* extension = LoadBackgroundExtension();
  ASSERT_NE(nullptr, extension);
  EXPECT_EQ(2U, tags_manager()->tracked_tags().size());
  ASSERT_EQ(2U, task_manager.tasks().size());

  // Now check the newly provided task.
  const Task* task = task_manager.tasks().back();
  EXPECT_EQ(Task::RENDERER, task->GetType());
  EXPECT_EQ(GetBackgroundTaskExpectedName(extension), task->title());

  // Unload the extension.
  UnloadExtension(extension->id());
  EXPECT_EQ(1U, task_manager.tasks().size());
  EXPECT_EQ(1U, tags_manager()->tracked_tags().size());
}

// Tests providing a pre-existing background task to the observing operation.
IN_PROC_BROWSER_TEST_F(BackgroundContentsTagTest, PreExistingTasksAreProvided) {
  MockWebContentsTaskManager task_manager;
  EXPECT_TRUE(task_manager.tasks().empty());
  // Browser tests start with only one tab available.
  EXPECT_EQ(1U, tags_manager()->tracked_tags().size());
  auto* extension = LoadBackgroundExtension();
  ASSERT_NE(nullptr, extension);
  EXPECT_EQ(2U, tags_manager()->tracked_tags().size());

  task_manager.StartObserving();

  // Pre-existing task will be provided to us.
  ASSERT_EQ(2U, task_manager.tasks().size());

  // Now check the provided task.
  const Task* task = task_manager.tasks().back();
  EXPECT_EQ(Task::RENDERER, task->GetType());
  EXPECT_EQ(GetBackgroundTaskExpectedName(extension), task->title());

  // Unload the extension.
  UnloadExtension(extension->id());
  EXPECT_EQ(1U, task_manager.tasks().size());
  EXPECT_EQ(1U, tags_manager()->tracked_tags().size());
}

}  // namespace task_manager
