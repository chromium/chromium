// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/scan_service_factory.h"

#include <memory>

#include "base/check_deref.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/login/test/chrome_user_session_test_environment_delegate.h"
#include "chrome/browser/ash/scanning/fake_lorgnette_scanner_manager.h"
#include "chrome/browser/ash/scanning/lorgnette_scanner_manager_factory.h"
#include "chrome/browser/ash/scanning/scan_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/session_manager/test/user_session_test_environment.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// Creates a new LorgnetteScannerManager for the given `context`.
std::unique_ptr<KeyedService> BuildLorgnetteScannerManager(
    content::BrowserContext* context) {
  return std::make_unique<FakeLorgnetteScannerManager>();
}

// Creates a new ScanService for the given `context`.
std::unique_ptr<KeyedService> BuildScanService(
    content::BrowserContext* context) {
  return ScanServiceFactory::BuildInstanceFor(context);
}

// The factories every Profile in this test is built with.
TestingProfile::TestingFactories GetTestingFactories() {
  return {
      TestingProfile::TestingFactory{
          LorgnetteScannerManagerFactory::GetInstance(),
          base::BindRepeating(&BuildLorgnetteScannerManager)},
      TestingProfile::TestingFactory{ScanServiceFactory::GetInstance(),
                                     base::BindRepeating(&BuildScanService)},
  };
}

}  // namespace

class ScanServiceFactoryTest : public testing::Test {
 public:
  void SetUp() override {
    auto* browser_process = TestingBrowserProcess::GetGlobal();
    user_session_test_environment_ =
        std::make_unique<test::UserSessionTestEnvironment>(
            browser_process->local_state(),
            std::make_unique<test::ChromeUserSessionTestEnvironmentDelegate>(
                browser_process, base::BindRepeating(&GetTestingFactories)));
  }

  void TearDown() override { user_session_test_environment_.reset(); }

 protected:
  // Logs a user in and returns the Profile created along the way. The
  // ScanService needs the User to resolve the MyFiles path.
  Profile& LogInUser() {
    const AccountId account_id =
        AccountId::FromUserEmailGaiaId("user@example.com", GaiaId("12345"));
    CHECK(user_session_test_environment_->AddRegularUser(account_id));
    user_session_test_environment_->LogIn(account_id);
    return CHECK_DEREF(Profile::FromBrowserContext(
        BrowserContextHelper::Get()->GetBrowserContextByAccountId(account_id)));
  }

  // Builds a Profile that is not backed by a User, the way the sign-in
  // browser context is.
  std::unique_ptr<Profile> CreateSigninProfile() {
    TestingProfile::Builder builder;
    builder.SetPath(base::FilePath(kSigninBrowserContextBaseName));
    builder.AddTestingFactories(GetTestingFactories());
    return builder.Build();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<test::UserSessionTestEnvironment>
      user_session_test_environment_;
};

// Test that the ScanService can be created with the original profile.
TEST_F(ScanServiceFactoryTest, OriginalProfileHasService) {
  EXPECT_NE(nullptr, ScanServiceFactory::GetForBrowserContext(&LogInUser()));
}

// Test that the ScanService can be created with an off-the-record profile.
TEST_F(ScanServiceFactoryTest, OffTheRecordProfileHasService) {
  EXPECT_NE(nullptr, ScanServiceFactory::GetForBrowserContext(
                         LogInUser().GetPrimaryOTRProfile(
                             /*create_if_needed=*/true)));
}

// Test that the ScanService cannot be created with a signin profile. It has
// no User, so it is not logged in either.
TEST_F(ScanServiceFactoryTest, SigninProfileNoService) {
  std::unique_ptr<Profile> signin_profile = CreateSigninProfile();
  EXPECT_EQ(nullptr,
            ScanServiceFactory::GetForBrowserContext(signin_profile.get()));
}

}  // namespace ash
