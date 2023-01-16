// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/web_app_preload_installer.h"

#include "chrome/browser/apps/app_preload_service/preload_app_definition.h"
#include "chrome/browser/apps/app_preload_service/proto/app_provisioning.pb.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class WebAppPreloadInstallerTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();

    profile_ = std::make_unique<TestingProfile>();

    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  Profile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(WebAppPreloadInstallerTest, GetAppId) {
  WebAppPreloadInstaller installer(profile());

  proto::AppProvisioningListAppsResponse_App app;
  app.set_package_id("web:https://cursive.apps.chrome/");

  ASSERT_EQ(installer.GetAppId(PreloadAppDefinition(app)),
            "apignacaigpffemhdbhmnajajaccbckh");
}

}  // namespace apps
