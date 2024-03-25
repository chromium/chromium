// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_restore_manager.h"

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webapk {

namespace {

std::unique_ptr<sync_pb::WebApkSpecifics> CreateWebApkSpecifics(
    const std::string& url) {
  std::unique_ptr<sync_pb::WebApkSpecifics> web_apk =
      std::make_unique<sync_pb::WebApkSpecifics>();
  web_apk->set_manifest_id(url);
  web_apk->set_start_url(url);

  return web_apk;
}

class FakeWebApkRestoreManager : public WebApkRestoreManager {
 public:
  explicit FakeWebApkRestoreManager(Profile* profile)
      : WebApkRestoreManager(profile) {}
  ~FakeWebApkRestoreManager() override = default;

  void PrepareTasksFinish(base::OnceClosure on_task_finish) {
    on_task_finish_ = std::move(on_task_finish);
  }

  void OnTaskFinished(const GURL& manifest_id) override {
    finished_tasks_.push_back(manifest_id);
    if (GetTasksCountForTesting() == 1u) {
      std::move(on_task_finish_).Run();
    }
    WebApkRestoreManager::OnTaskFinished(manifest_id);
  }

  std::vector<GURL> finished_tasks() { return finished_tasks_; }

 private:
  base::OnceClosure on_task_finish_;
  std::vector<GURL> finished_tasks_;
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

TEST_F(WebApkRestoreManagerTest, ScheduleTasks) {
  const std::string manifest_id_1 = "https://example.com/app1";
  const std::string manifest_id_2 = "https://example.com/app2";
  const std::string manifest_id_3 = "https://example.com/app3";

  base::RunLoop run_loop1;
  std::unique_ptr<FakeWebApkRestoreManager> manager =
      std::make_unique<FakeWebApkRestoreManager>(profile());

  manager->PrepareTasksFinish(run_loop1.QuitClosure());
  manager->ScheduleTask(*CreateWebApkSpecifics(manifest_id_1).get());

  run_loop1.Run();
  EXPECT_THAT(manager->finished_tasks(),
              testing::ElementsAre(GURL(manifest_id_1)));

  // Schedule 2 tasks and they run in sequence.
  base::RunLoop run_loop2;
  manager->PrepareTasksFinish(run_loop2.QuitClosure());
  manager->ScheduleTask(*CreateWebApkSpecifics(manifest_id_2).get());
  manager->ScheduleTask(*CreateWebApkSpecifics(manifest_id_3).get());
  run_loop2.Run();

  EXPECT_THAT(manager->finished_tasks(),
              testing::ElementsAre(GURL(manifest_id_1), GURL(manifest_id_2),
                                   GURL(manifest_id_3)));
}

}  // namespace webapk
