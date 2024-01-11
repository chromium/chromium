// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/uploading/upload_job.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <utility>

#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/uploading/upload_job_impl.h"
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
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

constexpr char kUploadPath[] = "/upload";
constexpr char kRobotAccountId[] = "robot@gserviceaccount.com";
constexpr char kCustomField1[] = "customfield1";
constexpr char kCustomField2[] = "customfield2";
constexpr char kTestPayload1[] = "**||--||PAYLOAD1||--||**";
constexpr char kTestPayload2[] = "**||--||PAYLOAD2||--||**";
constexpr char kTokenExpired[] = "EXPIRED_TOKEN";
constexpr char kTokenInvalid[] = "INVALID_TOKEN";
constexpr char kTokenValid[] = "VALID_TOKEN";

class RepeatingMimeBoundaryGenerator
    : public UploadJobImpl::MimeBoundaryGenerator {
 public:
  explicit RepeatingMimeBoundaryGenerator(char character)
      : character_(character) {}

  RepeatingMimeBoundaryGenerator(const RepeatingMimeBoundaryGenerator&) =
      delete;
  RepeatingMimeBoundaryGenerator& operator=(
      const RepeatingMimeBoundaryGenerator&) = delete;

  ~RepeatingMimeBoundaryGenerator() override {}

  // MimeBoundaryGenerator:
  std::string GenerateBoundary() const override {
    const int kMimeBoundarySize = 32;
    return std::string(kMimeBoundarySize, character_);
  }

 private:
  const char character_;
};

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
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
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
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
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
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      OAuth2AccessTokenConsumer* consumer,
      const std::string& token_binding_challenge) override {
    EXPECT_EQ(CoreAccountId::FromRobotEmail(kRobotAccountId), account_id);
    return GaiaAccessTokenFetcher::
        CreateExchangeRefreshTokenForAccessTokenInstance(
            consumer, url_loader_factory, "fake_refresh_token");
  }

  bool HasRefreshToken(const CoreAccountId& account_id) const override {
    return CoreAccountId::FromRobotEmail(kRobotAccountId) == account_id;
  }
};

}  // namespace

class UploadJobTestBase : public testing::Test, public UploadJob::Delegate {
 public:
  UploadJobTestBase()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        access_token_manager_(&token_manager_delegate_) {}

  // UploadJob::Delegate:
  void OnSuccess() override {
    if (!expected_error_)
      run_loop_.Quit();
    else
      FAIL();
  }

  // UploadJob::Delegate:
  void OnFailure(UploadJob::ErrorCode error_code) override {
    if (expected_error_ && *expected_error_.get() == error_code)
      run_loop_.Quit();
    else
      FAIL();
  }

  const GURL GetServerURL() const { return test_server_.GetURL(kUploadPath); }

  void SetExpectedError(std::unique_ptr<UploadJob::ErrorCode> expected_error) {
    expected_error_ = std::move(expected_error);
  }

  // testing::Test:
  void SetUp() override {
    url_loader_factory_ =
        base::MakeRefCounted<network::TestSharedURLLoaderFactory>();
    ASSERT_TRUE(test_server_.Start());
    // Set retry delay to prevent timeouts
    UploadJobImpl::SetRetryDelayForTesting(0);
  }

  // testing::Test:
  void TearDown() override {
    ASSERT_TRUE(test_server_.ShutdownAndWaitUntilComplete());
  }

 protected:
  std::unique_ptr<UploadJob> PrepareUploadJob(
      std::unique_ptr<UploadJobImpl::MimeBoundaryGenerator>
          mime_boundary_generator) {
    std::unique_ptr<UploadJob> upload_job(new UploadJobImpl(
        GetServerURL(), CoreAccountId::FromRobotEmail(kRobotAccountId),
        &access_token_manager_, url_loader_factory_, this,
        std::move(mime_boundary_generator), TRAFFIC_ANNOTATION_FOR_TESTS,
        base::SingleThreadTaskRunner::GetCurrentDefault()));

    std::map<std::string, std::string> header_entries;
    header_entries.insert(std::make_pair(kCustomField1, "CUSTOM1"));
    std::unique_ptr<std::string> data(new std::string(kTestPayload1));
    upload_job->AddDataSegment("Name1", "file1.ext", header_entries,
                               std::move(data));

    header_entries.insert(std::make_pair(kCustomField2, "CUSTOM2"));
    std::unique_ptr<std::string> data2(new std::string(kTestPayload2));
    upload_job->AddDataSegment("Name2", "", header_entries, std::move(data2));
    return upload_job;
  }

