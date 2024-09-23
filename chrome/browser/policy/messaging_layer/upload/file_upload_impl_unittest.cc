// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/policy/messaging_layer/upload/file_upload_impl.h"

#include <cstddef>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/queue.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/uploading/upload_job_impl.h"
#include "components/reporting/proto/synced/upload_tracker.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/fake_oauth2_access_token_manager.h"
#include "google_apis/gaia/gaia_access_token_fetcher.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_network_context.h"
#include "services/network/test/test_network_context_client.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::IsSupersetOf;
using ::testing::Pair;
using ::testing::Property;
using ::testing::StartsWith;
using ::testing::StrEq;

namespace reporting {

namespace {

constexpr char kUploadPath[] = "/upload";
constexpr char kRobotAccountId[] = "robot@gserviceaccount.com";
constexpr size_t kDataGranularity = 10;
constexpr size_t kMaxUploadBufferSize = kDataGranularity * 2;
constexpr char kUploadId[] = "ABC";
constexpr char kUploadMedata[] =
    "<File-Type>\r\n"
    "  support_file\r\n"
    "</File-Type>\r\n"
    "<Command-ID>\r\n"
    "  ID12345\r\n"
    "</Command-ID>\r\n"
    "<Filename>\r\n"
    "  resulting_file_name\r\n"
    "</Filename>\r\n";
constexpr char kUploadMetadataContentType[] = "text/xml";
constexpr char kResumableUrl[] =
    "/upload?upload_id=ABC&upload_protocol=resumable";
constexpr char kTokenInvalid[] = "INVALID_TOKEN";
constexpr char kTokenValid[] = "VALID_TOKEN";

constexpr char kTestData[] =
    "0123456789012345678901234567890123456789012345678901234567890123456789";
constexpr size_t kTestDataSize = sizeof(kTestData) - 1;

constexpr char kUploadStatusHeader[] = "X-Goog-Upload-Status";
constexpr char kUploadCommandHeader[] = "X-Goog-Upload-Command";
constexpr char kUploadChunkGranularityHeader[] =
    "X-Goog-Upload-Chunk-Granularity";
constexpr char kUploadUrlHeader[] = "X-Goog-Upload-Url";
constexpr char kUploadSizeReceivedHeader[] = "X-Goog-Upload-Size-Received";
constexpr char kUploadOffsetHeader[] = "X-Goog-Upload-Offset";
constexpr char kUploadProtocolHeader[] = "X-Goog-Upload-Protocol";
constexpr char kUploadIdHeader[] = "X-GUploader-UploadID";

// Test-only access token manager fake, that allows to pre-populate
// expected valid and invalid tokens ahead of the test execution.
class FakeOAuth2AccessTokenManagerWithCaching
    : public FakeOAuth2AccessTokenManager {
 public:
  explicit FakeOAuth2AccessTokenManagerWithCaching(
      OAuth2AccessTokenManager::Delegate* delegate);

  FakeOAuth2AccessTokenManagerWithCaching(
      const FakeOAuth2AccessTokenManagerWithCaching&) = delete;
  FakeOAuth2AccessTokenManagerWithCaching& operator=(
      const FakeOAuth2AccessTokenManagerWithCaching&) = delete;

  ~FakeOAuth2AccessTokenManagerWithCaching() override;

  // FakeOAuth2AccessTokenManager:
  void FetchOAuth2Token(
      OAuth2AccessTokenManager::RequestImpl* request,
      const CoreAccountId& account_id,
      scoped_refptr<::network::SharedURLLoaderFactory> url_loader_factory,
      const std::string& client_id,
      const std::string& client_secret,
      const std::string& consumer_name,
      const OAuth2AccessTokenManager::ScopeSet& scopes) override;
  void InvalidateAccessTokenImpl(
      const CoreAccountId& account_id,
      const std::string& client_id,
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      const std::string& access_token) override;

  void AddTokenToQueue(const std::string& token);
  bool IsTokenValid(const std::string& token) const;
  void SetTokenValid(const std::string& token);
  void SetTokenInvalid(const std::string& token);

 private:
  base::queue<std::string> token_replies_;
  std::set<std::string> valid_tokens_;
};

FakeOAuth2AccessTokenManagerWithCaching::
    FakeOAuth2AccessTokenManagerWithCaching(
        OAuth2AccessTokenManager::Delegate* delegate)
    : FakeOAuth2AccessTokenManager(delegate) {}

FakeOAuth2AccessTokenManagerWithCaching::
    ~FakeOAuth2AccessTokenManagerWithCaching() = default;

void FakeOAuth2AccessTokenManagerWithCaching::FetchOAuth2Token(
    OAuth2AccessTokenManager::RequestImpl* request,
    const CoreAccountId& account_id,
    scoped_refptr<::network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& client_id,
    const std::string& client_secret,
    const std::string& consumer_name,
    const OAuth2AccessTokenManager::ScopeSet& scopes) {
  GoogleServiceAuthError response_error =
      GoogleServiceAuthError::AuthErrorNone();
  OAuth2AccessTokenConsumer::TokenResponse token_response;
  if (token_replies_.empty()) {
    response_error =
        GoogleServiceAuthError::FromServiceError("Service unavailable.");
  } else {
    token_response.access_token = token_replies_.front();
    token_response.expiration_time = base::Time::Now();
    token_replies_.pop();
  }
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&OAuth2AccessTokenManager::RequestImpl::InformConsumer,
                     request->AsWeakPtr(), response_error, token_response));
}

