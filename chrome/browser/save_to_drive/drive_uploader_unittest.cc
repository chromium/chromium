// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/drive_uploader.h"

#include <memory>
#include <string>
#include <utility>

#include "base/json/json_writer.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/save_to_drive/content_reader.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/extensions/api/pdf_viewer_private.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/url_loader_interceptor.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace save_to_drive {

namespace {

using base::test::TestFuture;
using extensions::api::pdf_viewer_private::SaveToDriveErrorType;
using extensions::api::pdf_viewer_private::SaveToDriveProgress;
using extensions::api::pdf_viewer_private::SaveToDriveStatus;
using testing::_;
using testing::AllOf;
using testing::Field;

constexpr std::string_view kErrorResponseHeader =
    "HTTP/1.1 500 Internal Server Error\nContent-Type: application/json\n\n";
constexpr std::string_view kParentFolderUrl =
    "https://www.googleapis.com/drive/v3beta/"
    "files?create_as_client_folder=true";
constexpr std::string_view kSuccessFulResponseHeader =
    "HTTP/1.1 200 OK\nContent-Type: text/html\n\n";
constexpr std::string_view kTestFileId = "test_file_id";
constexpr std::string_view kTestFolderName = "test_folder";

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
                    Profile* profile,
                    ContentReader* content_reader)
      : DriveUploader(DriveUploaderType::kUnknown,
                      std::move(title),
                      std::move(account_info),
                      std::move(progress_callback),
                      profile,
                      content_reader) {}
  FakeDriveUploader(const FakeDriveUploader&) = delete;
  FakeDriveUploader& operator=(const FakeDriveUploader&) = delete;
  ~FakeDriveUploader() override = default;

  MOCK_METHOD(void, UploadFile, (), (override));

  void NotifyUploadInProgress(size_t uploaded_bytes, size_t total_bytes) {
    DriveUploader::NotifyUploadInProgress(uploaded_bytes, total_bytes);
  }

  const std::optional<Item>& parent_folder() const { return parent_folder_; }
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

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor> adaptor_;
  base::MockCallback<DriveUploader::ProgressCallback> progress_callback_;
  MockContentReader mock_content_reader_;
};

TEST_F(DriveUploaderTest, FetchAccessTokenSuccess) {
  auto account_info = test_env()->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  auto uploader = std::make_unique<FakeDriveUploader>(
      "test_title", account_info, progress_callback_.Get(), profile_.get(),
      &mock_content_reader_);

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
      "test_title", account_info, progress_callback_.Get(), profile_.get(),
      &mock_content_reader_);

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
      "test_title", account_info, progress_callback_.Get(), profile_.get(),
      &mock_content_reader_);

  EXPECT_CALL(progress_callback_,
              Run(AllOf(Field(&SaveToDriveProgress::status,
                              SaveToDriveStatus::kUploadFailed),
                        Field(&SaveToDriveProgress::error_type,
                              SaveToDriveErrorType::kOauthError))));

  uploader->Start();
}

TEST_F(DriveUploaderTest, OnRefreshTokenRemovedForAccount) {
  auto account_info = test_env()->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  auto uploader = std::make_unique<FakeDriveUploader>(
      "test_title", account_info, progress_callback_.Get(), profile_.get(),
      &mock_content_reader_);

  // The `progress_callback_` is expected to be run twice: once during the
  // initial `Start()` call as refresh token for the primary account is not
  // configured, and again when the refresh token is removed for the account.
  EXPECT_CALL(progress_callback_,
              Run(AllOf(Field(&SaveToDriveProgress::status,
                              SaveToDriveStatus::kUploadFailed),
                        Field(&SaveToDriveProgress::error_type,
                              SaveToDriveErrorType::kOauthError))))
      .Times(2);

  uploader->Start();
  test_env()->RemoveRefreshTokenForAccount(account_info.account_id);
}

TEST_F(DriveUploaderTest, NotifyUploadInProgressIsRateLimited) {
  auto account_info = test_env()->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  auto uploader = std::make_unique<FakeDriveUploader>(
      "test_title", account_info, progress_callback_.Get(), profile_.get(),
      &mock_content_reader_);
  const size_t kTotalBytes = 1000;

  // First call should trigger a callback.
  EXPECT_CALL(
      progress_callback_,
      Run(AllOf(Field(&SaveToDriveProgress::status,
                      SaveToDriveStatus::kUploadInProgress),
                Field(&SaveToDriveProgress::uploaded_bytes, 100u),
                Field(&SaveToDriveProgress::file_size_bytes, kTotalBytes))))
      .Times(1);
  uploader->NotifyUploadInProgress(100, kTotalBytes);

  // Subsequent calls within the interval should be ignored.
  EXPECT_CALL(progress_callback_, Run(_)).Times(0);
  uploader->NotifyUploadInProgress(200, kTotalBytes);

  task_environment_.FastForwardBy(base::Milliseconds(499));
  uploader->NotifyUploadInProgress(300, kTotalBytes);

  // After the interval, the next call should trigger a callback.
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_CALL(
      progress_callback_,
      Run(AllOf(Field(&SaveToDriveProgress::status,
                      SaveToDriveStatus::kUploadInProgress),
                Field(&SaveToDriveProgress::uploaded_bytes, 400u),
                Field(&SaveToDriveProgress::file_size_bytes, kTotalBytes))))
      .Times(1);
  uploader->NotifyUploadInProgress(400, kTotalBytes);
}

