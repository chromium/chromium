// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/virtual_tasks/install_isolated_web_app_virtual_task.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/ash/file_manager/virtual_file_tasks.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_app_ui_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/file_manager/app_id.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/entry_info.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"

using testing::_;

namespace file_manager::file_tasks {

class MockWebAppUiManager : public web_app::FakeWebAppUiManager {
 public:
  MOCK_METHOD(void,
              LaunchOrFocusIsolatedWebAppInstaller,
              (const base::FilePath&),
              (override));
};

class InstallIsolatedWebAppVirtualTaskTest : public testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {features::kIsolatedWebApps, features::kIsolatedWebAppUnmanagedInstall},
        {});

    auto ui_manager = std::make_unique<MockWebAppUiManager>();
    ui_manager_ = ui_manager.get();
    web_app::FakeWebAppProvider::Get(&profile_)->SetWebAppUiManager(
        std::move(ui_manager));
    web_app::test::AwaitStartWebAppProviderAndSubsystems(&profile_);

    GetTestVirtualTasks().push_back(&task_);
  }

  void TearDown() override {
    ui_manager_ = nullptr;
    GetTestVirtualTasks().clear();
  }

 protected:
  MockWebAppUiManager& ui_manager() { return *ui_manager_; }

  bool Matches(const std::vector<GURL>& file_urls) {
    std::vector<extensions::EntryInfo> entries;
    base::ranges::transform(
        file_urls, std::back_inserter(entries), [](const GURL& file_url) {
          return extensions::EntryInfo(base::FilePath(file_url.path()),
                                       "application/octet-stream",
                                       /*is_directory=*/false);
        });

    std::vector<FullTaskDescriptor> tasks;
    MatchVirtualTasks(&profile_, entries, file_urls, /*dlp_source_urls=*/{},
                      &tasks);

    return tasks.size() == 1 &&
           tasks[0].task_descriptor.action_id == task_.id();
  }

  bool ExecuteTask(const std::vector<GURL>& file_urls) {
    std::vector<FileSystemURL> file_system_urls;
    auto storage_key =
        blink::StorageKey::CreateFromStringForTesting("https://example.com/");
    base::ranges::transform(file_urls, std::back_inserter(file_system_urls),
                            [&](const GURL& url) {
                              return storage::FileSystemURL::CreateForTest(
                                  storage_key, storage::kFileSystemTypeLocal,
                                  base::FilePath::FromUTF8Unsafe(url.path()));
                            });

    return ExecuteVirtualTask(
        &profile_, {kFileManagerSwaAppId, TASK_TYPE_WEB_APP, task_.id()},
        file_system_urls);
  }

 private:
  content::BrowserTaskEnvironment browser_task_environment_;
  TestingProfile profile_;
  InstallIsolatedWebAppVirtualTask task_;
  raw_ptr<MockWebAppUiManager> ui_manager_ = nullptr;

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(InstallIsolatedWebAppVirtualTaskTest, MatchesSwbnFiles) {
  EXPECT_TRUE(Matches({GURL("file:///bundle.swbn"), GURL("file:///bundle.SWBN"),
                       GURL("file:///bundle.SwBn")}));
}

TEST_F(InstallIsolatedWebAppVirtualTaskTest, DoesNotMatchMultipleExtensions) {
  EXPECT_FALSE(
      Matches({GURL("file:///bundle.swbn"), GURL("file:///bundle.wbn")}));
}

TEST_F(InstallIsolatedWebAppVirtualTaskTest, DoesNotMatchIfIwasDisabled) {
  base::test::ScopedFeatureList disable_feature;
  disable_feature.InitAndDisableFeature(features::kIsolatedWebApps);

  EXPECT_FALSE(Matches({GURL("file:///bundle.swbn")}));
}

TEST_F(InstallIsolatedWebAppVirtualTaskTest,
       DoesNotMatchIfUnmanagedInstallDisabled) {
  base::test::ScopedFeatureList disable_feature;
  disable_feature.InitAndDisableFeature(
      features::kIsolatedWebAppUnmanagedInstall);

  EXPECT_FALSE(Matches({GURL("file:///bundle.swbn")}));
}

TEST_F(InstallIsolatedWebAppVirtualTaskTest, TaskNotRunWhenFeatureDisabled) {
  base::test::ScopedFeatureList disable_feature;
  disable_feature.InitAndDisableFeature(features::kIsolatedWebApps);

  EXPECT_FALSE(ExecuteTask({GURL("file:///bundle.swbn")}));
}

TEST_F(InstallIsolatedWebAppVirtualTaskTest, TaskNotRunIfNoFiles) {
  EXPECT_FALSE(ExecuteTask({}));
}

TEST_F(InstallIsolatedWebAppVirtualTaskTest, SingleFile) {
  EXPECT_CALL(ui_manager(), LaunchOrFocusIsolatedWebAppInstaller(
                                base::FilePath("/bundle.swbn")));

  EXPECT_TRUE(ExecuteTask({GURL("file:///bundle.swbn")}));
}

TEST_F(InstallIsolatedWebAppVirtualTaskTest, MultipleFiles) {
  EXPECT_CALL(ui_manager(), LaunchOrFocusIsolatedWebAppInstaller(
                                base::FilePath("/bundle1.swbn")));
  EXPECT_CALL(ui_manager(), LaunchOrFocusIsolatedWebAppInstaller(
                                base::FilePath("/bundle3.SWBN")));
  EXPECT_CALL(ui_manager(), LaunchOrFocusIsolatedWebAppInstaller(
                                base::FilePath("/bundle4.SwBn")));

  EXPECT_TRUE(ExecuteTask({
      GURL("file:///bundle1.swbn"),
      GURL("file:///bundle3.SWBN"),
      GURL("file:///bundle4.SwBn"),
  }));
}

}  // namespace file_manager::file_tasks
