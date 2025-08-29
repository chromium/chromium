// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/drive_uploader.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/api/pdf_viewer_private.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_request_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::TestFuture;
using extensions::api::pdf_viewer_private::SaveToDriveErrorType;
using extensions::api::pdf_viewer_private::SaveToDriveProgress;
using extensions::api::pdf_viewer_private::SaveToDriveStatus;
using testing::_;
using testing::Field;
using testing::IsEmpty;
using testing::StartsWith;

namespace save_to_drive {

using ProgressCallback = DriveUploader::ProgressCallback;

class FakeDriveUploader : public DriveUploader {
 public:
  FakeDriveUploader(std::string title,
                    AccountInfo account_info,
                    ProgressCallback progress_callback,
                    Profile* profile)
      : DriveUploader(DriveUploaderType::kUnknown,
                      std::move(title),
                      std::move(account_info),
                      std::move(progress_callback),
                      profile) {}
  FakeDriveUploader(const FakeDriveUploader&) = delete;
  FakeDriveUploader& operator=(const FakeDriveUploader&) = delete;
  ~FakeDriveUploader() override = default;

  // DriveUploader:
  void UploadFile() override {}

  const std::vector<std::string>& get_oauth_headers() const {
    return oauth_headers_;
  }
};

class DriveUploaderBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    adaptor_ = std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
        browser()->profile());

    identity_test_env_ = adaptor_->identity_test_env();
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  void TearDownOnMainThread() override {
    identity_test_env_ = nullptr;
    adaptor_.reset();

    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor> adaptor_;
  raw_ptr<signin::IdentityTestEnvironment> identity_test_env_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(DriveUploaderBrowserTest, CreateOAuthHeadersSuccess) {
  auto account_info = identity_test_env_->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  base::MockCallback<ProgressCallback> progress_callback;
  FakeDriveUploader uploader("test_title", std::move(account_info),
                             progress_callback.Get(), browser()->profile());
  TestFuture<void> future;
  EXPECT_CALL(progress_callback, Run(Field(&SaveToDriveProgress::status,
                                           SaveToDriveStatus::kFetchOauth)))
      .WillOnce(base::test::RunOnceClosure(future.GetCallback()));
  uploader.Start();

  identity_test_env_->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now() + base::Hours(1));

  EXPECT_TRUE(future.Wait());
  EXPECT_THAT(uploader.get_oauth_headers(),
              ElementsAre("X-Developer-Key",
                          GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
                          net::HttpRequestHeaders::kAuthorization,
                          StartsWith("Bearer access_token")));
}

IN_PROC_BROWSER_TEST_F(DriveUploaderBrowserTest, CreateOAuthHeadersFailure) {
  auto account_info = identity_test_env_->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  base::MockCallback<ProgressCallback> progress_callback;
  FakeDriveUploader uploader("test_title", std::move(account_info),
                             progress_callback.Get(), browser()->profile());
  TestFuture<void> future;
  EXPECT_CALL(progress_callback,
              Run(AllOf(Field(&SaveToDriveProgress::status,
                              SaveToDriveStatus::kUploadFailed),
                        Field(&SaveToDriveProgress::error_type,
                              SaveToDriveErrorType::kOauthError))))
      .WillOnce(base::test::RunOnceClosure(future.GetCallback()));
  uploader.Start();

  identity_test_env_->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
  EXPECT_TRUE(future.Wait());
  EXPECT_THAT(uploader.get_oauth_headers(), IsEmpty());
}

}  // namespace save_to_drive
