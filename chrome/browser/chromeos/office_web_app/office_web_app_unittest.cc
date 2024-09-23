// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/office_web_app/office_web_app.h"

#include "base/test/test_future.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/test/base/testing_profile.h"
#include "components/webapps/browser/install_result_code.h"

namespace chromeos {
namespace {

using base::test::TestFuture;

// Test class to check that the Office (Microsoft365) web app can be installed
// online and offline.
class OfficeWebAppUnitTest : public WebAppTest {
 protected:
  OfficeWebAppUnitTest() = default;
  ~OfficeWebAppUnitTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(OfficeWebAppUnitTest, InstallMicrosoft365WhenOffline) {
  TestFuture<webapps::InstallResultCode> future;
  InstallMicrosoft365(profile(), future.GetCallback());
  EXPECT_EQ(future.Get(),
            webapps::InstallResultCode::kSuccessOfflineOnlyInstall);
}

TEST_F(OfficeWebAppUnitTest, InstallMicrosoft365WhenOnline) {
  // Set the behaviour of `LoadUrl` to return `kUrlLoaded` for the Microsoft365
  // install URL (set the system to be online).
  auto& web_contents_manager = static_cast<web_app::FakeWebContentsManager&>(
      web_app::WebAppProvider::GetForTest(profile())->web_contents_manager());
  auto& fake_page_state =
      web_contents_manager.GetOrCreatePageState(GURL(kMicrosoft365WebAppUrl));
  fake_page_state.url_load_result = webapps::WebAppUrlLoaderResult::kUrlLoaded;

  TestFuture<webapps::InstallResultCode> future;
  InstallMicrosoft365(profile(), future.GetCallback());
  EXPECT_EQ(future.Get(), webapps::InstallResultCode::kSuccessNewInstall);
}

}  // namespace
}  // namespace chromeos
