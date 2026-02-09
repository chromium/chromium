// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/background/background_contents_test_waiter.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/preloading/scoped_prewarm_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/mock_web_contents_task_manager.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_tags_manager.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/embedder_support/switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/switches.h"
#include "ui/base/l10n/l10n_util.h"

namespace task_manager {

// Defines a browser test for testing that BackgroundContents are tagged
// properly and the TagsManager records these tags. It is also used to test that
// the WebContentsTaskProvider will be able to provide the appropriate
// BackgroundContentsTask.
class BackgroundContentsTagTest : public extensions::ExtensionBrowserTest {
 public:
  BackgroundContentsTagTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features*/ {omnibox::kWebUIOmniboxPopup,
                              omnibox::internal::kWebUIOmniboxAimPopup},
        /*disabled_features*/ {});
  }
  BackgroundContentsTagTest(const BackgroundContentsTagTest&) = delete;
  BackgroundContentsTagTest& operator=(const BackgroundContentsTagTest&) =
      delete;
  ~BackgroundContentsTagTest() override = default;

  const extensions::Extension* LoadBackgroundExtension() {
    auto* extension = LoadExtension(
        test_data_dir_.AppendASCII("app_process_background_instances"));
    // Wait for the hosted app's background page to start up. Normally, this
    // is handled by `LoadExtension()`, but only for extension-types (not hosted
    // apps).
    BackgroundContentsTestWaiter(profile()).WaitForBackgroundContents(
        extension->id());
    return extension;
  }

  std::string GetBackgroundTaskExpectedName(
      const extensions::Extension* extension) {
    return l10n_util::GetStringFUTF8(IDS_TASK_MANAGER_BACKGROUND_APP_PREFIX,
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

 private:
  // TODO(https://crbug.com/423465927): Explore a better approach to make the
  // existing tests run with the prewarm feature enabled.
  test::ScopedPrewarmFeatureList scoped_prewarm_feature_list_{
      test::ScopedPrewarmFeatureList::PrewarmState::kDisabled};
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that loading an extension that has a background contents will result in
// the tags manager recording a WebContentsTag.
IN_PROC_BROWSER_TEST_F(BackgroundContentsTagTest, TagsManagerRecordsATag) {
  // Browser tests start with only one tab available & two omnibox tags.
  EXPECT_THAT(
      ui_test_utils::GetAllTrackedTagWebContentTitles(),
      testing::ElementsAre("Omnibox Popup", "Omnibox Popup", "about:blank"));
  auto* extension = LoadBackgroundExtension();
  ASSERT_NE(extension, nullptr);
  EXPECT_THAT(
      ui_test_utils::GetAllTrackedTagWebContentTitles(),
      testing::ElementsAre("Omnibox Popup", "Omnibox Popup", "about:blank",
                           testing::Not(testing::IsEmpty())));

  // Unload the extension.
  UnloadExtension(extension->id());
  EXPECT_THAT(
      ui_test_utils::GetAllTrackedTagWebContentTitles(),
      testing::ElementsAre("Omnibox Popup", "Omnibox Popup", "about:blank"));
}

// Tests that background contents creation while the provider is being observed
// will also provide tasks.
IN_PROC_BROWSER_TEST_F(BackgroundContentsTagTest, TasksProvidedWhileObserving) {
  MockWebContentsTaskManager task_manager;
  EXPECT_TRUE(task_manager.tasks().empty());
  // Browser tests start with only one tab available & two omnibox tags.
  EXPECT_THAT(
      ui_test_utils::GetAllTrackedTagWebContentTitles(),
      testing::ElementsAre("Omnibox Popup", "Omnibox Popup", "about:blank"));

  task_manager.StartObserving();

  // The pre-existing tab is provided.
  EXPECT_THAT(task_manager.TaskTitles(),
              testing::ElementsAre("Tool: Omnibox", "Tool: Omnibox",
                                   "Tab: about:blank"));

  auto* extension = LoadBackgroundExtension();
  ASSERT_NE(extension, nullptr);
  EXPECT_THAT(
      ui_test_utils::GetAllTrackedTagWebContentTitles(),
      testing::ElementsAre("Omnibox Popup", "Omnibox Popup", "about:blank",
                           testing::Not(testing::IsEmpty())));
  ASSERT_THAT(
      task_manager.TaskTitles(),
      testing::ElementsAre("Tool: Omnibox", "Tool: Omnibox", "Tab: about:blank",
                           GetBackgroundTaskExpectedName(extension)));

  // Now check the newly provided task.
  EXPECT_EQ(task_manager.tasks()[2]->GetType(), Task::RENDERER);

  // Unload the extension.
  UnloadExtension(extension->id());
  EXPECT_THAT(task_manager.TaskTitles(),
              testing::ElementsAre("Tool: Omnibox", "Tool: Omnibox",
                                   "Tab: about:blank"));
  EXPECT_THAT(
      ui_test_utils::GetAllTrackedTagWebContentTitles(),
      testing::ElementsAre("Omnibox Popup", "Omnibox Popup", "about:blank"));
}

// Tests providing a pre-existing background task to the observing operation.
IN_PROC_BROWSER_TEST_F(BackgroundContentsTagTest, PreExistingTasksAreProvided) {
  MockWebContentsTaskManager task_manager;
  EXPECT_TRUE(task_manager.tasks().empty());
  // Browser tests start with only one tab available & 2 omnibox tags.
  EXPECT_THAT(
      ui_test_utils::GetAllTrackedTagWebContentTitles(),
      testing::ElementsAre("Omnibox Popup", "Omnibox Popup", "about:blank"));
  auto* extension = LoadBackgroundExtension();
  ASSERT_NE(nullptr, extension);
  EXPECT_THAT(
      ui_test_utils::GetAllTrackedTagWebContentTitles(),
      testing::ElementsAre("Omnibox Popup", "Omnibox Popup", "about:blank",
                           testing::Not(testing::IsEmpty())));

  task_manager.StartObserving();

  // Pre-existing task will be provided to us.
  ASSERT_THAT(
      task_manager.TaskTitles(),
      testing::ElementsAre("Tool: Omnibox", "Tool: Omnibox", "Tab: about:blank",
                           GetBackgroundTaskExpectedName(extension)));

  // Now check the provided task.
  EXPECT_EQ(task_manager.tasks().back()->GetType(), Task::RENDERER);

  // Unload the extension.
  UnloadExtension(extension->id());
  EXPECT_THAT(task_manager.TaskTitles(),
              testing::ElementsAre("Tool: Omnibox", "Tool: Omnibox",
                                   "Tab: about:blank"));
  EXPECT_THAT(
      ui_test_utils::GetAllTrackedTagWebContentTitles(),
      testing::ElementsAre("Omnibox Popup", "Omnibox Popup", "about:blank"));
}

}  // namespace task_manager
