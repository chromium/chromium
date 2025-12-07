// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/resumable_drive_uploader.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
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
#include "services/network/public/cpp/resource_request.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace save_to_drive {
namespace {

using extensions::api::pdf_viewer_private::SaveToDriveErrorType;
using extensions::api::pdf_viewer_private::SaveToDriveProgress;
using extensions::api::pdf_viewer_private::SaveToDriveStatus;
using testing::_;
using testing::AllOf;
using testing::Field;
using testing::Return;

constexpr char kInitiationUrl[] =
    "https://www.googleapis.com/upload/drive/v3beta/"
    "files";
constexpr char kTestUploadUrl[] = "https://example.com/upload";
constexpr std::string_view kTestContent = "test_content";
constexpr std::string_view kTestFileId = "test_file_id";
constexpr std::string_view kTestTitle = "test_title";

constexpr std::string_view kSuccessFulResponseHeader =
    "HTTP/1.1 200 OK\nContent-Type: application/json\n\n";
constexpr std::string_view kSuccessFulFinalResponseHeader =
    "HTTP/1.1 200 OK\nContent-Type: application/json\nX-Goog-Upload-Status: "
    "final\n\n";
constexpr std::string_view kSuccessFulActiveResponseHeader =
    "HTTP/1.1 200 OK\nContent-Type: application/json\nX-Goog-Upload-Status: "
    "active\n\n";
constexpr std::string_view kUnauthorizedErrorResponse =
    "HTTP/1.1 401 Unauthorized\nContent-Type: application/json\n\n";
constexpr std::string_view kForbiddenErrorResponse =
    "HTTP/1.1 403 Forbidden\nContent-Type: application/json\n\n";
constexpr std::string_view kNotFoundErrorResponse =
    "HTTP/1.1 404 Not Found\nContent-Type: application/json\n\n";
constexpr std::string_view kInternalServerErrorResponse =
    "HTTP/1.1 500 Internal Server Error\nContent-Type: application/json\n\n";

AccountInfo CreateAccountInfo() {
  AccountInfo account_info;
  account_info.email = "test@example.com";
  account_info.account_id = CoreAccountId::FromGaiaId(GaiaId("12345"));
  return account_info;
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

// A test class for ResumableDriveUploader that allows for setting protected
// members.
class FakeResumableDriveUploader : public ResumableDriveUploader {
 public:
  FakeResumableDriveUploader(std::string title,
                             AccountInfo account_info,
                             ProgressCallback progress_callback,
                             Profile* profile,
                             ContentReader* content_reader)
      : ResumableDriveUploader(std::move(title),
                               std::move(account_info),
                               std::move(progress_callback),
                               profile,
                               content_reader) {}
  ~FakeResumableDriveUploader() override = default;

  void set_parent_folder(std::optional<Item> parent_folder) {
    parent_folder_ = std::move(parent_folder);
  }
};

class ResumableDriveUploaderTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment();
    adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
    uploader_ = std::make_unique<FakeResumableDriveUploader>(
        std::string(kTestTitle), CreateAccountInfo(), progress_callback_.Get(),
        profile_.get(), &mock_content_reader_);
    uploader_->set_oauth_headers_for_testing(
        {net::HttpRequestHeaders::kAuthorization, "Bearer test_token"});
    uploader_->set_parent_folder(
        DriveUploader::Item{.id = "test_folder_id", .name = "test_folder"});
  }

