// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/web_file_tasks.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/chromeos/file_manager/file_tasks.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/test/test_app_registrar.h"
#include "chrome/browser/web_applications/test/test_file_handler_manager.h"
#include "chrome/browser/web_applications/test/test_web_app_provider.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/entry_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace file_manager {
namespace file_tasks {

class WebFileTasksTest : public testing::Test {
 protected:
  WebFileTasksTest() {}

  void SetUp() override {
    app_provider_ = web_app::TestWebAppProvider::Get(&profile_);

    auto app_registrar = std::make_unique<web_app::TestAppRegistrar>();
    app_registrar_ = app_registrar.get();
    app_provider_->SetRegistrar(std::move(app_registrar));

    auto file_handler_manager =
        std::make_unique<web_app::TestFileHandlerManager>();
    file_handler_manager_ = file_handler_manager.get();
    app_provider_->SetFileHandlerManager(std::move(file_handler_manager));

    app_provider_->Start();
  }

  void InstallFileHandler(const web_app::AppId& app_id,
                          const GURL& install_url,
                          const std::vector<std::string> accepts) {
    app_registrar_->AddExternalApp(app_id, {install_url});
    file_handler_manager_->InstallFileHandler(app_id, install_url, accepts);
  }

  Profile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  web_app::TestWebAppProvider* app_provider_;
  web_app::TestAppRegistrar* app_registrar_;
  web_app::TestFileHandlerManager* file_handler_manager_;
};

TEST_F(WebFileTasksTest, WebAppFileHandlingCanBeDisabled) {
  const char kGraphrId[] = "graphr-app-id";
  const char kGraphrAction[] = "https://graphr.tld/csv";
  InstallFileHandler(kGraphrId, GURL(kGraphrAction), {".csv", "text/csv"});

  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(
      util::GetMyFilesFolderForProfile(profile()).AppendASCII("foo.csv"),
      "text/csv", false);

  std::vector<FullTaskDescriptor> tasks;

  {
    // Web Apps should not be able to handle files unless
    // kNativeFileSystemAPI and kFileHandlingAPI are enabled.
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures({},
                                         {blink::features::kNativeFileSystemAPI,
                                          blink::features::kFileHandlingAPI});
    FindWebTasks(profile(), entries, &tasks);
    EXPECT_EQ(0u, tasks.size());
    tasks.clear();
  }

  {
    // When the flags are enabled, it should be possible to handle files from
    // bookmark apps.
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures({blink::features::kNativeFileSystemAPI,
                                          blink::features::kFileHandlingAPI},
                                         {});
    // Test that when enabled, bookmark apps can handle files
    FindWebTasks(profile(), entries, &tasks);
    // Graphr should be a valid handler.
    ASSERT_EQ(1u, tasks.size());
    EXPECT_EQ(kGraphrId, tasks[0].task_descriptor().app_id);
    EXPECT_EQ(kGraphrAction, tasks[0].task_descriptor().action_id);
    EXPECT_EQ(file_tasks::TaskType::TASK_TYPE_WEB_APP,
              tasks[0].task_descriptor().task_type);
  }
}

TEST_F(WebFileTasksTest, FindWebFileHandlerTasks) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({blink::features::kNativeFileSystemAPI,
                                        blink::features::kFileHandlingAPI},
                                       {});
  const char kFooId[] = "foo-app-id";
  const char kFooAction[] = "https://foo.tld/files";

  const char kBarId[] = "bar-app-id";
  const char kBarAction[] = "https://bar.tld/files";

  // Foo can handle "text/plain" and "text/html".
  InstallFileHandler(kFooId, GURL(kFooAction), {"text/plain", "text/html"});
  // Bar can only handle "text/plain".
  InstallFileHandler(kBarId, GURL(kBarAction), {"text/plain"});

  // Find apps for a "text/plain" file. Both Foo and Bar should be found.
  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(
      util::GetMyFilesFolderForProfile(profile()).AppendASCII("foo.txt"),
      "text/plain", false);

  std::vector<FullTaskDescriptor> tasks;
  FindWebTasks(profile(), entries, &tasks);

  // Expect both apps to be found.
  ASSERT_EQ(2U, tasks.size());
  std::vector<std::string> app_ids = {tasks[0].task_descriptor().app_id,
                                      tasks[1].task_descriptor().app_id};
  EXPECT_THAT(app_ids, testing::UnorderedElementsAre(kFooId, kBarId));

