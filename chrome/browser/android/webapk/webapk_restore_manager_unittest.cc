// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_restore_manager.h"

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/types/pass_key.h"
#include "chrome/browser/android/webapk/webapk_helpers.h"
#include "chrome/browser/android/webapk/webapk_install_service_factory.h"
#include "chrome/browser/android/webapk/webapk_restore_task.h"
#include "chrome/browser/android/webapk/webapk_restore_web_contents_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/webapps/browser/android/webapk/webapk_types.h"
#include "components/webapps/browser/web_contents/web_app_url_loader.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webapk {

std::vector<WebApkRestoreData> CreateRestoreAppsList(
    const std::vector<GURL>& manifest_ids) {
  std::vector<WebApkRestoreData> apps;
  for (const auto& manifest_id : manifest_ids) {
    auto shortcut_info = std::make_unique<webapps::ShortcutInfo>(manifest_id);
    apps.emplace_back(
        WebApkRestoreData(GenerateAppIdFromManifestId(manifest_id),
                          std::move(shortcut_info), base::Time()));
  }
  return apps;
}

// A mock WebApkRestoreTask that does nothing but run the complete callback when
// starts.
class MockWebApkRestoreTask : public WebApkRestoreTask {
 public:
  explicit MockWebApkRestoreTask(
      base::PassKey<WebApkRestoreManager> pass_key,
      WebApkInstallService* web_apk_install_service,
      WebApkRestoreWebContentsManager* web_contents_manager,
      std::unique_ptr<webapps::ShortcutInfo> shortcut_info,
      base::Time last_used_time,
      std::vector<std::pair<GURL, std::string>>* task_log)
      : WebApkRestoreTask(pass_key,
                          web_apk_install_service,
                          web_contents_manager,
                          std::move(shortcut_info),
                          last_used_time),
        task_log_(task_log) {}
  ~MockWebApkRestoreTask() override = default;

  void DownloadIcon(base::OnceClosure done_closure) {
    std::move(done_closure).Run();
  }

  void Start(CompleteCallback complete_callback) override {
    task_log_->emplace_back(manifest_id(), "Start");

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(complete_callback), manifest_id(),
                                  webapps::WebApkInstallResult::SUCCESS));
  }

 private:
  raw_ptr<std::vector<std::pair<GURL, std::string>>> task_log_;
};

class TestWebContentsManager : public WebApkRestoreWebContentsManager {
 public:
  explicit TestWebContentsManager(Profile* profile)
      : WebApkRestoreWebContentsManager(profile) {}
  ~TestWebContentsManager() override = default;

  void LoadUrl(const GURL& url,
               webapps::WebAppUrlLoader::ResultCallback callback) override {
    std::move(callback).Run(webapps::WebAppUrlLoaderResult::kUrlLoaded);
  }
};

class TestWebApkRestoreManager : public WebApkRestoreManager {
 public:
  explicit TestWebApkRestoreManager(Profile* profile)
      : WebApkRestoreManager(
            WebApkInstallServiceFactory::GetForBrowserContext(profile),
            std::make_unique<TestWebContentsManager>(profile)),
        web_apk_install_service_(
            WebApkInstallServiceFactory::GetForBrowserContext(profile)) {}
  ~TestWebApkRestoreManager() override = default;

  void PrepareTasksFinish(base::OnceClosure on_task_finish) {
    on_task_finish_ = std::move(on_task_finish);
  }

  void OnTaskFinished(const GURL& manifest_id,
                      webapps::WebApkInstallResult result) override {
    task_log_.emplace_back(manifest_id, "Finish");
    WebApkRestoreManager::OnTaskFinished(manifest_id, result);

    if (GetTasksCountForTesting() == 0u) {
      std::move(on_task_finish_).Run();
    }
  }

  std::vector<std::pair<GURL, std::string>> task_log() { return task_log_; }

 private:
  // Mock CreateNewTask to create the Mock task instead.
  std::unique_ptr<WebApkRestoreTask> CreateNewTask(
      std::unique_ptr<webapps::ShortcutInfo> shortcut_info,
      base::Time last_used_time) override {
    task_log_.emplace_back(shortcut_info->manifest_id, "Create");
    return std::make_unique<MockWebApkRestoreTask>(
        WebApkRestoreManager::PassKeyForTesting(), web_apk_install_service_,
        web_contents_manager(), std::move(shortcut_info), last_used_time,
        &task_log_);
  }

  const raw_ptr<WebApkInstallService> web_apk_install_service_;
  base::OnceClosure on_task_finish_;
  std::vector<std::pair<GURL, std::string>> task_log_;
};