 protected:
  void MockSuccessfulInitiation(
      content::URLLoaderInterceptor::RequestParams* params) {
    const auto& request = params->url_request;
    EXPECT_EQ(request.method, "POST");

    ASSERT_TRUE(request.request_body);
    auto& elements = *request.request_body->elements();
    ASSERT_EQ(elements.size(), 1u);
    EXPECT_EQ(elements[0].As<network::DataElementBytes>().AsStringView(),
              *base::WriteJson(base::Value::Dict()
                                   .Set("name", kTestTitle)
                                   .Set("parents", base::Value::List().Append(
                                                       "test_folder_id"))));

    std::optional<std::string> content_length =
        request.headers.GetHeader("X-Goog-Upload-Header-Content-Length");
    ASSERT_TRUE(content_length);
    EXPECT_EQ(*content_length, base::NumberToString(kTestContent.size()));

    std::optional<std::string> upload_protocol =
        request.headers.GetHeader("X-Goog-Upload-Protocol");
    ASSERT_TRUE(upload_protocol);
    EXPECT_EQ(*upload_protocol, "resumable");

    std::optional<std::string> upload_command =
        request.headers.GetHeader("X-Goog-Upload-Command");
    ASSERT_TRUE(upload_command);
    EXPECT_EQ(*upload_command, "start");

    std::optional<std::string> upload_content_type =
        request.headers.GetHeader("X-Goog-Upload-Header-Content-Type");
    ASSERT_TRUE(upload_content_type);
    EXPECT_EQ(*upload_content_type, "application/octet-stream");

    content::URLLoaderInterceptor::WriteResponse(
        base::StrCat({"HTTP/1.1 200 OK\nX-Goog-Upload-URL: ", kTestUploadUrl,
                      "\nX-Goog-Upload-Status: active\n\n"}),
        "", params->client.get());
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
  std::unique_ptr<FakeResumableDriveUploader> uploader_;
};

TEST_F(ResumableDriveUploaderTest, UploadSuccess) {
  EXPECT_CALL(mock_content_reader_, GetSize())
      .WillRepeatedly(Return(kTestContent.size()));
  EXPECT_CALL(mock_content_reader_, Read(0, kTestContent.size(), _))
      .WillOnce([&](uint32_t, uint32_t, ContentReadCallback callback) {
        std::move(callback).Run(
            mojo_base::BigBuffer(base::as_byte_span(kTestContent)));
      });

  const content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        const auto& request = params->url_request;
        if (request.url == GURL(kInitiationUrl)) {
          MockSuccessfulInitiation(params);
          return true;
        }

        if (request.url == GURL(kTestUploadUrl)) {
          // Upload request.
          EXPECT_EQ(request.method, "PUT");
          if (!request.request_body) {
            ADD_FAILURE() << "Request body is missing";
            return false;
          }
          auto& elements = *request.request_body->elements();
          if (elements.size() != 1u) {
            ADD_FAILURE() << "Expected 1 element, got " << elements.size();
            return false;
          }
          EXPECT_EQ(elements[0].As<network::DataElementBytes>().AsStringView(),
                    kTestContent);
          content::URLLoaderInterceptor::WriteResponse(
              kSuccessFulFinalResponseHeader,
              *base::WriteJson(base::Value::Dict()
                                   .Set("id", kTestFileId)
                                   .Set("name", kTestTitle)),
              params->client.get());
          return true;
        }
        return false;
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
          Field(&SaveToDriveProgress::file_name, kTestTitle),
          Field(&SaveToDriveProgress::parent_folder_name, "test_folder"))));

  uploader_->UploadFile();
  task_environment_.RunUntilIdle();
}

TEST_F(ResumableDriveUploaderTest, UploadSuccessInChunks) {
  EXPECT_CALL(mock_content_reader_, GetSize())
      .WillRepeatedly(Return(kTestContent.size()));
  EXPECT_CALL(mock_content_reader_, Read(0, kTestContent.size(), _))
      .WillOnce([&](uint32_t, uint32_t, ContentReadCallback callback) {
        std::move(callback).Run(mojo_base::BigBuffer(
            base::as_byte_span(kTestContent.substr(0, 4))));
      });
  EXPECT_CALL(mock_content_reader_, Read(4, kTestContent.size() - 4, _))
      .WillOnce([&](uint32_t, uint32_t, ContentReadCallback callback) {
        std::move(callback).Run(
            mojo_base::BigBuffer(base::as_byte_span(kTestContent.substr(4))));
      });

  const content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        const auto& request = params->url_request;
        if (request.url == GURL(kInitiationUrl)) {
          MockSuccessfulInitiation(params);
          return true;
        }

        if (request.url == GURL(kTestUploadUrl)) {
          std::optional<std::string> offset =
              request.headers.GetHeader("X-Goog-Upload-Offset");
          if (!offset) {
            ADD_FAILURE() << "X-Goog-Upload-Offset header is missing";
            return false;
          }
          std::optional<std::string> upload_command =
              request.headers.GetHeader("X-Goog-Upload-Command");
          if (!upload_command) {
            ADD_FAILURE() << "X-Goog-Upload-command header is missing";
            return false;
          }
          if (offset == "0") {
            EXPECT_EQ(*upload_command, "upload");
            content::URLLoaderInterceptor::WriteResponse(
                kSuccessFulActiveResponseHeader, "", params->client.get());

          } else {
            EXPECT_EQ(*upload_command, "upload, finalize");
            content::URLLoaderInterceptor::WriteResponse(
                kSuccessFulFinalResponseHeader,
                *base::WriteJson(base::Value::Dict()
                                     .Set("id", kTestFileId)
                                     .Set("name", kTestTitle)),
                params->client.get());
          }
          return true;
        }
        return false;
      }));

  EXPECT_CALL(progress_callback_,
              Run(AllOf(Field(&SaveToDriveProgress::status,
                              SaveToDriveStatus::kUploadInProgress),
                        Field(&SaveToDriveProgress::error_type,
                              SaveToDriveErrorType::kNoError),
                        Field(&SaveToDriveProgress::uploaded_bytes, 4))));
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
          Field(&SaveToDriveProgress::file_name, kTestTitle),
          Field(&SaveToDriveProgress::parent_folder_name, "test_folder"))));
  uploader_->UploadFile();
  task_environment_.RunUntilIdle();
}