  // Add a "text/html" file. Only Foo should be found.
  entries.emplace_back(
      util::GetMyFilesFolderForProfile(profile()).AppendASCII("foo.html"),
      "text/html", false);
  tasks.clear();
  FindWebTasks(profile(), entries, &tasks);
  // Confirm only Foo was found.
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kFooId, tasks[0].task_descriptor().app_id);

  // Add an "image/png" file. No tasks should be found.
  entries.emplace_back(
      util::GetMyFilesFolderForProfile(profile()).AppendASCII("foo.png"),
      "image/png", false);
  tasks.clear();
  FindWebTasks(profile(), entries, &tasks);
}

TEST_F(WebFileTasksTest, FindWebFileHandlerTask_Generic) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({blink::features::kNativeFileSystemAPI,
                                        blink::features::kFileHandlingAPI},
                                       {});

  const char kBarId[] = "bar-app-id";
  const char kBarAction[] = "https://bar.tld/files";

  const char kBazId[] = "baz-app-id";
  const char kBazAction[] = "https://baz.tld/files";

  const char kFooId[] = "foo-app-id";
  const char kFooAction[] = "https://foo.tld/files";

  const char kQuxId[] = "qux-app-id";
  const char kQuxAction[] = "https://qux.tld/files";

  // Task sorter, to ensure a stable ordering.
  auto task_sorter = [](const FullTaskDescriptor& first,
                        const FullTaskDescriptor& second) -> int {
    return first.task_descriptor().app_id < second.task_descriptor().app_id;
  };

  // Bar provides a file handler for .txt files, and has no generic handler.
  InstallFileHandler(kBarId, GURL(kBarAction), {".txt"});

  // Baz provides a file handler for all extensions and all images.
  InstallFileHandler(kBazId, GURL(kBazAction), {".*"});
  InstallFileHandler(kBazId, GURL(kBazAction), {"image/*"});

  // Foo provides a file handler for "text/plain" and "*/*" <-- All file types.
  InstallFileHandler(kFooId, GURL(kFooAction), {"text/plain"});
  InstallFileHandler(kFooId, GURL(kFooAction), {"*/*"});

  // Qux provides a file handler for all file types.
  InstallFileHandler(kQuxId, GURL(kQuxAction), {"*"});

  std::vector<extensions::EntryInfo> entries;
  std::vector<FullTaskDescriptor> tasks;

  // All apps should be able to handle ".txt" files.
  entries.emplace_back(
      util::GetMyFilesFolderForProfile(profile()).AppendASCII("foo.txt"),
      "text/plain", false);
  FindWebTasks(profile(), entries, &tasks);
  // Ensure stable order.
  std::sort(tasks.begin(), tasks.end(), task_sorter);
  ASSERT_EQ(4U, tasks.size());
  // Bar provides a handler for ".txt".
  EXPECT_EQ(kBarId, tasks[0].task_descriptor().app_id);
  EXPECT_FALSE(tasks[0].is_generic_file_handler());
  // Baz provides a handler for all extensions.
  EXPECT_EQ(kBazId, tasks[1].task_descriptor().app_id);
  EXPECT_TRUE(tasks[1].is_generic_file_handler());
  // Foo provides a handler for "text/plain".
  EXPECT_EQ(kFooId, tasks[2].task_descriptor().app_id);
  EXPECT_FALSE(tasks[2].is_generic_file_handler());
  // Qux provides a handler for all file types.
  EXPECT_EQ(kQuxId, tasks[3].task_descriptor().app_id);
  EXPECT_TRUE(tasks[3].is_generic_file_handler());

  // Reset entries and tasks.
  entries.clear();
  tasks.clear();

  // Every app but Bar should be able to handle jpegs.
  entries.emplace_back(
      util::GetMyFilesFolderForProfile(profile()).AppendASCII("foo.jpg"),
      "image/jpeg", false);
  FindWebTasks(profile(), entries, &tasks);
  // Ensure stable order.,
  std::sort(tasks.begin(), tasks.end(), task_sorter);
  ASSERT_EQ(3U, tasks.size());
  // Baz provides a handler for "image/*".
  EXPECT_EQ(kBazId, tasks[0].task_descriptor().app_id);
  EXPECT_FALSE(tasks[0].is_generic_file_handler());
  // Foo provides a handler for all types.
  EXPECT_EQ(kFooId, tasks[1].task_descriptor().app_id);
  EXPECT_TRUE(tasks[1].is_generic_file_handler());
  // Qux provides a handler for all types.
  EXPECT_EQ(kQuxId, tasks[2].task_descriptor().app_id);
  EXPECT_TRUE(tasks[2].is_generic_file_handler());
}

}  // namespace file_tasks
}  // namespace file_manager