class FetchParentFolderTest : public DriveUploaderTest {
 public:
  FetchParentFolderTest() = default;
  FetchParentFolderTest(const FetchParentFolderTest&) = delete;
  FetchParentFolderTest& operator=(const FetchParentFolderTest&) = delete;
  ~FetchParentFolderTest() override = default;

  void VerifyParentFolderResponse(std::string_view response_header,
                                  std::string_view response_body,
                                  std::optional<DriveUploader::Item> expected) {
    auto account_info = test_env()->MakePrimaryAccountAvailable(
        "test@example.com", signin::ConsentLevel::kSignin);
    const content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
        [&](content::URLLoaderInterceptor::RequestParams* params) {
          if (params->url_request.url.spec() == kParentFolderUrl) {
            content::URLLoaderInterceptor::WriteResponse(
                response_header, response_body, params->client.get());
            return true;
          }
          return false;
        }));

    EXPECT_CALL(progress_callback_, Run(Field(&SaveToDriveProgress::status,
                                              SaveToDriveStatus::kFetchOauth)));
    TestFuture<void> future;
    FakeDriveUploader uploader("test_title", std::move(account_info),
                               progress_callback_.Get(), profile_.get(),
                               &mock_content_reader_);

    if (expected.has_value()) {
      EXPECT_CALL(progress_callback_,
                  Run(Field(&SaveToDriveProgress::status,
                            SaveToDriveStatus::kFetchParentFolder)))
          .WillOnce(base::test::RunOnceClosure(future.GetCallback()));
      EXPECT_CALL(uploader, UploadFile());
    } else {
      EXPECT_CALL(
          progress_callback_,
          Run(AllOf(Field(&SaveToDriveProgress::status,
                          SaveToDriveStatus::kUploadFailed),
                    Field(&SaveToDriveProgress::error_type,
                          SaveToDriveErrorType::kParentFolderSelectionFailed))))
          .WillOnce(base::test::RunOnceClosure(future.GetCallback()));
    }

    uploader.Start();

    test_env()->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
        "access_token", base::Time::Now() + base::Hours(1));

    EXPECT_TRUE(future.Wait());

    // Verify that the parent folder is set correctly.
    const std::optional<DriveUploader::Item>& parent_folder =
        uploader.parent_folder();
    EXPECT_EQ(parent_folder.has_value(), expected.has_value());

    if (expected.has_value()) {
      EXPECT_EQ(parent_folder->id, expected->id);
      EXPECT_EQ(parent_folder->name, expected->name);
    }
  }
};

TEST_F(FetchParentFolderTest, Success) {
  base::Value::Dict response;
  response.Set("id", kTestFileId);
  response.Set("name", kTestFolderName);
  std::optional<std::string> response_string = base::WriteJson(response);
  ASSERT_TRUE(response_string.has_value());
  VerifyParentFolderResponse(
      kSuccessFulResponseHeader, *response_string,
      DriveUploader::Item{.id = std::string(kTestFileId),
                          .name = std::string(kTestFolderName)});
}

TEST_F(FetchParentFolderTest, EmptyResponse) {
  VerifyParentFolderResponse(kSuccessFulResponseHeader, "", std::nullopt);
}

TEST_F(FetchParentFolderTest, InvalidJson) {
  VerifyParentFolderResponse(kSuccessFulResponseHeader, "{\"id=", std::nullopt);
}

TEST_F(FetchParentFolderTest, InternalError) {
  VerifyParentFolderResponse(kErrorResponseHeader, "{}", std::nullopt);
}

TEST_F(FetchParentFolderTest, MissingId) {
  base::Value::Dict response;
  response.Set("name", kTestFolderName);
  std::optional<std::string> response_string = base::WriteJson(response);
  ASSERT_TRUE(response_string.has_value());
  VerifyParentFolderResponse(kSuccessFulResponseHeader, *response_string,
                             std::nullopt);
}

TEST_F(FetchParentFolderTest, MissingName) {
  base::Value::Dict response;
  response.Set("id", kTestFileId);
  std::optional<std::string> response_string = base::WriteJson(response);
  ASSERT_TRUE(response_string.has_value());
  VerifyParentFolderResponse(kSuccessFulResponseHeader, *response_string,
                             std::nullopt);
}

}  // namespace

}  // namespace save_to_drive
