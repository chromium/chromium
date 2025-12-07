// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/multipart_drive_uploader.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/save_to_drive/content_reader.h"
#include "chrome/browser/save_to_drive/drive_uploader.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/common/extensions/api/pdf_viewer_private.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/url_loader_interceptor.h"
#include "google_apis/gaia/core_account_id.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
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
using testing::Return;

constexpr std::string_view kMultipartUploadUrl =
    "https://www.googleapis.com/upload/drive/v3beta/files?uploadType=multipart";
constexpr std::string_view kTestContent = "test_content";
constexpr std::string_view kTestFileId = "test_file_id";
constexpr std::string_view kSuccessFulResponseHeader =
    "HTTP/1.1 200 OK\nContent-Type: text/html\n\n";
constexpr std::string_view kInternalServerErrorResponse =
    "HTTP/1.1 500 Internal Server Error\nContent-Type: application/json\n\n";
constexpr std::string_view kUnauthorizedErrorResponse =
    "HTTP/1.1 401 Unauthorized\nContent-Type: application/json\n\n";
constexpr std::string_view kForbiddenErrorResponse =
    "HTTP/1.1 403 Forbidden\nContent-Type: application/json\n\n";

AccountInfo CreateAccountInfo() {
  AccountInfo account_info;
  account_info.email = "test@example.com";
  account_info.account_id = CoreAccountId::FromGaiaId(GaiaId("12345"));
  return account_info;
}

std::string GetExpectedMultipartRequestBody(
    const network::ResourceRequest& request) {
  std::string content_type_header =
      request.headers.GetHeader(net::HttpRequestHeaders::kContentType).value();
  constexpr std::string_view kBoundaryPrefix = "multipart/related; boundary=";
  EXPECT_TRUE(base::StartsWith(content_type_header, kBoundaryPrefix));
  const std::string boundary =
      content_type_header.substr(kBoundaryPrefix.size());

  base::Value::Dict metadata;
  metadata.Set("name", "test_title");
  base::Value::List parents;
  parents.Append("test_folder_id");
  metadata.Set("parents", std::move(parents));
  return base::StrCat(
      {"--", boundary,
       "\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n",
       *base::WriteJson(metadata), "\r\n--", boundary,
       "\r\nContent-Type: application/octet-stream\r\n\r\n", kTestContent,
       "\r\n--", boundary, "--\r\n"});
}

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

// A test class for MultipartDriveUploader that allows for setting protected
// members.
class FakeMultipartDriveUploader : public MultipartDriveUploader {
 public:
  FakeMultipartDriveUploader(std::string title,
                             AccountInfo account_info,
                             ProgressCallback progress_callback,
                             Profile* profile,
                             ContentReader* content_reader)
      : MultipartDriveUploader(std::move(title),
                               std::move(account_info),
                               std::move(progress_callback),
                               profile,
                               content_reader) {}
  ~FakeMultipartDriveUploader() override = default;

  void set_parent_folder(std::optional<Item> parent_folder) {
    parent_folder_ = std::move(parent_folder);
  }
};

class MultipartDriveUploaderTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment();
    adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
    uploader_ = std::make_unique<FakeMultipartDriveUploader>(
        "test_title", CreateAccountInfo(), progress_callback_.Get(),
        profile_.get(), &mock_content_reader_);
    uploader_->set_oauth_headers_for_testing(
        {net::HttpRequestHeaders::kAuthorization, "Bearer test_token"});
    uploader_->set_parent_folder(
        DriveUploader::Item{.id = "test_folder_id", .name = "test_folder"});
  }

 protected:
  void RunUploadTestAndExpectError(const std::string_view& headers,
                                   const std::string_view& body,
                                   SaveToDriveErrorType expected_error_type) {
    EXPECT_CALL(mock_content_reader_, GetSize())
        .WillRepeatedly(Return(kTestContent.size()));
    EXPECT_CALL(mock_content_reader_, Read(0, kTestContent.size(), _))
        .WillOnce([&](uint32_t, uint32_t, ContentReadCallback callback) {
          std::move(callback).Run(
              mojo_base::BigBuffer(base::as_byte_span(kTestContent)));
        });

    const content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
        [&](content::URLLoaderInterceptor::RequestParams* params) {
          if (params->url_request.url.spec() == kMultipartUploadUrl) {
            content::URLLoaderInterceptor::WriteResponse(headers, body,
                                                         params->client.get());
            return true;
          }
          return false;
        }));

    EXPECT_CALL(progress_callback_,
                Run(AllOf(Field(&SaveToDriveProgress::status,
                                SaveToDriveStatus::kUploadFailed),
                          Field(&SaveToDriveProgress::error_type,
                                expected_error_type))));

    uploader_->UploadFile();
    task_environment_.RunUntilIdle();
  }

  signin::IdentityTestEnvironment* test_env() {
    return adaptor_->identity_test_env();
  }

  // Must be the first member.
  content::BrowserTaskEnvironment task_environment_;
  MockContentReader mock_content_reader_;
  base::MockCallback<DriveUploader::ProgressCallback> progress_callback_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor> adaptor_;
  std::unique_ptr<FakeMultipartDriveUploader> uploader_;
};