TEST_F(ResumableDriveUploaderTest, UploadFailsInitiationHttpError) {
  EXPECT_CALL(mock_content_reader_, GetSize())
      .WillRepeatedly(Return(kTestContent.size()));
  const content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        content::URLLoaderInterceptor::WriteResponse(
            kInternalServerErrorResponse, "{}", params->client.get());
        return true;
      }));

  EXPECT_CALL(progress_callback_,
              Run(AllOf(Field(&SaveToDriveProgress::status,
                              SaveToDriveStatus::kUploadFailed),
                        Field(&SaveToDriveProgress::error_type,
                              SaveToDriveErrorType::kUnknownError))));
  uploader_->UploadFile();
  task_environment_.RunUntilIdle();
}

TEST_F(ResumableDriveUploaderTest, UploadFailsInitiationNetworkError) {
  EXPECT_CALL(mock_content_reader_, GetSize())
      .WillRepeatedly(Return(kTestContent.size()));
  EXPECT_CALL(progress_callback_,
              Run(AllOf(Field(&SaveToDriveProgress::status,
                              SaveToDriveStatus::kUploadFailed),
                        Field(&SaveToDriveProgress::error_type,
                              SaveToDriveErrorType::kOffline))));
  uploader_->UploadFile();
  task_environment_.RunUntilIdle();
}

TEST_F(ResumableDriveUploaderTest, UploadFailsInitiationNoUploadUrlHeader) {
  EXPECT_CALL(mock_content_reader_, GetSize())
      .WillRepeatedly(Return(kTestContent.size()));
  const content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        content::URLLoaderInterceptor::WriteResponse(kSuccessFulResponseHeader,
                                                     "", params->client.get());
        return true;
      }));

  EXPECT_CALL(progress_callback_,
              Run(AllOf(Field(&SaveToDriveProgress::status,
                              SaveToDriveStatus::kUploadFailed),
                        Field(&SaveToDriveProgress::error_type,
                              SaveToDriveErrorType::kUnknownError))));
  uploader_->UploadFile();
  task_environment_.RunUntilIdle();
}

TEST_F(ResumableDriveUploaderTest, UploadFailsContentReadError) {
  EXPECT_CALL(mock_content_reader_, GetSize())
      .WillRepeatedly(Return(kTestContent.size()));
  EXPECT_CALL(mock_content_reader_, Read(_, _, _))
      .WillOnce([&](uint32_t, uint32_t, ContentReadCallback callback) {
        std::move(callback).Run(mojo_base::BigBuffer());
      });

  const content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        MockSuccessfulInitiation(params);
        return true;
      }));
  EXPECT_CALL(progress_callback_,
              Run(AllOf(Field(&SaveToDriveProgress::status,
                              SaveToDriveStatus::kUploadFailed),
                        Field(&SaveToDriveProgress::error_type,
                              SaveToDriveErrorType::kUnknownError))));
  uploader_->UploadFile();
  task_environment_.RunUntilIdle();
}

TEST_F(ResumableDriveUploaderTest, UploadFailsUploadHttpError) {
  EXPECT_CALL(mock_content_reader_, GetSize())
      .WillRepeatedly(Return(kTestContent.size()));
  EXPECT_CALL(mock_content_reader_, Read(_, _, _))
      .WillOnce([&](uint32_t, uint32_t, ContentReadCallback callback) {
        std::move(callback).Run(
            mojo_base::BigBuffer(base::as_byte_span(kTestContent)));
      });

  const content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url == GURL(kInitiationUrl)) {
          MockSuccessfulInitiation(params);
        } else {
          content::URLLoaderInterceptor::WriteResponse(
              kInternalServerErrorResponse, "{}", params->client.get());
        }
        return true;
      }));

  EXPECT_CALL(progress_callback_,
              Run(AllOf(Field(&SaveToDriveProgress::status,
                              SaveToDriveStatus::kUploadFailed),
                        Field(&SaveToDriveProgress::error_type,
                              SaveToDriveErrorType::kUnknownError))));
  uploader_->UploadFile();
  task_environment_.RunUntilIdle();
}

