// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/task_manager/mock_web_contents_task_manager.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/test_image_loader.h"
#include "extensions/common/constants.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/skia_util.h"

namespace task_manager {

class ExtensionTagsTest : public extensions::ExtensionBrowserTest {
 public:
  ExtensionTagsTest() = default;
  ExtensionTagsTest(const ExtensionTagsTest&) = delete;
  ExtensionTagsTest& operator=(const ExtensionTagsTest&) = delete;
  ~ExtensionTagsTest() override = default;

 protected:
  // If no extension task was found, a nullptr will be returned.
  Task* FindAndGetExtensionTask(
      const MockWebContentsTaskManager& task_manager) {
    auto itr = base::ranges::find(task_manager.tasks(), Task::EXTENSION,
                                  &Task::GetType);

    return itr != task_manager.tasks().end() ? *itr : nullptr;
  }

  const std::vector<raw_ptr<WebContentsTag, VectorExperimental>>& tracked_tags()
      const {
    return WebContentsTagsManager::GetInstance()->tracked_tags();
  }
};

// Tests loading, disabling, enabling and unloading extensions and how that will
// affect the recording of tags.
IN_PROC_BROWSER_TEST_F(ExtensionTagsTest, Basic) {
  // Browser tests start with a single tab.
  EXPECT_EQ(1U, tracked_tags().size());

  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
          .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
          .AppendASCII("1.0.0.0"));
  ASSERT_TRUE(extension);

  EXPECT_EQ(2U, tracked_tags().size());

  DisableExtension(extension->id());
  EXPECT_EQ(1U, tracked_tags().size());

  EnableExtension(extension->id());
  EXPECT_EQ(2U, tracked_tags().size());

  UnloadExtension(extension->id());
  EXPECT_EQ(1U, tracked_tags().size());
}

// Disabled due to flakiness, see crbug.com/519333 and crbug.com/639185.
IN_PROC_BROWSER_TEST_F(ExtensionTagsTest,
                       DISABLED_PreAndPostExistingTaskProviding) {
  // Browser tests start with a single tab.
  EXPECT_EQ(1U, tracked_tags().size());
  MockWebContentsTaskManager task_manager;
  EXPECT_TRUE(task_manager.tasks().empty());

  const extensions::Extension* extension = LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
          .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
          .AppendASCII("1.0.0.0"));
  ASSERT_TRUE(extension);

  EXPECT_EQ(2U, tracked_tags().size());
  EXPECT_TRUE(task_manager.tasks().empty());

  // Start observing, pre-existing tasks will be provided.
  task_manager.StartObserving();

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  ASSERT_EQ(2U, task_manager.tasks().size());
  const Task* extension_task = FindAndGetExtensionTask(task_manager);
  ASSERT_TRUE(extension_task);

  SkBitmap expected_bitmap =
      extensions::TestImageLoader::LoadAndGetExtensionBitmap(
          extension,
          "icon_128.png",
          extension_misc::EXTENSION_ICON_SMALL);
  ASSERT_FALSE(expected_bitmap.empty());

  EXPECT_TRUE(gfx::BitmapsAreEqual(*extension_task->icon().bitmap(),
                                   expected_bitmap));

  // Unload the extension and expect that the task manager now shows only the
  // about:blank tab.
  UnloadExtension(extension->id());
  EXPECT_EQ(1U, tracked_tags().size());
  ASSERT_EQ(1U, task_manager.tasks().size());
  const Task* about_blank_task = task_manager.tasks().back();
  EXPECT_EQ(Task::RENDERER, about_blank_task->GetType());
  EXPECT_EQ(u"Tab: about:blank", about_blank_task->title());

  // Reload the extension, the task manager should show it again.
  ReloadExtension(extension->id());
  EXPECT_EQ(2U, tracked_tags().size());
  ASSERT_EQ(2U, task_manager.tasks().size());
  extension_task = FindAndGetExtensionTask(task_manager);
  ASSERT_TRUE(extension_task);
}

}  // namespace task_manager