TEST_F(MultipartDriveUploaderTest, UploadSuccess) {
  EXPECT_CALL(mock_content_reader_, GetSize())
      .WillRepeatedly(Return(kTestContent.size()));
  EXPECT_CALL(mock_content_reader_, Read(0, kTestContent.size(), _))
      .WillOnce([&](uint32_t, uint32_t, ContentReadCallback callback) {
        std::move(callback).Run(
            mojo_base::BigBuffer(base::as_bytes(base::span(kTestContent))));
      });

  auto account_info = test_env()->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  const content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        const auto& request = params->url_request;
        if (request.url.spec() != kMultipartUploadUrl) {
          return false;
        }
        EXPECT_EQ(request.method, "POST");
        EXPECT_EQ(request.url.spec(), kMultipartUploadUrl);
        auto& elements = *request.request_body->elements();
        if (elements.size() != 1u) {
          ADD_FAILURE() << "Expected 1 element in the request body, got "
                        << elements.size();
          return false;
        }
        auto request_body =
            elements[0].As<network::DataElementBytes>().AsStringPiece();
        EXPECT_EQ(request_body, GetExpectedMultipartRequestBody(request));

        base::Value::Dict response_dict;
        response_dict.Set("id", kTestFileId);
        response_dict.Set("name", "test_title.pdf");
        content::URLLoaderInterceptor::WriteResponse(
            kSuccessFulResponseHeader, *base::WriteJson(response_dict),
            params->client.get());

        return true;
      }));

  EXPECT_CALL(
      progress_callback_,
      Run(AllOf(
          Field(&SaveToDriveProgress::status,
                SaveToDriveStatus::kUploadCompleted),
          Field(&SaveToDriveProgress::error_type,
                SaveToDriveErrorType::kNoError),
          Field(&SaveToDriveProgress::drive_item_id, kTestFileId),
          Field(&SaveToDriveProgress::file_size_bytes, kTestContent.size()),
          Field(&SaveToDriveProgress::uploaded_bytes, kTestContent.size()),
          Field(&SaveToDriveProgress::file_name, "test_title.pdf"),
          Field(&SaveToDriveProgress::parent_folder_name, "test_folder"))));

  uploader_->UploadFile();
  task_environment_.RunUntilIdle();
}

TEST_F(MultipartDriveUploaderTest, UploadFailsContentReadError) {
  EXPECT_CALL(mock_content_reader_, GetSize())
      .WillRepeatedly(Return(kTestContent.size()));
  EXPECT_CALL(mock_content_reader_, Read(0, kTestContent.size(), _))
      .WillOnce([&](uint32_t, uint32_t, ContentReadCallback callback) {
        std::move(callback).Run(mojo_base::BigBuffer());
      });

  auto account_info = test_env()->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);
  EXPECT_CALL(progress_callback_,
              Run(AllOf(Field(&SaveToDriveProgress::status,
                              SaveToDriveStatus::kUploadFailed),
                        Field(&SaveToDriveProgress::error_type,
                              SaveToDriveErrorType::kUnknownError))));

  uploader_->UploadFile();
  task_environment_.RunUntilIdle();
}

TEST_F(MultipartDriveUploaderTest, UploadFailsNetError) {
  EXPECT_CALL(mock_content_reader_, GetSize())
      .WillRepeatedly(Return(kTestContent.size()));
  EXPECT_CALL(mock_content_reader_, Read(0, kTestContent.size(), _))
      .WillOnce([&](uint32_t, uint32_t, ContentReadCallback callback) {
        std::move(callback).Run(
            mojo_base::BigBuffer(base::as_byte_span(kTestContent)));
      });
  // No URLLoaderInterceptor is set up, so the request will fail with a net
  // error.
  EXPECT_CALL(progress_callback_,
              Run(AllOf(Field(&SaveToDriveProgress::status,
                              SaveToDriveStatus::kUploadFailed),
                        Field(&SaveToDriveProgress::error_type,
                              SaveToDriveErrorType::kOffline))));

  uploader_->UploadFile();
  task_environment_.RunUntilIdle();
}