  content::BrowserTaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  net::EmbeddedTestServer test_server_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  FakeOAuth2AccessTokenManagerDelegate token_manager_delegate_;
  FakeOAuth2AccessTokenManagerWithCaching access_token_manager_;

  std::unique_ptr<UploadJob::ErrorCode> expected_error_;
};

class UploadFlowTest : public UploadJobTestBase {
 public:
  UploadFlowTest() {}

  // UploadJobTestBase:
  void SetUp() override {
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &UploadFlowTest::HandlePostRequest, base::Unretained(this)));
    UploadJobTestBase::SetUp();
    upload_attempt_count_ = 0;
  }

  // Sets the response code which will be returned when no other problems occur.
  // Default is |net::HTTP_OK|
  void SetResponseDefaultStatusCode(net::HttpStatusCode code) {
    default_status_code_ = code;
  }

  std::unique_ptr<net::test_server::HttpResponse> HandlePostRequest(
      const net::test_server::HttpRequest& request) {
    upload_attempt_count_++;
    EXPECT_TRUE(request.headers.find("Authorization") != request.headers.end());
    const std::string authorization_header =
        request.headers.at("Authorization");
    std::unique_ptr<net::test_server::BasicHttpResponse> response(
        new net::test_server::BasicHttpResponse);
    const size_t pos = authorization_header.find(" ");
    if (pos == std::string::npos) {
      response->set_code(net::HTTP_UNAUTHORIZED);
      return std::move(response);
    }

    const std::string token = authorization_header.substr(pos + 1);
    response->set_code(access_token_manager_.IsTokenValid(token)
                           ? default_status_code_
                           : net::HTTP_UNAUTHORIZED);
    return std::move(response);
  }

 protected:
  int upload_attempt_count_;
  net::HttpStatusCode default_status_code_ = net::HTTP_OK;
};

TEST_F(UploadFlowTest, SuccessfulUpload) {
  access_token_manager_.SetTokenValid(kTokenValid);
  access_token_manager_.AddTokenToQueue(kTokenValid);
  std::unique_ptr<UploadJob> upload_job = PrepareUploadJob(
      base::WrapUnique(new UploadJobImpl::RandomMimeBoundaryGenerator));
  upload_job->Start();
  run_loop_.Run();
  ASSERT_EQ(1, upload_attempt_count_);
}

TEST_F(UploadFlowTest, TokenExpired) {
  access_token_manager_.SetTokenValid(kTokenValid);
  access_token_manager_.AddTokenToQueue(kTokenExpired);
  access_token_manager_.AddTokenToQueue(kTokenValid);
  std::unique_ptr<UploadJob> upload_job = PrepareUploadJob(
      base::WrapUnique(new UploadJobImpl::RandomMimeBoundaryGenerator));
  upload_job->Start();
  run_loop_.Run();
  ASSERT_EQ(2, upload_attempt_count_);
}

TEST_F(UploadFlowTest, TokenInvalid) {
  access_token_manager_.AddTokenToQueue(kTokenInvalid);
  access_token_manager_.AddTokenToQueue(kTokenInvalid);
  access_token_manager_.AddTokenToQueue(kTokenInvalid);
  access_token_manager_.AddTokenToQueue(kTokenInvalid);
  SetExpectedError(
      std::make_unique<UploadJob::ErrorCode>(UploadJob::AUTHENTICATION_ERROR));

  std::unique_ptr<UploadJob> upload_job = PrepareUploadJob(
      base::WrapUnique(new UploadJobImpl::RandomMimeBoundaryGenerator));
  upload_job->Start();
  run_loop_.Run();
  ASSERT_EQ(4, upload_attempt_count_);
}

