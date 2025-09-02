// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/drive_uploader.h"

#include <memory>
#include <string>
#include <utility>

#include "base/test/mock_callback.h"
#include "chrome/browser/save_to_drive/content_reader.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/extensions/api/pdf_viewer_private.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace save_to_drive {

namespace {

using extensions::api::pdf_viewer_private::SaveToDriveErrorType;
using extensions::api::pdf_viewer_private::SaveToDriveProgress;
using extensions::api::pdf_viewer_private::SaveToDriveStatus;
using testing::_;
using testing::AllOf;
using testing::Field;

class MockContentReader : public ContentReader {
 public:
  MOCK_METHOD(void, Open, (OpenCallback callback), (override));
  MOCK_METHOD(size_t, GetSize, (), (override));
  MOCK_METHOD(void,
              Read,
              (uint32_t offset, uint32_t size, ContentReadCallback callback),
              (override));
  MOCK_METHOD(void, Close, (), (override));
};

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

  MOCK_METHOD(void, UploadFile, (), (override));
};

class DriveUploaderTest : public testing::Test {
 public:
  DriveUploaderTest()
      : profile_(IdentityTestEnvironmentProfileAdaptor::
                     CreateProfileForIdentityTestEnvironment()),
        adaptor_(std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            profile_.get())) {}
  DriveUploaderTest(const DriveUploaderTest&) = delete;
  DriveUploaderTest& operator=(const DriveUploaderTest&) = delete;
  ~DriveUploaderTest() override = default;

 protected:
  signin::IdentityTestEnvironment* test_env() {
    return adaptor_->identity_test_env();
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor> adaptor_;
  base::MockCallback<DriveUploader::ProgressCallback> progress_callback_;
  MockContentReader mock_content_reader_;
};

TEST_F(DriveUploaderTest, FetchAccessTokenSuccess) {
  auto account_info = test_env()->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  auto uploader = std::make_unique<FakeDriveUploader>(
      "test_title", account_info, progress_callback_.Get(), profile_.get());

  EXPECT_CALL(progress_callback_,
              Run(AllOf(Field(&SaveToDriveProgress::status,
                              SaveToDriveStatus::kFetchOauth),
                        Field(&SaveToDriveProgress::error_type,
                              SaveToDriveErrorType::kNoError))));

  uploader->Start();
  test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "test_token", base::Time::Max());
}

TEST_F(DriveUploaderTest, FetchAccessTokenFailure) {
  auto account_info = test_env()->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  auto uploader = std::make_unique<FakeDriveUploader>(
      "test_title", account_info, progress_callback_.Get(), profile_.get());

  EXPECT_CALL(progress_callback_,
              Run(AllOf(Field(&SaveToDriveProgress::status,
                              SaveToDriveStatus::kUploadFailed),
                        Field(&SaveToDriveProgress::error_type,
                              SaveToDriveErrorType::kOauthError))));

  uploader->Start();
  test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
}

TEST_F(DriveUploaderTest, NoRefreshToken) {
  AccountInfo account_info;
  account_info.email = "test@example.com";
  account_info.account_id = CoreAccountId::FromGaiaId(GaiaId("12345"));

  auto uploader = std::make_unique<FakeDriveUploader>(
      "test_title", account_info, progress_callback_.Get(), profile_.get());

  EXPECT_CALL(progress_callback_,
              Run(AllOf(Field(&SaveToDriveProgress::status,
                              SaveToDriveStatus::kUploadFailed),
                        Field(&SaveToDriveProgress::error_type,
                              SaveToDriveErrorType::kOauthError))));

  uploader->Start();
}

}  // namespace

}  // namespace save_to_drive
