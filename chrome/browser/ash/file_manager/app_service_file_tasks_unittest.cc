// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/app_service_file_tasks.h"

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/intent_test_util.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/entry_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using extensions::api::file_manager_private::Verb;

namespace {
const char kAppIdText[] = "abcdefg";
const char kAppIdImage[] = "gfedcba";
const char kAppIdAny[] = "hijklmn";
const char kMimeTypeText[] = "text/plain";
const char kMimeTypeImage[] = "image/jpeg";
const char kMimeTypeAny[] = "*/*";
const char kFileExtensionText[] = "txt";
const char kFileExtensionImage[] = "jpeg";
const char kFileExtensionAny[] = "fake";
const char kActivityLabelText[] = "some_text_activity";
const char kActivityLabelImage[] = "some_image_activity";
const char kActivityLabelAny[] = "some_any_file";
}  // namespace

namespace file_manager {
namespace file_tasks {

class AppServiceFileTasksTest : public testing::Test {
 protected:
  AppServiceFileTasksTest() {}
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    app_service_test_.SetUp(profile_.get());
    app_service_proxy_ =
        apps::AppServiceProxyFactory::GetForProfile(profile_.get());
    ASSERT_TRUE(app_service_proxy_);
  }

  Profile* profile() { return profile_.get(); }

  void AddFakeAppWithIntentFilters(
      const std::string& app_id,
      std::vector<apps::mojom::IntentFilterPtr> intent_filters,
      apps::mojom::AppType app_type) {
    std::vector<apps::mojom::AppPtr> apps;
    auto app = apps::mojom::App::New();
    app->app_id = app_id;
    app->app_type = app_type;
    app->show_in_launcher = apps::mojom::OptionalBool::kTrue;
    app->readiness = apps::mojom::Readiness::kReady;
    app->intent_filters = std::move(intent_filters);
    apps.push_back(std::move(app));
    app_service_proxy_->AppRegistryCache().OnApps(
        std::move(apps), app_type, false /* should_notify_initialized */);
    app_service_test_.WaitForAppService();
  }

  void AddFakeWebApp(const std::string& app_id,
                     const std::string& mime_type,
                     const std::string& file_extension,
                     const std::string& activity_label) {
    auto mime_filter =
        apps_util::CreateMimeTypeIntentFilterForView(mime_type, activity_label);
    auto file_ext_filter = apps_util::CreateFileExtensionIntentFilterForView(
        file_extension, activity_label);
    std::vector<apps::mojom::IntentFilterPtr> filters;
    filters.push_back(std::move(mime_filter));
    filters.push_back(std::move(file_ext_filter));
    AddFakeAppWithIntentFilters(app_id, std::move(filters),
                                apps::mojom::AppType::kWeb);
  }

  void AddTextApp() {
    AddFakeWebApp(kAppIdText, kMimeTypeText, kFileExtensionText,
                  kActivityLabelText);
  }

  void AddImageApp() {
    AddFakeWebApp(kAppIdImage, kMimeTypeImage, kFileExtensionImage,
                  kActivityLabelImage);
  }

  void AddAnyApp() {
    AddFakeWebApp(kAppIdAny, kMimeTypeAny, kFileExtensionAny,
                  kActivityLabelAny);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  apps::AppServiceProxyChromeOs* app_service_proxy_ = nullptr;
  apps::AppServiceTest app_service_test_;
};

GURL test_url(const std::string& file_name) {
  GURL url = GURL("filesystem:https://site.com/isolated/" + file_name);
  EXPECT_TRUE(url.is_valid());
  return url;
}

// Test that between an image app and text app, the text app can be
// found for an text file entry.
TEST_F(AppServiceFileTasksTest, FindAppServiceFileTasksText) {
  AddTextApp();
  AddImageApp();
  // Find apps for a "text/plain" file.
  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(
      util::GetMyFilesFolderForProfile(profile()).AppendASCII("foo.txt"),
      kMimeTypeText, false);

  std::vector<GURL> file_urls;
  file_urls.push_back(test_url("foo.txt"));
  std::vector<FullTaskDescriptor> tasks;
  FindAppServiceTasks(profile(), entries, file_urls, &tasks);
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kAppIdText, tasks[0].task_descriptor.app_id);
  EXPECT_EQ(kActivityLabelText, tasks[0].task_title);
}

// Test that between an image app and text app, the image app can be
// found for an image file entry.
TEST_F(AppServiceFileTasksTest, FindAppServiceFileTasksImage) {
  AddTextApp();
  AddImageApp();
  // Find apps for a "image/jpeg" file.
  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(
      util::GetMyFilesFolderForProfile(profile()).AppendASCII("bar.jpeg"),
      kMimeTypeImage, false);

  std::vector<GURL> file_urls;
  file_urls.push_back(test_url("bar.jpeg"));
  std::vector<FullTaskDescriptor> tasks;
  FindAppServiceTasks(profile(), entries, file_urls, &tasks);
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kAppIdImage, tasks[0].task_descriptor.app_id);
  EXPECT_EQ(kActivityLabelImage, tasks[0].task_title);
}