void FakeOAuth2AccessTokenManagerWithCaching::AddTokenToQueue(
    const std::string& token) {
  token_replies_.push(token);
}

bool FakeOAuth2AccessTokenManagerWithCaching::IsTokenValid(
    const std::string& token) const {
  return valid_tokens_.find(token) != valid_tokens_.end();
}

void FakeOAuth2AccessTokenManagerWithCaching::SetTokenValid(
    const std::string& token) {
  valid_tokens_.insert(token);
}

void FakeOAuth2AccessTokenManagerWithCaching::SetTokenInvalid(
    const std::string& token) {
  valid_tokens_.erase(token);
}

void FakeOAuth2AccessTokenManagerWithCaching::InvalidateAccessTokenImpl(
    const CoreAccountId& account_id,
    const std::string& client_id,
    const OAuth2AccessTokenManager::ScopeSet& scopes,
    const std::string& access_token) {
  SetTokenInvalid(access_token);
}

class FakeOAuth2AccessTokenManagerDelegate
    : public OAuth2AccessTokenManager::Delegate {
 public:
  FakeOAuth2AccessTokenManagerDelegate() = default;
  ~FakeOAuth2AccessTokenManagerDelegate() override = default;

  // OAuth2AccessTokenManager::Delegate:
  std::unique_ptr<OAuth2AccessTokenFetcher> CreateAccessTokenFetcher(
      const CoreAccountId& account_id,
      scoped_refptr<::network::SharedURLLoaderFactory> url_loader_factory,
      OAuth2AccessTokenConsumer* consumer,
      const std::string& token_binding_challenge) override {
    EXPECT_EQ(CoreAccountId::FromRobotEmail(kRobotAccountId), account_id);
    return GaiaAccessTokenFetcher::
        CreateExchangeRefreshTokenForAccessTokenInstance(
            consumer, url_loader_factory, "fake_refresh_token");
  }

  bool HasRefreshToken(const CoreAccountId& account_id) const override {
    return CoreAccountId::FromEmail(kRobotAccountId) == account_id;
  }
};
}  // namespace

class FileUploadDelegateTest : public ::testing::Test {
 protected:
  FileUploadDelegateTest() { DETACH_FROM_SEQUENCE(sequence_checker_); }

  const GURL GetServerURL(std::string_view relative_path) const {
    return test_server_.GetURL(relative_path);
  }