TEST_F(ResumableDriveUploaderTest, UploadFailsUnauthorized) {
  EXPECT_CALL(mock_content_reader_, GetSize())
      .WillRepeatedly(Return(kTestContent.size()));
  EXPECT_CALL(mock_content_reader_, Read(_, _, _))
      .WillOnce([&](uint32_t, uint32_t, ContentReadCallback callback) {
        std::move(callback).Run(
            mojo_base::BigBuffer(base::as_byte_span(kTestContent)));
      });

  const content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url == GURL(kInitiationUrl)) {
          MockSuccessfulInitiation(params);
        } else {
          content::URLLoaderInterceptor::WriteResponse(
              kUnauthorizedErrorResponse, "{}", params->client.get());
        }
        return true;
      }));

  EXPECT_CALL(progress_callback_,
              Run(AllOf(Field(&SaveToDriveProgress::status,
                              SaveToDriveStatus::kUploadFailed),
                        Field(&SaveToDriveProgress::error_type,
                              SaveToDriveErrorType::kOauthError))));
  uploader_->UploadFile();
  task_environment_.RunUntilIdle();
}

TEST_F(ResumableDriveUploaderTest, UploadFailsQuotaExceeded) {
  EXPECT_CALL(mock_content_reader_, GetSize())
      .WillRepeatedly(Return(kTestContent.size()));
  EXPECT_CALL(mock_content_reader_, Read(_, _, _))
      .WillOnce([&](uint32_t, uint32_t, ContentReadCallback callback) {
        std::move(callback).Run(
            mojo_base::BigBuffer(base::as_byte_span(kTestContent)));
      });

  const content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url == GURL(kInitiationUrl)) {
          MockSuccessfulInitiation(params);
        } else {
          content::URLLoaderInterceptor::WriteResponse(
              kForbiddenErrorResponse,
              R"({"error":{"errors":[{"reason":"quotaExceeded"}]}})",
              params->client.get());
        }
        return true;
      }));

  EXPECT_CALL(progress_callback_,
              Run(AllOf(Field(&SaveToDriveProgress::status,
                              SaveToDriveStatus::kUploadFailed),
                        Field(&SaveToDriveProgress::error_type,
                              SaveToDriveErrorType::kQuotaExceeded))));
  uploader_->UploadFile();
  task_environment_.RunUntilIdle();
}

TEST_F(ResumableDriveUploaderTest, UploadResumesAfterNotFound) {
  int initiation_request_count = 0;
  const content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url == GURL(kInitiationUrl)) {
          initiation_request_count++;
          MockSuccessfulInitiation(params);
          return true;
        }

        if (params->url_request.url != GURL(kTestUploadUrl)) {
          return false;
        }

        if (initiation_request_count == 1) {
          // First attempt fails with 404.
          content::URLLoaderInterceptor::WriteResponse(
              kNotFoundErrorResponse, "", params->client.get());
          return true;
        } else if (initiation_request_count == 2) {
          // Second attempt succeeds.
          content::URLLoaderInterceptor::WriteResponse(
              kSuccessFulFinalResponseHeader,
              *base::WriteJson(base::Value::Dict()
                                   .Set("id", kTestFileId)
                                   .Set("name", kTestTitle)),
              params->client.get());
          return true;
        }
        ADD_FAILURE() << "Unexpected request count: "
                      << initiation_request_count;
        return false;
      }));

  EXPECT_CALL(mock_content_reader_, GetSize())
      .WillRepeatedly(Return(kTestContent.size()));
  EXPECT_CALL(mock_content_reader_, Read(_, _, _))
      .WillRepeatedly(([&](uint32_t, uint32_t, ContentReadCallback callback) {
        std::move(callback).Run(
            mojo_base::BigBuffer(base::as_byte_span(kTestContent)));
      }));
  EXPECT_CALL(progress_callback_,
              Run(AllOf(Field(&SaveToDriveProgress::status,
                              SaveToDriveStatus::kUploadCompleted),
                        Field(&SaveToDriveProgress::error_type,
                              SaveToDriveErrorType::kNoError))));

  uploader_->UploadFile();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(initiation_request_count, 2);
}

TEST_F(ResumableDriveUploaderTest, UploadFailsNoOAuthToken) {
  uploader_->set_oauth_headers_for_testing({});
  EXPECT_CALL(progress_callback_,
              Run(AllOf(Field(&SaveToDriveProgress::status,
                              SaveToDriveStatus::kUploadFailed),
                        Field(&SaveToDriveProgress::error_type,
                              SaveToDriveErrorType::kOauthError))));
  uploader_->UploadFile();
  task_environment_.RunUntilIdle();
}

TEST_F(ResumableDriveUploaderTest, UploadFailsNoParentFolder) {
  uploader_->set_parent_folder(std::nullopt);
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