TEST_F(MultipartDriveUploaderTest, UploadFailsHttpError) {
  RunUploadTestAndExpectError(kInternalServerErrorResponse, "{}",
                              SaveToDriveErrorType::kUnknownError);
}

TEST_F(MultipartDriveUploaderTest, UploadFailsUnauthorized) {
  RunUploadTestAndExpectError(kUnauthorizedErrorResponse, "{}",
                              SaveToDriveErrorType::kOauthError);
}

TEST_F(MultipartDriveUploaderTest, UploadFailsForbidden) {
  RunUploadTestAndExpectError(kForbiddenErrorResponse, "{}",
                              SaveToDriveErrorType::kUnknownError);
}

TEST_F(MultipartDriveUploaderTest, UploadFailsQuotaExceeded) {
  RunUploadTestAndExpectError(
      kForbiddenErrorResponse,
      R"({"error":{"errors":[{"reason":"quotaExceeded"}]}})",
      SaveToDriveErrorType::kQuotaExceeded);
}

TEST_F(MultipartDriveUploaderTest, UploadFailsStorageQuotaExceeded) {
  RunUploadTestAndExpectError(
      kForbiddenErrorResponse,
      R"({"error":{"errors":[{"reason":"storageQuotaExceeded"}]}})",
      SaveToDriveErrorType::kQuotaExceeded);
}

TEST_F(MultipartDriveUploaderTest, UploadFailsEmptyResponse) {
  RunUploadTestAndExpectError(kSuccessFulResponseHeader, "",
                              SaveToDriveErrorType::kUnknownError);
}

TEST_F(MultipartDriveUploaderTest, UploadFailsInvalidJsonResponse) {
  RunUploadTestAndExpectError(kSuccessFulResponseHeader, "invalid json",
                              SaveToDriveErrorType::kUnknownError);
}

TEST_F(MultipartDriveUploaderTest, UploadFailsNoFileIdInResponse) {
  RunUploadTestAndExpectError(kSuccessFulResponseHeader, R"({"name":"test"})",
                              SaveToDriveErrorType::kUnknownError);
}

TEST_F(MultipartDriveUploaderTest, UploadFailsNoNameInResponse) {
  RunUploadTestAndExpectError(kSuccessFulResponseHeader, R"({"id":"test_id"})",
                              SaveToDriveErrorType::kUnknownError);
}

TEST_F(MultipartDriveUploaderTest, UploadFailsNoOAuthToken) {
  uploader_->set_oauth_headers_for_testing({});
  EXPECT_CALL(mock_content_reader_, GetSize())
      .WillRepeatedly(Return(kTestContent.size()));
  EXPECT_CALL(mock_content_reader_, Read(0, kTestContent.size(), _))
      .WillOnce([&](uint32_t, uint32_t, ContentReadCallback callback) {
        std::move(callback).Run(
            mojo_base::BigBuffer(base::as_bytes(base::span(kTestContent))));
      });
  EXPECT_CALL(progress_callback_,
              Run(AllOf(Field(&SaveToDriveProgress::status,
                              SaveToDriveStatus::kUploadFailed),
                        Field(&SaveToDriveProgress::error_type,
                              SaveToDriveErrorType::kOauthError))));
  uploader_->UploadFile();
  task_environment_.RunUntilIdle();
}

TEST_F(MultipartDriveUploaderTest, UploadFailsNoParentFolder) {
  uploader_->set_parent_folder(std::nullopt);
  EXPECT_CALL(mock_content_reader_, GetSize())
      .WillRepeatedly(Return(kTestContent.size()));
  EXPECT_CALL(mock_content_reader_, Read(0, kTestContent.size(), _))
      .WillOnce([&](uint32_t, uint32_t, ContentReadCallback callback) {
        std::move(callback).Run(
            mojo_base::BigBuffer(base::as_bytes(base::span(kTestContent))));
      });
  EXPECT_CALL(
      progress_callback_,
      Run(AllOf(
          Field(&SaveToDriveProgress::status, SaveToDriveStatus::kUploadFailed),
          Field(&SaveToDriveProgress::error_type,
                SaveToDriveErrorType::kParentFolderSelectionFailed))));
  uploader_->UploadFile();
  task_environment_.RunUntilIdle();
}

}  // namespace

}  // namespace save_to_drive
