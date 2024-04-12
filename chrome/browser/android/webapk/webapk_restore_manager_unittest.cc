// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_restore_manager.h"

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/types/pass_key.h"
#include "chrome/browser/android/webapk/webapk_restore_task.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "components/webapps/browser/android/webapk/webapk_types.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webapk {

namespace {

std::unique_ptr<sync_pb::WebApkSpecifics> CreateWebApkSpecifics(
    const GURL& url) {
  std::unique_ptr<sync_pb::WebApkSpecifics> web_apk =
      std::make_unique<sync_pb::WebApkSpecifics>();
  web_apk->set_manifest_id(url.spec());
  web_apk->set_start_url(url.spec());

  return web_apk;
}

// A mock WebApkRestoreTask that does nothing but run the complete callback when
// starts.
class MockWebApkRestoreTask : public WebApkRestoreTask {
 public:
  explicit MockWebApkRestoreTask(
      base::PassKey<WebApkRestoreManager> pass_key,
      Profile* profile,
      const sync_pb::WebApkSpecifics& webapk_specifics,
      std::vector<std::pair<GURL, std::string>>* task_log)
      : WebApkRestoreTask(pass_key, profile, webapk_specifics),
        manifest_id_(GURL(webapk_specifics.manifest_id())),
        task_log_(task_log) {}
  ~MockWebApkRestoreTask() override = default;

  void Start(WebApkRestoreWebContentsManager* web_contents_manager,
             CompleteCallback complete_callback) override {
    task_log_->emplace_back(manifest_id_, "Start");

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(complete_callback), manifest_id_,
                                  webapps::WebApkInstallResult::SUCCESS));
  }

 private:
  const GURL manifest_id_;
  raw_ptr<std::vector<std::pair<GURL, std::string>>> task_log_;
};

class FakeWebApkRestoreManager : public WebApkRestoreManager {
 public:
  explicit FakeWebApkRestoreManager(Profile* profile)
      : WebApkRestoreManager(profile) {}
  ~FakeWebApkRestoreManager() override = default;

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
      const sync_pb::WebApkSpecifics& webapk_specifics) override {
    task_log_.emplace_back(GURL(webapk_specifics.manifest_id()), "Create");
    return std::make_unique<MockWebApkRestoreTask>(
        WebApkRestoreManager::PassKeyForTesting(), profile(), webapk_specifics,
        &task_log_);
  }

  base::OnceClosure on_task_finish_;
  std::vector<std::pair<GURL, std::string>> task_log_;
};

}  // namespace

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

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_;
};

TEST_F(WebApkRestoreManagerTest, OneTasks) {
  const GURL manifest_id_1("https://example.com/app1");

  base::RunLoop run_loop;

  std::unique_ptr<FakeWebApkRestoreManager> manager =
      std::make_unique<FakeWebApkRestoreManager>(profile());

  manager->PrepareTasksFinish(run_loop.QuitClosure());
  manager->ScheduleTask(*CreateWebApkSpecifics(manifest_id_1).get());

  run_loop.Run();
  EXPECT_THAT(manager->task_log(),
              testing::ElementsAre(std::pair(manifest_id_1, "Create"),
                                   std::pair(manifest_id_1, "Start"),
                                   std::pair(manifest_id_1, "Finish")));
}

// Schedule multiple tasks and they run in sequence.
TEST_F(WebApkRestoreManagerTest, ScheduleMultipleTasks) {
  const GURL manifest_id_1("https://example.com/app1");
  const GURL manifest_id_2("https://example.com/app2");
  const GURL manifest_id_3("https://example.com/app3");

  std::unique_ptr<FakeWebApkRestoreManager> manager =
      std::make_unique<FakeWebApkRestoreManager>(profile());
  base::RunLoop run_loop;
  manager->PrepareTasksFinish(run_loop.QuitClosure());
  manager->ScheduleTask(*CreateWebApkSpecifics(manifest_id_1).get());
  manager->ScheduleTask(*CreateWebApkSpecifics(manifest_id_2).get());
  run_loop.Run();

  // Tests are created when added to the queue. The second task only start until
  // the first one finished.
  EXPECT_THAT(manager->task_log(),
              testing::ElementsAre(std::pair(manifest_id_1, "Create"),
                                   std::pair(manifest_id_1, "Start"),
                                   std::pair(manifest_id_2, "Create"),
                                   std::pair(manifest_id_1, "Finish"),
                                   std::pair(manifest_id_2, "Start"),
                                   std::pair(manifest_id_2, "Finish")));

  base::RunLoop run_loop2;
  manager->PrepareTasksFinish(run_loop2.QuitClosure());
  manager->ScheduleTask(*CreateWebApkSpecifics(manifest_id_3).get());
  run_loop2.Run();
  EXPECT_THAT(
      manager->task_log(),
      testing::ElementsAre(
          std::pair(manifest_id_1, "Create"), std::pair(manifest_id_1, "Start"),
          std::pair(manifest_id_2, "Create"),
          std::pair(manifest_id_1, "Finish"), std::pair(manifest_id_2, "Start"),
          std::pair(manifest_id_2, "Finish"),
          std::pair(manifest_id_3, "Create"), std::pair(manifest_id_3, "Start"),
          std::pair(manifest_id_3, "Finish")));
}

}  // namespace webapk
