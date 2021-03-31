// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/app_service_file_tasks.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/chromeos/file_manager/file_tasks.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/common/chrome_features.h"
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
const char kActivityLabelText[] = "some_text_activity";
const char kActivityLabelImage[] = "some_image_activity";
const char kActivityLabelAny[] = "some_any_file";
}  // namespace

namespace file_manager {
namespace file_tasks {

class AppServiceFileTasksTest : public testing::Test {
 protected:
  AppServiceFileTasksTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kIntentHandlingSharing);
  }
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    app_service_test_.SetUp(profile_.get());
    app_service_proxy_ =
        apps::AppServiceProxyFactory::GetForProfile(profile_.get());
    ASSERT_TRUE(app_service_proxy_);
    AddApps();
  }

  Profile* profile() { return profile_.get(); }

  void AddFakeAppWithIntentFilter(const std::string& app_id,
                                  const std::string& mime_type,
                                  const std::string& activity_label,
                                  bool is_send_multiple) {
    std::vector<apps::mojom::AppPtr> apps;
    auto app = apps::mojom::App::New();
    app->app_id = app_id;
    app->app_type = apps::mojom::AppType::kArc;
    auto intent_filter =
        is_send_multiple
            ? apps_util::CreateIntentFilterForSendMultiple(mime_type,
                                                           activity_label)
            : apps_util::CreateIntentFilterForSend(mime_type, activity_label);
    app->intent_filters.push_back(std::move(intent_filter));
    apps.push_back(std::move(app));
    app_service_proxy_->AppRegistryCache().OnApps(
        std::move(apps), apps::mojom::AppType::kArc,
        false /* should_notify_initialized */);
    app_service_test_.WaitForAppService();
  }

  void AddApps() {
    AddFakeAppWithIntentFilter(kAppIdText, kMimeTypeText, kActivityLabelText,
                               /*is_send_multiple=*/false);
    AddFakeAppWithIntentFilter(kAppIdImage, kMimeTypeImage, kActivityLabelImage,
                               /*is_send_multiple=*/false);
    AddFakeAppWithIntentFilter(kAppIdAny, kMimeTypeAny, kActivityLabelAny,
                               /*is_send_multiple=*/true);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  apps::AppServiceProxyChromeOs* app_service_proxy_ = nullptr;
  apps::AppServiceTest app_service_test_;
};

// Test that between an image app and text app, the text app can be
// found for an text file entry.
TEST_F(AppServiceFileTasksTest, FindAppServiceFileTasksText) {
  // Find apps for a "text/plain" file.
  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(
      util::GetMyFilesFolderForProfile(profile()).AppendASCII("foo.txt"),
      kMimeTypeText, false);

  // This test doesn't test file_urls, leave it empty.
  std::vector<GURL> file_urls{GURL()};
  std::vector<FullTaskDescriptor> tasks;
  FindAppServiceTasks(profile(), entries, file_urls, &tasks);
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kAppIdText, tasks[0].task_descriptor().app_id);
  EXPECT_EQ(kActivityLabelText, tasks[0].task_title());
}

// Test that between an image app and text app, the image app can be
// found for an image file entry.
TEST_F(AppServiceFileTasksTest, FindAppServiceFileTasksImage) {
  // Find apps for a "image/jpeg" file.
  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(
      util::GetMyFilesFolderForProfile(profile()).AppendASCII("bar.jpeg"),
      kMimeTypeImage, false);

  // This test doesn't test file_urls, leave it empty.
  std::vector<GURL> file_urls{GURL()};
  std::vector<FullTaskDescriptor> tasks;
  FindAppServiceTasks(profile(), entries, file_urls, &tasks);
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kAppIdImage, tasks[0].task_descriptor().app_id);
  EXPECT_EQ(kActivityLabelImage, tasks[0].task_title());
}

// Test that between an image app, text app and an app that can handle every
// file, the app that can handle every file can be found for an image file entry
// and text file entry.
TEST_F(AppServiceFileTasksTest, FindAppServiceFileTasksMultiple) {
  std::vector<extensions::EntryInfo> entries;
  entries.emplace_back(
      util::GetMyFilesFolderForProfile(profile()).AppendASCII("foo.txt"),
      kMimeTypeText, false);
  entries.emplace_back(
      util::GetMyFilesFolderForProfile(profile()).AppendASCII("bar.jpeg"),
      kMimeTypeImage, false);

  // This test doesn't test file_urls, leave it empty.
  std::vector<GURL> file_urls{GURL(), GURL()};
  std::vector<FullTaskDescriptor> tasks;
  FindAppServiceTasks(profile(), entries, file_urls, &tasks);
  ASSERT_EQ(1U, tasks.size());
  EXPECT_EQ(kAppIdAny, tasks[0].task_descriptor().app_id);
  EXPECT_EQ(kActivityLabelAny, tasks[0].task_title());
}

}  // namespace file_tasks
}  // namespace file_manager.
