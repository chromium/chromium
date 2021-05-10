// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/webapk/webapk_manager.h"

#include <memory>

#include "base/strings/strcat.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/apps/app_service/webapk/webapk_install_queue.h"
#include "chrome/browser/apps/app_service/webapk/webapk_install_task.h"
#include "chrome/browser/apps/app_service/webapk/webapk_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/web_applications/test/test_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestAppUrl[] = "https://www.example.com/";
constexpr char kTestAppActionUrl[] = "https://www.example.com/share";
constexpr char kTestManifestUrl[] = "https://www.example.com/manifest.json";
constexpr char kTestShareTextParam[] = "share_text";
const std::u16string kTestAppTitle = u"Test App";

std::unique_ptr<WebApplicationInfo> BuildDefaultWebAppInfo() {
  auto app_info = std::make_unique<WebApplicationInfo>();
  app_info->start_url = GURL(kTestAppUrl);
  app_info->scope = GURL(kTestAppUrl);
  app_info->title = kTestAppTitle;
  app_info->manifest_url = GURL(kTestManifestUrl);

  apps::ShareTarget target;
  target.action = GURL(kTestAppActionUrl);
  target.method = apps::ShareTarget::Method::kPost;
  target.enctype = apps::ShareTarget::Enctype::kMultipartFormData;
  target.params.text = kTestShareTextParam;
  app_info->share_target = target;

  return app_info;
}

}  // namespace

class WebApkManagerTest : public testing::Test {
 public:
  WebApkManagerTest() = default;

  void SetUp() override {
    testing::Test::SetUp();

    extensions::TestExtensionSystem* extension_system(
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(&profile_)));
    extension_service_ = extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
    extension_service_->Init();

    arc_test_.SetUp(&profile_);

    auto* const provider = web_app::TestWebAppProvider::Get(profile());
    provider->SetRunSubsystemStartupTasks(true);
    provider->Start();
  }

  void StartWebApkManager() {
    app_service_test_.SetUp(&profile_);
    app_service_test_.FlushMojoCalls();
    webapk_manager_ = std::make_unique<apps::WebApkManager>(profile());
  }

  void AssertNoPendingInstalls() {
    ASSERT_FALSE(webapk_manager()->GetInstallQueueForTest()->PopTaskForTest());
  }

  TestingProfile* profile() { return &profile_; }
  apps::AppServiceTest* app_service_test() { return &app_service_test_; }
  apps::WebApkManager* webapk_manager() { return webapk_manager_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  ArcAppTest arc_test_;
  apps::AppServiceTest app_service_test_;
  extensions::ExtensionService* extension_service_ = nullptr;

  std::unique_ptr<apps::WebApkManager> webapk_manager_;
};

TEST_F(WebApkManagerTest, InstallsWebApkOnStartup) {
  auto app_id =
      web_app::test::InstallWebApp(profile(), BuildDefaultWebAppInfo());

  StartWebApkManager();

  auto install_task =
      webapk_manager()->GetInstallQueueForTest()->PopTaskForTest();
  ASSERT_TRUE(install_task);
  ASSERT_EQ(install_task->app_id(), app_id);
  AssertNoPendingInstalls();
}

TEST_F(WebApkManagerTest, InstallWebApkAfterStartup) {
  StartWebApkManager();
  AssertNoPendingInstalls();

  auto app_id =
      web_app::test::InstallWebApp(profile(), BuildDefaultWebAppInfo());
  app_service_test()->FlushMojoCalls();

  auto install_task =
      webapk_manager()->GetInstallQueueForTest()->PopTaskForTest();
  ASSERT_TRUE(install_task);
  ASSERT_EQ(install_task->app_id(), app_id);
  AssertNoPendingInstalls();
}

// Does not install web apps without a Share Target definition.
TEST_F(WebApkManagerTest, NoShareTarget) {
  auto app_info = std::make_unique<WebApplicationInfo>();
  app_info->start_url = GURL(kTestAppUrl);
  app_info->title = kTestAppTitle;
  auto app_id = web_app::test::InstallWebApp(profile(), std::move(app_info));

  StartWebApkManager();

  AssertNoPendingInstalls();
}

// When two eligible apps are available during startup, but one of them already
// has a WebAPK installed, only install a new WebAPK for the other app.
TEST_F(WebApkManagerTest, IgnoresAlreadyInstalledWebApkOnStartup) {
  auto app_info_1 = BuildDefaultWebAppInfo();
  auto app_info_2 = BuildDefaultWebAppInfo();
  // Change the start_url so that the two apps have different IDs.
  app_info_2->start_url = GURL(base::StrCat({kTestAppUrl, "/app_2"}));

  auto app_id_1 =
      web_app::test::InstallWebApp(profile(), std::move(app_info_1));
  auto app_id_2 =
      web_app::test::InstallWebApp(profile(), std::move(app_info_2));
  apps::webapk_prefs::AddWebApk(profile(), app_id_1,
                                "org.chromium.webapk.some_package");

  StartWebApkManager();

  auto install_task =
      webapk_manager()->GetInstallQueueForTest()->PopTaskForTest();
  ASSERT_TRUE(install_task);
  ASSERT_EQ(install_task->app_id(), app_id_2);
  AssertNoPendingInstalls();
}