  void SetUp() override {
    memory_resource_ =
        base::MakeRefCounted<ResourceManager>(4u * 1024LLu * 1024LLu);  // 4 MiB

    url_loader_factory_ =
        base::MakeRefCounted<::network::TestSharedURLLoaderFactory>();
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &FileUploadDelegateTest::HandlePostRequest, base::Unretained(this)));
    ASSERT_TRUE(test_server_.Start());
    PrepareFileForUpload();
  }

  void TearDown() override {
    ASSERT_TRUE(test_server_.ShutdownAndWaitUntilComplete());
    EXPECT_THAT(memory_resource_->GetUsed(), Eq(0uL));
  }

  std::unique_ptr<FileUploadDelegate> PrepareFileUploadDelegate() {
    auto delegate = std::make_unique<FileUploadDelegate>();
    DCHECK_CALLED_ON_VALID_SEQUENCE(delegate->sequence_checker_);
    delegate->upload_url_ = GetServerURL(kUploadPath);
    delegate->account_id_ = CoreAccountId::FromRobotEmail(kRobotAccountId);
    delegate->access_token_manager_ = &access_token_manager_;
    delegate->url_loader_factory_ = url_loader_factory_;
    delegate->traffic_annotation_ =
        std::make_unique<::net::NetworkTrafficAnnotationTag>(
            TRAFFIC_ANNOTATION_FOR_TESTS);
    delegate->max_upload_buffer_size_ = 20;
    return delegate;
  }

  void PrepareFileForUpload() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    origin_path_ = temp_dir_.GetPath().AppendASCII("upload_file");
    base::File file(origin_path_,
                    base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    ASSERT_TRUE(file.IsValid());
    ASSERT_THAT(file.error_details(), Eq(base::File::FILE_OK));

    const int bytes_written = file.Write(0, kTestData, kTestDataSize);
    EXPECT_THAT(bytes_written, Eq(static_cast<int>(kTestDataSize)));
  }

  std::unique_ptr<::net::test_server::HttpResponse> HandlePostRequest(
      const ::net::test_server::HttpRequest& request) {
    auto response = std::make_unique<::net::test_server::BasicHttpResponse>();
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    mock_request_call_.Call(request, response.get());
    return std::move(response);
  }

  void ExpectStart(const ::net::test_server::HttpRequest& request) {
    EXPECT_THAT(request.method, Eq(::net::test_server::METHOD_POST));
    EXPECT_THAT(request.headers,
                IsSupersetOf({Pair("Authorization", StartsWith("Bearer "))}));
    EXPECT_THAT(
        request.headers,
        IsSupersetOf({
            Pair("Authorization",
                 ::testing::MatcherCast<std::string>(StartsWith("Bearer "))),
            Pair(kUploadProtocolHeader,
                 ::testing::MatcherCast<std::string>(StrEq("resumable"))),
            Pair(kUploadCommandHeader,
                 ::testing::MatcherCast<std::string>(StrEq("start"))),
            Pair("X-Goog-Upload-Header-Content-Length",
                 ::testing::MatcherCast<std::string>(
                     StrEq(base::NumberToString(kTestDataSize)))),
            Pair("X-Goog-Upload-Header-Content-Type",
                 ::testing::MatcherCast<std::string>(
                     StrEq("application/octet-stream"))),
            Pair("Content-Type", ::testing::MatcherCast<std::string>(
                                     kUploadMetadataContentType)),
        }));
    EXPECT_TRUE(request.has_content);
    EXPECT_THAT(request.content, StrEq(kUploadMedata));
  }

  void ExpectQuery(const ::net::test_server::HttpRequest& request) {
    EXPECT_THAT(request.method, Eq(::net::test_server::METHOD_POST));
    EXPECT_THAT(request.relative_url, StrEq(kResumableUrl));
    EXPECT_THAT(request.headers,
                IsSupersetOf({
                    Pair(kUploadProtocolHeader, StrEq("resumable")),
                    Pair(kUploadCommandHeader, StrEq("query")),
                }));
  }

  void ExpectStep(size_t offset,
                  const ::net::test_server::HttpRequest& request) {
    EXPECT_THAT(request.method, Eq(::net::test_server::METHOD_POST));
    EXPECT_THAT(request.relative_url, StrEq(kResumableUrl));
    EXPECT_THAT(
        request.headers,
        IsSupersetOf({
            Pair(kUploadProtocolHeader, StrEq("resumable")),
            Pair(kUploadCommandHeader, StrEq("upload")),
            Pair(kUploadOffsetHeader, StrEq(base::NumberToString(offset))),
        }));
  }

  void ExpectFinish(const ::net::test_server::HttpRequest& request) {
    EXPECT_THAT(request.method, Eq(::net::test_server::METHOD_POST));
    EXPECT_THAT(request.relative_url, StrEq(kResumableUrl));
    EXPECT_THAT(request.headers,
                IsSupersetOf({
                    Pair(kUploadProtocolHeader, StrEq("resumable")),
                    Pair(kUploadCommandHeader, StrEq("finalize")),
                }));
  }

  std::string origin_path() const { return origin_path_.MaybeAsASCII(); }

  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::MainThreadType::IO};

  // Make sure `mock_request_call_` is called sequentially.
  SEQUENCE_CHECKER(sequence_checker_);
  ::testing::MockFunction<void(const ::net::test_server::HttpRequest& request,
                               ::net::test_server::BasicHttpResponse* response)>
      mock_request_call_;

  FakeOAuth2AccessTokenManagerWithCaching access_token_manager_{
      &token_manager_delegate_};

  scoped_refptr<ResourceManager> memory_resource_;

 private:
  base::ScopedTempDir temp_dir_;
  base::FilePath origin_path_;
  ::net::EmbeddedTestServer test_server_;
  scoped_refptr<::network::TestSharedURLLoaderFactory> url_loader_factory_;
  FakeOAuth2AccessTokenManagerDelegate token_manager_delegate_;
};

TEST_F(FileUploadDelegateTest, SuccessfulUploadStart) {
  // Prepare the delegate.
  std::unique_ptr<FileUploadJob::Delegate> delegate =
      PrepareFileUploadDelegate();

  // Prepare access token.
  access_token_manager_.SetTokenValid(kTokenValid);
  access_token_manager_.AddTokenToQueue(kTokenValid);

  // Set up responses.
  EXPECT_CALL(mock_request_call_, Call(_, _))
      .WillOnce(Invoke([this](const ::net::test_server::HttpRequest& request,
                              ::net::test_server::BasicHttpResponse* response) {
        ExpectStart(request);
        response->AddCustomHeader(kUploadStatusHeader, "active");
        response->AddCustomHeader(kUploadChunkGranularityHeader,
                                  base::NumberToString(kDataGranularity));
        response->AddCustomHeader(kUploadUrlHeader,
                                  GetServerURL(kResumableUrl).spec());
        response->set_code(::net::HTTP_OK);
      }));

  test::TestEvent<
      StatusOr<std::pair<int64_t /*total*/, std::string /*session_token*/>>>
      init_done;
  delegate->DoInitiate(
      origin_path(),
      /*upload_parameters=*/
      base::StrCat({kUploadMedata, kUploadMetadataContentType}),
      init_done.cb());
  const auto& result = init_done.result();
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_THAT(result.value().first, Eq(static_cast<int64_t>(kTestDataSize)));
  ASSERT_THAT(result.value().second,
              StrEq(base::StrCat(
                  {origin_path(), "\n", GetServerURL(kResumableUrl).spec()})));
}