// Test that between an image app, text app and an app that can handle every
// file, the app that can handle every file can be found for an image file entry
// and text file entry.
TEST_F(AppServiceFileTasksTest, FindAppServiceFileTasksMultiple) {
  AddTextApp();
  AddImageApp();
  AddAnyApp();
  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(
      util::GetMyFilesFolderForProfile(profile()).AppendASCII("foo.txt"),
      kMimeTypeText, false);
  entries.emplace_back(
      util::GetMyFilesFolderForProfile(profile()).AppendASCII("bar.jpeg"),
      kMimeTypeImage, false);

  std::vector<GURL> file_urls;
  file_urls.push_back(test_url("foo.txt"));
  file_urls.push_back(test_url("bar.jpeg"));
  std::vector<FullTaskDescriptor> tasks;
  FindAppServiceTasks(profile(), entries, file_urls, &tasks);
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kAppIdAny, tasks[0].task_descriptor.app_id);
  EXPECT_EQ(kActivityLabelAny, tasks[0].task_title);
}

// Don't register any apps and check that we get no matches.
TEST_F(AppServiceFileTasksTest, FindAppServiceWebFileTasksNoTasks) {
  // Find web apps for a "text/plain" file.
  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(
      util::GetMyFilesFolderForProfile(profile()).AppendASCII("foo.txt"),
      kMimeTypeText, false);

  std::vector<GURL> file_urls;
  file_urls.push_back(test_url("foo.txt"));
  std::vector<FullTaskDescriptor> tasks;
  FindAppServiceTasks(profile(), entries, file_urls, &tasks);
  ASSERT_EQ(0U, tasks.size());
}

// Register a text handler and check we get no matches with an image.
TEST_F(AppServiceFileTasksTest, FindAppServiceWebFileTasksNoMatchingTask) {
  AddTextApp();
  // Find apps for a "image/jpeg" file.
  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(
      util::GetMyFilesFolderForProfile(profile()).AppendASCII("bar.jpeg"),
      kMimeTypeImage, false);

  std::vector<GURL> file_urls;
  file_urls.push_back(test_url("bar.jpeg"));
  std::vector<FullTaskDescriptor> tasks;
  FindAppServiceTasks(profile(), entries, file_urls, &tasks);
  ASSERT_EQ(0U, tasks.size());
}

// Check we get a match for a text file + web app.
TEST_F(AppServiceFileTasksTest, FindAppServiceWebFileTasksText) {
  AddTextApp();
  // Find web apps for a "text/plain" file.
  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(
      util::GetMyFilesFolderForProfile(profile()).AppendASCII("foo.txt"),
      kMimeTypeText, false);

  std::vector<GURL> file_urls;
  file_urls.push_back(test_url("foo.txt"));
  std::vector<FullTaskDescriptor> tasks;
  FindAppServiceTasks(profile(), entries, file_urls, &tasks);
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kAppIdText, tasks[0].task_descriptor.app_id);
  EXPECT_EQ(kActivityLabelText, tasks[0].task_title);
}

// Check that a web app that only handles text does not match when we have both
// a text file and an image.
TEST_F(AppServiceFileTasksTest, FindAppServiceWebFileTasksTwoFilesNoMatch) {
  AddTextApp();
  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(
      util::GetMyFilesFolderForProfile(profile()).AppendASCII("foo.txt"),
      kMimeTypeText, false);
  entries.emplace_back(
      util::GetMyFilesFolderForProfile(profile()).AppendASCII("bar.jpeg"),
      kMimeTypeImage, false);

  std::vector<GURL> file_urls;
  file_urls.push_back(test_url("foo.txt"));
  file_urls.push_back(test_url("bar.jpeg"));
  std::vector<FullTaskDescriptor> tasks;
  FindAppServiceTasks(profile(), entries, file_urls, &tasks);
  ASSERT_EQ(0U, tasks.size());
}

}  // namespace file_tasks
}  // namespace file_manager.