TEST_F(UploadFlowTest, TokenMultipleTries) {
  access_token_manager_.SetTokenValid(kTokenValid);
  access_token_manager_.AddTokenToQueue(kTokenInvalid);
  access_token_manager_.AddTokenToQueue(kTokenInvalid);
  access_token_manager_.AddTokenToQueue(kTokenValid);

  std::unique_ptr<UploadJob> upload_job = PrepareUploadJob(
      base::WrapUnique(new UploadJobImpl::RandomMimeBoundaryGenerator));
  upload_job->Start();
  run_loop_.Run();
  ASSERT_EQ(3, upload_attempt_count_);
}

TEST_F(UploadFlowTest, TokenFetchFailure) {
  SetExpectedError(
      std::make_unique<UploadJob::ErrorCode>(UploadJob::AUTHENTICATION_ERROR));

  std::unique_ptr<UploadJob> upload_job = PrepareUploadJob(
      base::WrapUnique(new UploadJobImpl::RandomMimeBoundaryGenerator));
  upload_job->Start();
  run_loop_.Run();
  // Without a token we don't try to upload
  ASSERT_EQ(0, upload_attempt_count_);
}

TEST_F(UploadFlowTest, InternalServerError) {
  SetResponseDefaultStatusCode(net::HTTP_INTERNAL_SERVER_ERROR);
  access_token_manager_.SetTokenValid(kTokenValid);
  access_token_manager_.AddTokenToQueue(kTokenValid);

  SetExpectedError(
      std::make_unique<UploadJob::ErrorCode>(UploadJob::SERVER_ERROR));

  std::unique_ptr<UploadJob> upload_job = PrepareUploadJob(
      base::WrapUnique(new UploadJobImpl::RandomMimeBoundaryGenerator));
  upload_job->Start();
  run_loop_.Run();
  // kMaxAttempts
  ASSERT_EQ(4, upload_attempt_count_);
}

class UploadRequestTest : public UploadJobTestBase {
 public:
  UploadRequestTest() {}

  // UploadJobTestBase:
  void SetUp() override {
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &UploadRequestTest::HandlePostRequest, base::Unretained(this)));
    UploadJobTestBase::SetUp();
  }

  std::unique_ptr<net::test_server::HttpResponse> HandlePostRequest(
      const net::test_server::HttpRequest& request) {
    std::unique_ptr<net::test_server::BasicHttpResponse> response(
        new net::test_server::BasicHttpResponse);
    response->set_code(net::HTTP_OK);
    EXPECT_EQ(expected_content_, request.content);
    return std::move(response);
  }

  void SetExpectedRequestContent(const std::string& expected_content) {
    expected_content_ = expected_content;
  }

 protected:
  std::string expected_content_;
};

TEST_F(UploadRequestTest, TestRequestStructure) {
  access_token_manager_.SetTokenValid(kTokenValid);
  access_token_manager_.AddTokenToQueue(kTokenValid);
  std::unique_ptr<UploadJob> upload_job =
      PrepareUploadJob(std::make_unique<RepeatingMimeBoundaryGenerator>('A'));
  SetExpectedRequestContent(
      "--AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\r\n"
      "Content-Disposition: form-data; "
      "name=\"Name1\"; filename=\"file1.ext\"\r\n"
      "customfield1: CUSTOM1\r\n"
      "\r\n"
      "**||--||PAYLOAD1||--||**\r\n"
      "--AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\r\n"
      "Content-Disposition: form-data; name=\"Name2\"\r\n"
      "customfield1: CUSTOM1\r\n"
      "customfield2: CUSTOM2\r\n"
      "\r\n"
      "**||--||PAYLOAD2||--||**\r\n--"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA--\r\n");

  upload_job->Start();
  run_loop_.Run();
}

}  // namespace policy