TEST_F(FileUploadDelegateTest, FailedUploadStart) {
  // Prepare the delegate.
  std::unique_ptr<FileUploadJob::Delegate> delegate =
      PrepareFileUploadDelegate();

  // Set up responses.
  EXPECT_CALL(mock_request_call_, Call(_, _))
      .WillOnce(Invoke([this](const ::net::test_server::HttpRequest& request,
                              ::net::test_server::BasicHttpResponse* response) {
        ExpectStart(request);
        response->AddCustomHeader(kUploadStatusHeader, "final");
        response->AddCustomHeader(kUploadChunkGranularityHeader,
                                  base::NumberToString(kDataGranularity));
        response->AddCustomHeader(kUploadUrlHeader,
                                  GetServerURL(kResumableUrl).spec());
        response->set_code(::net::HTTP_OK);
      }))
      .WillOnce(Invoke([this](const ::net::test_server::HttpRequest& request,
                              ::net::test_server::BasicHttpResponse* response) {
        ExpectStart(request);
        response->AddCustomHeader(kUploadStatusHeader, "active");
        response->AddCustomHeader(kUploadUrlHeader,
                                  GetServerURL(kResumableUrl).spec());
        response->set_code(::net::HTTP_OK);
      }))
      .WillOnce(Invoke([this](const ::net::test_server::HttpRequest& request,
                              ::net::test_server::BasicHttpResponse* response) {
        ExpectStart(request);
        response->AddCustomHeader(kUploadStatusHeader, "active");
        response->AddCustomHeader(kUploadChunkGranularityHeader,
                                  base::NumberToString(kDataGranularity));
        response->set_code(::net::HTTP_OK);
      }))
      .WillOnce(Invoke([this](const ::net::test_server::HttpRequest& request,
                              ::net::test_server::BasicHttpResponse* response) {
        ExpectStart(request);
        response->AddCustomHeader(kUploadStatusHeader, "active");
        response->AddCustomHeader(kUploadChunkGranularityHeader,
                                  base::NumberToString(kDataGranularity));
        response->AddCustomHeader(kUploadUrlHeader,
                                  GetServerURL(kResumableUrl).spec());
        response->set_code(::net::HTTP_INTERNAL_SERVER_ERROR);
      }));

  access_token_manager_.SetTokenValid(kTokenValid);
  {
    // Prepare access token.
    access_token_manager_.AddTokenToQueue(kTokenValid);

    test::TestEvent<
        StatusOr<std::pair<int64_t /*total*/, std::string /*session_token*/>>>
        init_done;
    // Incorrect upload parameters prevent calling the server - no expectation
    // is provided!
    delegate->DoInitiate(origin_path(),
                         /*upload_parameters=*/"ABCD", init_done.cb());
    EXPECT_THAT(
        init_done.result().error(),
        AllOf(Property(&Status::error_code, Eq(error::INVALID_ARGUMENT)),
              Property(&Status::error_message,
                       StrEq("Cannot parse upload_parameters=`ABCD`"))));
  }
  {
    // Prepare access token.
    access_token_manager_.AddTokenToQueue(kTokenValid);

    test::TestEvent<
        StatusOr<std::pair<int64_t /*total*/, std::string /*session_token*/>>>
        init_done;
    delegate->DoInitiate(
        origin_path(),
        /*upload_parameters=*/
        base::StrCat({kUploadMedata, kUploadMetadataContentType}),
        init_done.cb());
    EXPECT_THAT(init_done.result().error(),
                AllOf(Property(&Status::error_code, Eq(error::DATA_LOSS)),
                      Property(&Status::error_message,
                               StrEq("Unexpected upload status=final"))));
  }
  {
    // Prepare access token.
    access_token_manager_.AddTokenToQueue(kTokenValid);

    test::TestEvent<
        StatusOr<std::pair<int64_t /*total*/, std::string /*session_token*/>>>
        init_done;
    delegate->DoInitiate(
        origin_path(),
        /*upload_parameters=*/
        base::StrCat({kUploadMedata, kUploadMetadataContentType}),
        init_done.cb());
    EXPECT_THAT(init_done.result().error(),
                AllOf(Property(&Status::error_code, Eq(error::DATA_LOSS)),
                      Property(&Status::error_message,
                               StrEq("No granularity returned"))));
  }
  {
    // Prepare access token.
    access_token_manager_.AddTokenToQueue(kTokenValid);

    test::TestEvent<
        StatusOr<std::pair<int64_t /*total*/, std::string /*session_token*/>>>
        init_done;
    delegate->DoInitiate(
        origin_path(),
        /*upload_parameters=*/
        base::StrCat({kUploadMedata, kUploadMetadataContentType}),
        init_done.cb());
    EXPECT_THAT(init_done.result().error(),
                AllOf(Property(&Status::error_code, Eq(error::DATA_LOSS)),
                      Property(&Status::error_message,
                               StrEq("No upload URL returned"))));
  }
  {
    // Prepare access token.
    access_token_manager_.AddTokenToQueue(kTokenValid);

    test::TestEvent<
        StatusOr<std::pair<int64_t /*total*/, std::string /*session_token*/>>>
        init_done;
    delegate->DoInitiate(
        origin_path(),
        /*upload_parameters=*/
        base::StrCat({kUploadMedata, kUploadMetadataContentType}),
        init_done.cb());
    EXPECT_THAT(
        init_done.result().error(),
        AllOf(
            Property(&Status::error_code, Eq(error::DATA_LOSS)),
            Property(&Status::error_message,
                     StrEq("POST request failed with HTTP status code 500"))));
  }

  access_token_manager_.SetTokenInvalid(kTokenInvalid);
  {
    access_token_manager_.SetTokenValid(kTokenInvalid);

    test::TestEvent<
        StatusOr<std::pair<int64_t /*total*/, std::string /*session_token*/>>>
        init_done;
    delegate->DoInitiate(
        origin_path(),
        /*upload_parameters=*/
        base::StrCat({kUploadMedata, kUploadMetadataContentType}),
        init_done.cb());
    EXPECT_THAT(
        init_done.result().error(),
        AllOf(
            Property(&Status::error_code, Eq(error::UNAUTHENTICATED)),
            Property(
                &Status::error_message,
                StrEq(
                    "Service responded with error: 'Service unavailable.'"))));
  }
}