class WebApkRestoreManagerTest : public ::testing::Test {
 public:
  WebApkRestoreManagerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    profile_ = testing_profile_manager_.CreateTestingProfile(
        TestingProfile::kDefaultProfileUserName, true);
  }

  TestingProfile* profile() { return profile_.get(); }

 protected:
  const GURL kManifestId1 = GURL("https://example.com/app1");
  const GURL kManifestId2 = GURL("https://example.com/app2");
  const GURL kManifestId3 = GURL("https://example.com/app3");
  const webapps::AppId kAppId1 = GenerateAppIdFromManifestId(kManifestId1);
  const webapps::AppId kAppId2 = GenerateAppIdFromManifestId(kManifestId2);
  const webapps::AppId kAppId3 = GenerateAppIdFromManifestId(kManifestId3);

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_;
};

TEST_F(WebApkRestoreManagerTest, GetAppResults) {
  std::unique_ptr<TestWebApkRestoreManager> manager =
      std::make_unique<TestWebApkRestoreManager>(profile());

  std::vector<WebApkRestoreData> apps;
  auto shortcut_info_1 = std::make_unique<webapps::ShortcutInfo>(kManifestId1);
  shortcut_info_1->name = u"app1";
  auto shortcut_info_2 = std::make_unique<webapps::ShortcutInfo>(kManifestId2);
  shortcut_info_2->name = u"app2";
  apps.emplace_back(
      WebApkRestoreData(kAppId1, std::move(shortcut_info_1), base::Time()));
  apps.emplace_back(
      WebApkRestoreData(kAppId2, std::move(shortcut_info_2), base::Time()));

  base::RunLoop run_loop;
  manager->PrepareRestorableApps(
      std::move(apps),
      base::BindLambdaForTesting([&](const std::vector<std::string>& ids,
                                     const std::vector<std::u16string>& names,
                                     const std::vector<int>& last_used_in_days,
                                     const std::vector<SkBitmap>& icons) {
        EXPECT_THAT(ids, testing::ElementsAre(
                             GenerateAppIdFromManifestId(kManifestId1),
                             GenerateAppIdFromManifestId(kManifestId2)));
        EXPECT_THAT(names, testing::ElementsAre(u"app1", u"app2"));
        run_loop.Quit();
      }));
  run_loop.Run();

}

TEST_F(WebApkRestoreManagerTest, RunOneTasks) {
  std::unique_ptr<TestWebApkRestoreManager> manager =
      std::make_unique<TestWebApkRestoreManager>(profile());

  auto apps = CreateRestoreAppsList({kManifestId1});
  manager->PrepareRestorableApps(std::move(apps), base::DoNothing());

  base::RunLoop run_loop;
  manager->PrepareTasksFinish(run_loop.QuitClosure());
  manager->ScheduleRestoreTasks({kAppId1});
  run_loop.Run();

  EXPECT_THAT(manager->task_log(),
              testing::ElementsAre(std::pair(kManifestId1, "Create"),
                                   std::pair(kManifestId1, "Start"),
                                   std::pair(kManifestId1, "Finish")));
}

// Schedule multiple tasks and they run in sequence.
TEST_F(WebApkRestoreManagerTest, ScheduleMultipleTasks) {
  std::unique_ptr<TestWebApkRestoreManager> manager =
      std::make_unique<TestWebApkRestoreManager>(profile());

  auto apps = CreateRestoreAppsList({kManifestId1, kManifestId2, kManifestId3});
  manager->PrepareRestorableApps(std::move(apps), base::DoNothing());

  base::RunLoop run_loop;
  manager->PrepareTasksFinish(run_loop.QuitClosure());
  manager->ScheduleRestoreTasks({kAppId1, kAppId2});
  run_loop.Run();

  // Tests are created when added to the queue. The second task only start until
  // the first one finished.
  EXPECT_THAT(
      manager->task_log(),
      testing::ElementsAre(
          std::pair(kManifestId1, "Create"), std::pair(kManifestId2, "Create"),
          std::pair(kManifestId3, "Create"), std::pair(kManifestId1, "Start"),
          std::pair(kManifestId1, "Finish"), std::pair(kManifestId2, "Start"),
          std::pair(kManifestId2, "Finish")));

  base::RunLoop run_loop2;
  manager->PrepareTasksFinish(run_loop2.QuitClosure());
  manager->ScheduleRestoreTasks({kAppId3});
  run_loop2.Run();

  EXPECT_THAT(
      manager->task_log(),
      testing::ElementsAre(
          std::pair(kManifestId1, "Create"), std::pair(kManifestId2, "Create"),
          std::pair(kManifestId3, "Create"), std::pair(kManifestId1, "Start"),
          std::pair(kManifestId1, "Finish"), std::pair(kManifestId2, "Start"),
          std::pair(kManifestId2, "Finish"), std::pair(kManifestId3, "Start"),
          std::pair(kManifestId3, "Finish")));
}

}  // namespace webapk