TEST_F(FileUploadDelegateTest, SuccessfulUploadStep) {
  // Prepare the delegate.
  std::unique_ptr<FileUploadJob::Delegate> delegate =
      PrepareFileUploadDelegate();

  // Set up responses: query at offset = kMaxUploadBufferSize, and make one
  // upload.
  EXPECT_CALL(mock_request_call_, Call(_, _))
      .WillOnce(Invoke([this](const ::net::test_server::HttpRequest& request,
                              ::net::test_server::BasicHttpResponse* response) {
        ExpectQuery(request);
        response->AddCustomHeader(kUploadStatusHeader, "active");
        response->AddCustomHeader(kUploadChunkGranularityHeader,
                                  base::NumberToString(kDataGranularity));
        response->AddCustomHeader(kUploadSizeReceivedHeader,
                                  base::NumberToString(kMaxUploadBufferSize));
        response->set_code(::net::HTTP_OK);
      }))
      .WillOnce(Invoke([this](const ::net::test_server::HttpRequest& request,
                              ::net::test_server::BasicHttpResponse* response) {
        ExpectStep(kMaxUploadBufferSize, request);
        response->AddCustomHeader(kUploadStatusHeader, "active");
        response->set_code(::net::HTTP_OK);
      }));

  test::TestEvent<
      StatusOr<std::pair<int64_t /*uploaded*/, std::string /*session_token*/>>>
      step_done;
  delegate->DoNextStep(
      kTestDataSize, kMaxUploadBufferSize,
      /*session_token=*/
      base::StrCat({origin_path(), "\n", GetServerURL(kResumableUrl).spec()}),
      ScopedReservation(0uL, memory_resource_), step_done.cb());
  const auto& result = step_done.result();
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_THAT(
      result.value().first,
      Eq(static_cast<int64_t>(kMaxUploadBufferSize + kMaxUploadBufferSize)));
  ASSERT_THAT(result.value().second,
              StrEq(base::StrCat(
                  {origin_path(), "\n", GetServerURL(kResumableUrl).spec()})));
}

TEST_F(FileUploadDelegateTest, SuccessfulUploadStepTillEnd) {
  // Prepare the delegate.
  std::unique_ptr<FileUploadJob::Delegate> delegate =
      PrepareFileUploadDelegate();

  // Set up responses: query at offset = (kTestDataSize - kMaxUploadBufferSize),
  // and make one upload.
  EXPECT_CALL(mock_request_call_, Call(_, _))
      .WillOnce(Invoke([this](const ::net::test_server::HttpRequest& request,
                              ::net::test_server::BasicHttpResponse* response) {
        ExpectQuery(request);
        response->AddCustomHeader(kUploadStatusHeader, "active");
        response->AddCustomHeader(kUploadChunkGranularityHeader,
                                  base::NumberToString(kDataGranularity));
        response->AddCustomHeader(
            kUploadSizeReceivedHeader,
            base::NumberToString(kTestDataSize - kMaxUploadBufferSize));
        response->set_code(::net::HTTP_OK);
      }))
      .WillOnce(Invoke([this](const ::net::test_server::HttpRequest& request,
                              ::net::test_server::BasicHttpResponse* response) {
        ExpectStep(kTestDataSize - kMaxUploadBufferSize, request);
        response->AddCustomHeader(kUploadStatusHeader, "final");
        response->set_code(::net::HTTP_OK);
      }));

  test::TestEvent<
      StatusOr<std::pair<int64_t /*uploaded*/, std::string /*session_token*/>>>
      step_done;
  delegate->DoNextStep(
      kTestDataSize, kTestDataSize - kMaxUploadBufferSize,
      /*session_token=*/
      base::StrCat({origin_path(), "\n", GetServerURL(kResumableUrl).spec()}),
      ScopedReservation(0uL, memory_resource_), step_done.cb());
  const auto& result = step_done.result();
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_THAT(result.value().first, Eq(static_cast<int64_t>(kTestDataSize)));
  ASSERT_THAT(result.value().second,
              StrEq(base::StrCat(
                  {origin_path(), "\n", GetServerURL(kResumableUrl).spec()})));
}

TEST_F(FileUploadDelegateTest, UploadStepOutOfMemory) {
  // Prepare the delegate.
  std::unique_ptr<FileUploadJob::Delegate> delegate =
      PrepareFileUploadDelegate();

  // Set up responses: query at offset = (kTestDataSize - kMaxUploadBufferSize).
  EXPECT_CALL(mock_request_call_, Call(_, _))
      .WillOnce(Invoke([this](const ::net::test_server::HttpRequest& request,
                              ::net::test_server::BasicHttpResponse* response) {
        ExpectQuery(request);
        response->AddCustomHeader(kUploadStatusHeader, "active");
        response->AddCustomHeader(kUploadChunkGranularityHeader,
                                  base::NumberToString(kDataGranularity));
        response->AddCustomHeader(
            kUploadSizeReceivedHeader,
            base::NumberToString(kTestDataSize - kMaxUploadBufferSize));
        response->set_code(::net::HTTP_OK);
      }));

  test::TestEvent<
      StatusOr<std::pair<int64_t /*uploaded*/, std::string /*session_token*/>>>
      step_done;
  ScopedReservation scoped_reservation(memory_resource_->GetTotal(),
                                       memory_resource_);
  ASSERT_TRUE(scoped_reservation.reserved());
  delegate->DoNextStep(
      kTestDataSize, kTestDataSize - kMaxUploadBufferSize,
      /*session_token=*/
      base::StrCat({origin_path(), "\n", GetServerURL(kResumableUrl).spec()}),
      std::move(scoped_reservation), step_done.cb());
  const auto& result = step_done.result();
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_THAT(result.value().first,
              Eq(static_cast<int64_t>(kTestDataSize - kMaxUploadBufferSize)));
  ASSERT_THAT(result.value().second,
              StrEq(base::StrCat(
                  {origin_path(), "\n", GetServerURL(kResumableUrl).spec()})));
}

TEST_F(FileUploadDelegateTest, UploadStepFailures) {
  // Prepare the delegate.
  std::unique_ptr<FileUploadJob::Delegate> delegate =
      PrepareFileUploadDelegate();

  // Set up responses: query at offset = (kTestDataSize - kMaxUploadBufferSize).
  EXPECT_CALL(mock_request_call_, Call(_, _))
      .WillOnce(Invoke([this](const ::net::test_server::HttpRequest& request,
                              ::net::test_server::BasicHttpResponse* response) {
        ExpectQuery(request);
        response->AddCustomHeader(kUploadStatusHeader, "unknown");
        response->set_code(::net::HTTP_OK);
      }))
      .WillOnce(Invoke([this](const ::net::test_server::HttpRequest& request,
                              ::net::test_server::BasicHttpResponse* response) {
        ExpectQuery(request);
        response->AddCustomHeader(kUploadStatusHeader, "active");
        response->AddCustomHeader(kUploadChunkGranularityHeader,
                                  base::NumberToString(kDataGranularity));
        response->set_code(::net::HTTP_OK);
      }))
      .WillOnce(Invoke([this](const ::net::test_server::HttpRequest& request,
                              ::net::test_server::BasicHttpResponse* response) {
        ExpectQuery(request);
        response->AddCustomHeader(kUploadStatusHeader, "active");
        response->AddCustomHeader(
            kUploadSizeReceivedHeader,
            base::NumberToString(kTestDataSize - kMaxUploadBufferSize));
        response->set_code(::net::HTTP_OK);
      }))
      .WillOnce(Invoke([this](const ::net::test_server::HttpRequest& request,
                              ::net::test_server::BasicHttpResponse* response) {
        ExpectQuery(request);
        response->AddCustomHeader(kUploadStatusHeader, "active");
        response->AddCustomHeader(kUploadChunkGranularityHeader,
                                  base::NumberToString(kDataGranularity));
        response->AddCustomHeader(kUploadSizeReceivedHeader, "12345Z");
        response->set_code(::net::HTTP_OK);
      }))
      .WillOnce(Invoke([this](const ::net::test_server::HttpRequest& request,
                              ::net::test_server::BasicHttpResponse* response) {
        ExpectQuery(request);
        response->AddCustomHeader(kUploadStatusHeader, "active");
        response->AddCustomHeader(kUploadChunkGranularityHeader, "12345Z");
        response->AddCustomHeader(
            kUploadSizeReceivedHeader,
            base::NumberToString(kTestDataSize - kMaxUploadBufferSize));
        response->set_code(::net::HTTP_OK);
      }))
      .WillOnce(Invoke([this](const ::net::test_server::HttpRequest& request,
                              ::net::test_server::BasicHttpResponse* response) {
        ExpectQuery(request);
        response->AddCustomHeader(kUploadStatusHeader, "active");
        response->AddCustomHeader(kUploadChunkGranularityHeader,
                                  base::NumberToString(kDataGranularity));
        response->AddCustomHeader(kUploadSizeReceivedHeader,
                                  base::NumberToString(kMaxUploadBufferSize));
        response->set_code(::net::HTTP_OK);
      }))
      .WillOnce(Invoke([this](const ::net::test_server::HttpRequest& request,
                              ::net::test_server::BasicHttpResponse* response) {
        ExpectStep(kMaxUploadBufferSize, request);
        response->AddCustomHeader(kUploadStatusHeader, "unknown");
        response->set_code(::net::HTTP_OK);
      }));

  {
    test::TestEvent<StatusOr<
        std::pair<int64_t /*uploaded*/, std::string /*session_token*/>>>
        step_done;
    delegate->DoNextStep(
        kTestDataSize, kMaxUploadBufferSize,
        /*session_token=*/
        base::StrCat({origin_path(), "\n", GetServerURL(kResumableUrl).spec()}),
        ScopedReservation(0uL, memory_resource_), step_done.cb());
    const auto& result = step_done.result();
    ASSERT_THAT(result.error(),
                AllOf(Property(&Status::error_code, Eq(error::DATA_LOSS)),
                      Property(&Status::error_message,
                               StrEq("Unexpected upload status=unknown"))));
  }
  {
    test::TestEvent<StatusOr<
        std::pair<int64_t /*uploaded*/, std::string /*session_token*/>>>
        step_done;
    delegate->DoNextStep(
        kTestDataSize, kMaxUploadBufferSize,
        /*session_token=*/
        base::StrCat({origin_path(), "\n", GetServerURL(kResumableUrl).spec()}),
        ScopedReservation(0uL, memory_resource_), step_done.cb());
    const auto& result = step_done.result();
    ASSERT_THAT(result.error(),
                AllOf(Property(&Status::error_code, Eq(error::DATA_LOSS)),
                      Property(&Status::error_message,
                               StrEq("No upload size returned"))));
  }
  {
    test::TestEvent<StatusOr<
        std::pair<int64_t /*uploaded*/, std::string /*session_token*/>>>
        step_done;
    delegate->DoNextStep(
        kTestDataSize, kMaxUploadBufferSize,
        /*session_token=*/
        base::StrCat({origin_path(), "\n", GetServerURL(kResumableUrl).spec()}),
        ScopedReservation(0uL, memory_resource_), step_done.cb());
    const auto& result = step_done.result();
    ASSERT_THAT(result.error(),
                AllOf(Property(&Status::error_code, Eq(error::DATA_LOSS)),
                      Property(&Status::error_message,
                               StrEq("No granularity returned"))));
  }
  {
    test::TestEvent<StatusOr<
        std::pair<int64_t /*uploaded*/, std::string /*session_token*/>>>
        step_done;
    delegate->DoNextStep(
        kTestDataSize, kMaxUploadBufferSize,
        /*session_token=*/
        base::StrCat({origin_path(), "\n", GetServerURL(kResumableUrl).spec()}),
        ScopedReservation(0uL, memory_resource_), step_done.cb());
    const auto& result = step_done.result();
    ASSERT_THAT(
        result.error(),
        AllOf(Property(&Status::error_code, Eq(error::DATA_LOSS)),
              Property(&Status::error_message,
                       StrEq(base::StrCat(
                           {"Unexpected received=12345Z, expected=",
                            base::NumberToString(kMaxUploadBufferSize)})))));
  }
  {
    test::TestEvent<StatusOr<
        std::pair<int64_t /*uploaded*/, std::string /*session_token*/>>>
        step_done;
    delegate->DoNextStep(
        kTestDataSize, kMaxUploadBufferSize,
        /*session_token=*/
        base::StrCat({origin_path(), "\n", GetServerURL(kResumableUrl).spec()}),
        ScopedReservation(0uL, memory_resource_), step_done.cb());
    const auto& result = step_done.result();
    ASSERT_THAT(result.error(),
                AllOf(Property(&Status::error_code, Eq(error::DATA_LOSS)),
                      Property(&Status::error_message,
                               StrEq("Unexpected granularity=12345Z"))));
  }
  {
    test::TestEvent<StatusOr<
        std::pair<int64_t /*uploaded*/, std::string /*session_token*/>>>
        step_done;
    delegate->DoNextStep(
        kTestDataSize, kMaxUploadBufferSize,
        /*session_token=*/
        base::StrCat({origin_path(), "\n", GetServerURL(kResumableUrl).spec()}),
        ScopedReservation(0uL, memory_resource_), step_done.cb());
    const auto& result = step_done.result();
    ASSERT_THAT(result.error(),
                AllOf(Property(&Status::error_code, Eq(error::DATA_LOSS)),
                      Property(&Status::error_message,
                               StrEq("Unexpected upload status=unknown"))));
  }
}

TEST_F(FileUploadDelegateTest, SuccessfulUploadFinish) {
  // Prepare the delegate.
  std::unique_ptr<FileUploadJob::Delegate> delegate =
      PrepareFileUploadDelegate();

  // Set up responses: query at offset=total, and finalize.
  EXPECT_CALL(mock_request_call_, Call(_, _))
      .WillOnce(Invoke([this](const ::net::test_server::HttpRequest& request,
                              ::net::test_server::BasicHttpResponse* response) {
        ExpectQuery(request);
        response->AddCustomHeader(kUploadStatusHeader, "active");
        response->AddCustomHeader(kUploadChunkGranularityHeader,
                                  base::NumberToString(kDataGranularity));
        response->AddCustomHeader(kUploadSizeReceivedHeader,
                                  base::NumberToString(kTestDataSize));
        response->set_code(::net::HTTP_OK);
      }))
      .WillOnce(Invoke([this](const ::net::test_server::HttpRequest& request,
                              ::net::test_server::BasicHttpResponse* response) {
        ExpectFinish(request);
        response->AddCustomHeader(kUploadStatusHeader, "final");
        response->AddCustomHeader(kUploadSizeReceivedHeader,
                                  base::NumberToString(kTestDataSize));
        response->AddCustomHeader(kUploadIdHeader, kUploadId);
        response->set_code(::net::HTTP_OK);
      }));

  test::TestEvent<StatusOr<std::string /*access_parameters*/>> finish_done;
  delegate->DoFinalize(
      /*session_token=*/base::StrCat(
          {origin_path(), "\n", GetServerURL(kResumableUrl).spec()}),
      finish_done.cb());
  const auto& result = finish_done.result();
  ASSERT_TRUE(result.has_value()) << result.error();
  ASSERT_THAT(result.value(), StrEq(base::StrCat({"Upload_id=", kUploadId})));
}

TEST_F(FileUploadDelegateTest, FinishFailures) {
  // Prepare the delegate.
  std::unique_ptr<FileUploadJob::Delegate> delegate =
      PrepareFileUploadDelegate();

  // Set up responses: query at offset=total, and finalize.
  EXPECT_CALL(mock_request_call_, Call(_, _))
      .WillOnce(Invoke([this](const ::net::test_server::HttpRequest& request,
                              ::net::test_server::BasicHttpResponse* response) {
        ExpectQuery(request);
        response->AddCustomHeader(kUploadStatusHeader, "unknown");
        response->set_code(::net::HTTP_OK);
      }))
      .WillOnce(Invoke([this](const ::net::test_server::HttpRequest& request,
                              ::net::test_server::BasicHttpResponse* response) {
        ExpectQuery(request);
        response->AddCustomHeader(kUploadStatusHeader, "active");
        response->set_code(::net::HTTP_OK);
      }))
      .WillOnce(Invoke([this](const ::net::test_server::HttpRequest& request,
                              ::net::test_server::BasicHttpResponse* response) {
        ExpectQuery(request);
        response->AddCustomHeader(kUploadStatusHeader, "active");
        response->AddCustomHeader(kUploadSizeReceivedHeader, "12345Z");
        response->set_code(::net::HTTP_OK);
      }))
      .WillOnce(Invoke([this](const ::net::test_server::HttpRequest& request,
                              ::net::test_server::BasicHttpResponse* response) {
        ExpectQuery(request);
        response->AddCustomHeader(kUploadStatusHeader, "active");
        response->AddCustomHeader(kUploadSizeReceivedHeader,
                                  base::NumberToString(kTestDataSize));
        response->set_code(::net::HTTP_OK);
      }))
      .WillOnce(Invoke([this](const ::net::test_server::HttpRequest& request,
                              ::net::test_server::BasicHttpResponse* response) {
        ExpectFinish(request);
        response->AddCustomHeader(kUploadStatusHeader, "active");
        response->set_code(::net::HTTP_OK);
      }))
      .WillOnce(Invoke([this](const ::net::test_server::HttpRequest& request,
                              ::net::test_server::BasicHttpResponse* response) {
        ExpectQuery(request);
        response->AddCustomHeader(kUploadStatusHeader, "active");
        response->AddCustomHeader(kUploadSizeReceivedHeader,
                                  base::NumberToString(kTestDataSize));
        response->set_code(::net::HTTP_OK);
      }))
      .WillOnce(Invoke([this](const ::net::test_server::HttpRequest& request,
                              ::net::test_server::BasicHttpResponse* response) {
        ExpectFinish(request);
        response->AddCustomHeader(kUploadStatusHeader, "final");
        response->set_code(::net::HTTP_OK);
      }));

  {
    test::TestEvent<StatusOr<std::string /*access_parameters*/>> finish_done;
    delegate->DoFinalize(
        /*session_token=*/base::StrCat(
            {origin_path(), "\n", GetServerURL(kResumableUrl).spec()}),
        finish_done.cb());
    const auto& result = finish_done.result();
    ASSERT_THAT(result.error(),
                AllOf(Property(&Status::error_code, Eq(error::DATA_LOSS)),
                      Property(&Status::error_message,
                               "Unexpected upload status=unknown")));
  }
  {
    test::TestEvent<StatusOr<std::string /*access_parameters*/>> finish_done;
    delegate->DoFinalize(
        /*session_token=*/base::StrCat(
            {origin_path(), "\n", GetServerURL(kResumableUrl).spec()}),
        finish_done.cb());
    const auto& result = finish_done.result();
    ASSERT_THAT(
        result.error(),
        AllOf(Property(&Status::error_code, Eq(error::DATA_LOSS)),
              Property(&Status::error_message, "No upload size returned")));
  }
  {
    test::TestEvent<StatusOr<std::string /*access_parameters*/>> finish_done;
    delegate->DoFinalize(
        /*session_token=*/base::StrCat(
            {origin_path(), "\n", GetServerURL(kResumableUrl).spec()}),
        finish_done.cb());
    const auto& result = finish_done.result();
    ASSERT_THAT(
        result.error(),
        AllOf(Property(&Status::error_code, Eq(error::DATA_LOSS)),
              Property(&Status::error_message, "Unexpected received=12345Z")));
  }
  {
    test::TestEvent<StatusOr<std::string /*access_parameters*/>> finish_done;
    delegate->DoFinalize(
        /*session_token=*/base::StrCat(
            {origin_path(), "\n", GetServerURL(kResumableUrl).spec()}),
        finish_done.cb());
    const auto& result = finish_done.result();
    ASSERT_THAT(result.error(),
                AllOf(Property(&Status::error_code, Eq(error::DATA_LOSS)),
                      Property(&Status::error_message,
                               "Unexpected upload status=active")));
  }
  {
    test::TestEvent<StatusOr<std::string /*access_parameters*/>> finish_done;
    delegate->DoFinalize(
        /*session_token=*/base::StrCat(
            {origin_path(), "\n", GetServerURL(kResumableUrl).spec()}),
        finish_done.cb());
    const auto& result = finish_done.result();
    ASSERT_THAT(
        result.error(),
        AllOf(Property(&Status::error_code, Eq(error::DATA_LOSS)),
              Property(&Status::error_message, "No upload ID returned")));
  }
}

TEST_F(FileUploadDelegateTest, DeleteFile) {
  // Prepare the delegate.
  std::unique_ptr<FileUploadJob::Delegate> delegate =
      PrepareFileUploadDelegate();

  delegate->DoDeleteFile(origin_path());
  EXPECT_FALSE(base::PathExists(base::FilePath(origin_path())));
}
}  // namespace reporting
