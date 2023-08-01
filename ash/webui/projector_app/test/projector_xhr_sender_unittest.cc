// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/projector_xhr_sender.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/projector_app/public/mojom/projector_types.mojom-forward.h"
#include "ash/webui/projector_app/public/mojom/projector_types.mojom-shared.h"
#include "ash/webui/projector_app/test/mock_app_client.h"
#include "base/functional/callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "google_apis/google_api_keys.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kTestUserEmail[] = "testuser1@gmail.com";
const char kTestUserSecondaryEmail[] = "testuser2@gmail.com";
const char kInvalidTestUserEmail[] = "testuser0@gmail.com";
const base::TimeDelta kExpiryTimeFromNow = base::Minutes(10);
constexpr char kTestDriveRequestUrl[] =
    "https://www.googleapis.com/drive/v3/files/fileID";
constexpr char kTestTranslationRequestUrl[] =
    "https://translation.googleapis.com/language/translate/v2";

GURL GetUrlWithApiKey(const GURL& url) {
  return net::AppendQueryParameter(url, "key", google_apis::GetAPIKey());
}

using SendRequestFuture =
    base::test::TestFuture<ash::projector::mojom::XhrResponsePtr>;

}  // namespace

// Used to verify the access token is removed from cache on
// net::HTTP_UNAUTHORIZED.
class MockIdentityDiagnosticsObserver
    : public signin::IdentityManager::DiagnosticsObserver {
 public:
  MOCK_METHOD2(OnAccessTokenRemovedFromCache,
               void(const CoreAccountId&, const signin::ScopeSet&));
};

namespace ash {

class ProjectorXhrSenderTest : public testing::Test {
 public:
  ProjectorXhrSenderTest() = default;
  ProjectorXhrSenderTest(const ProjectorXhrSenderTest&) = delete;
  ProjectorXhrSenderTest& operator=(const ProjectorXhrSenderTest&) = delete;
  ~ProjectorXhrSenderTest() override = default;

  // testing::Test:
  void SetUp() override {
    sender_ = std::make_unique<ProjectorXhrSender>(
        mock_app_client_.GetUrlLoaderFactory());
    mock_app_client_.AddSecondaryAccount(kTestUserSecondaryEmail);
  }

  void VerifySendRequestFuture(SendRequestFuture& future,
                               const std::string& response_body,
                               const projector::mojom::XhrResponseCode code) {
    auto& response = std::move(future.Get<0>());
    EXPECT_EQ(response_body, response->response);
    EXPECT_EQ(code, response->response_code);
  }

  void VerifySendRequestFutureWithNetworkErrorCode(
      SendRequestFuture& future,
      const std::string& response_body,
      const projector::mojom::XhrResponseCode code,
      projector::mojom::JsNetErrorCode error_code) {
    auto& response = std::move(future.Get<0>());
    EXPECT_EQ(response_body, response->response);
    EXPECT_EQ(code, response->response_code);
    EXPECT_EQ(error_code, response->net_error_code);
  }

  ProjectorXhrSender* sender() { return sender_.get(); }
  MockAppClient& mock_app_client() { return mock_app_client_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<ProjectorXhrSender> sender_;
  MockAppClient mock_app_client_;
};

TEST_F(ProjectorXhrSenderTest, Success) {
  SendRequestFuture future;

  const std::string& test_response_body = "{}";
  sender()->Send(GURL(kTestDriveRequestUrl),
                 projector::mojom::RequestType::kGet, /*request_body=*/"",
                 /*use_credentials=*/false,
                 /*use_api_key=*/false, future.GetCallback());

  mock_app_client().test_url_loader_factory().AddResponse(kTestDriveRequestUrl,
                                                          test_response_body);

  mock_app_client().GrantOAuthTokenFor(
      kTestUserEmail,
      /* expiry_time = */ base::Time::Now() + kExpiryTimeFromNow);
  VerifySendRequestFuture(future, test_response_body,
                          projector::mojom::XhrResponseCode::kSuccess);
}

TEST_F(ProjectorXhrSenderTest, TwoRequests) {
  SendRequestFuture future1;

  const std::string& test_response_body = "{}";
  sender()->Send(GURL(kTestDriveRequestUrl),
                 projector::mojom::RequestType::kGet, /*request_body=*/"",
                 /*use_credentials=*/false,
                 /*use_api_key=*/false, future1.GetCallback());

  SendRequestFuture future2;
  const std::string& test_response_body2 = "{data: {}}";
  auto translation_url = GURL(kTestTranslationRequestUrl);
  sender()->Send(translation_url, projector::mojom::RequestType::kGet,
                 /*request_body=*/"",
                 /*use_credentials=*/false,
                 /*use_api_key=*/false, future2.GetCallback());

  mock_app_client().test_url_loader_factory().AddResponse(kTestDriveRequestUrl,
                                                          test_response_body);

  mock_app_client().test_url_loader_factory().AddResponse(
      translation_url.spec(), test_response_body2);

  mock_app_client().GrantOAuthTokenFor(
      kTestUserEmail,
      /* expiry_time = */ base::Time::Now() + kExpiryTimeFromNow);
  VerifySendRequestFuture(future1, test_response_body,
                          projector::mojom::XhrResponseCode::kSuccess);
  VerifySendRequestFuture(future2, test_response_body2,
                          projector::mojom::XhrResponseCode::kSuccess);
}

TEST_F(ProjectorXhrSenderTest, UseCredentials) {
  SendRequestFuture future;

  const std::string& test_response_body = "{}";
  sender()->Send(GURL(kTestDriveRequestUrl),
                 projector::mojom::RequestType::kGet, /*request_body=*/"",
                 /*use_credentials=*/true,
                 /*use_api_key=*/false, future.GetCallback());

  mock_app_client().test_url_loader_factory().AddResponse(kTestDriveRequestUrl,
                                                          test_response_body);

  mock_app_client().GrantOAuthTokenFor(
      kTestUserEmail,
      /* expiry_time = */ base::Time::Now() + kExpiryTimeFromNow);

  VerifySendRequestFuture(future, test_response_body,
                          projector::mojom::XhrResponseCode::kSuccess);
}

TEST_F(ProjectorXhrSenderTest, UseApiKey) {
  SendRequestFuture future;

  auto url = GURL(kTestTranslationRequestUrl);
  const std::string& test_response_body = "{}";
  sender()->Send(url, projector::mojom::RequestType::kGet, /*request_body=*/"",
                 /*use_credentials=*/false,
                 /*use_api_key=*/true, future.GetCallback());

  // Verify that http request is sent with API key.
  mock_app_client().test_url_loader_factory().AddResponse(
      GetUrlWithApiKey(url).spec(), test_response_body);

  VerifySendRequestFuture(future, test_response_body,
                          projector::mojom::XhrResponseCode::kSuccess);
}

TEST_F(ProjectorXhrSenderTest, NetworkError) {
  SendRequestFuture future;

  sender()->Send(
      GURL(kTestDriveRequestUrl),
      /*method=*/projector::mojom::RequestType::kGet, /*request_body=*/"",
      /*use_credentials=*/false, /*use_api_key=*/false, future.GetCallback());

  mock_app_client().test_url_loader_factory().AddResponse(
      GURL(kTestDriveRequestUrl), network::mojom::URLResponseHead::New(),
      std::string(), network::URLLoaderCompletionStatus(net::HTTP_NOT_FOUND));

  mock_app_client().GrantOAuthTokenFor(
      kTestUserEmail,
      /* expiry_time = */ base::Time::Now() + kExpiryTimeFromNow);
  VerifySendRequestFutureWithNetworkErrorCode(
      future, "", projector::mojom::XhrResponseCode::kXhrFetchFailure,
      projector::mojom::JsNetErrorCode::kHttpError);
}

TEST_F(ProjectorXhrSenderTest, TokenFetchFailure) {
  EXPECT_CALL(mock_app_client(), HandleAccountReauth(kTestUserEmail));
  SendRequestFuture future;
  sender()->Send(
      GURL(kTestDriveRequestUrl),
      /*method=*/projector::mojom::RequestType::kGet, /*request_body=*/"",
      /*use_credentials=*/false, /*use_api_key=*/false, future.GetCallback());

  mock_app_client().MakeFetchTokenFailWithError(GoogleServiceAuthError(
      GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));
  VerifySendRequestFuture(
      future, "", projector::mojom::XhrResponseCode::kTokenFetchFailure);
}

TEST_F(ProjectorXhrSenderTest, UnauthorizedToken) {
  testing::NiceMock<MockIdentityDiagnosticsObserver> identity_observer;
  mock_app_client().GetIdentityManager()->AddDiagnosticsObserver(
      &identity_observer);
  EXPECT_CALL(identity_observer,
              OnAccessTokenRemovedFromCache(testing::_, testing::_));

  SendRequestFuture future;

  sender()->Send(
      GURL(kTestDriveRequestUrl),
      /*method=*/projector::mojom::RequestType::kGet, /*request_body=*/"",
      /*use_credentials=*/false, /*use_api_key=*/false, future.GetCallback());

  mock_app_client().test_url_loader_factory().AddResponse(
      kTestDriveRequestUrl, std::string(), net::HTTP_UNAUTHORIZED);

  mock_app_client().GrantOAuthTokenFor(
      kTestUserEmail,
      /* expiry_time = */ base::Time::Now() + kExpiryTimeFromNow);
  VerifySendRequestFuture(future, "",
                          projector::mojom::XhrResponseCode::kXhrFetchFailure);

  mock_app_client().GetIdentityManager()->RemoveDiagnosticsObserver(
      &identity_observer);
}

TEST_F(ProjectorXhrSenderTest, UnsupportedUrl) {
  SendRequestFuture future;

  sender()->Send(
      GURL("https://example.com"),
      /*method=*/projector::mojom::RequestType::kGet, /*request_body=*/"",
      /*use_credentials=*/false, /*use_api_key=*/false, future.GetCallback());
  VerifySendRequestFuture(future, "",
                          projector::mojom::XhrResponseCode::kUnsupportedURL);
}

TEST_F(ProjectorXhrSenderTest, SuccessWithPrimaryEmail) {
  SendRequestFuture future;

  const std::string& test_response_body = "{}";
  sender()->Send(GURL(kTestDriveRequestUrl),
                 projector::mojom::RequestType::kGet, /*request_body=*/"",
                 /*use_credentials=*/false,
                 /*use_api_key=*/false, future.GetCallback(),
                 base::flat_map<std::string, std::string>(), kTestUserEmail);

  mock_app_client().test_url_loader_factory().AddResponse(kTestDriveRequestUrl,
                                                          test_response_body);

  mock_app_client().GrantOAuthTokenFor(
      kTestUserEmail,
      /* expiry_time = */ base::Time::Now() + kExpiryTimeFromNow);
  VerifySendRequestFuture(future, test_response_body,
                          projector::mojom::XhrResponseCode::kSuccess);
}

TEST_F(ProjectorXhrSenderTest, InvalidAccountEmail) {
  SendRequestFuture future;

  sender()->Send(
      GURL(kTestDriveRequestUrl),
      /*method=*/projector::mojom::RequestType::kGet, /*request_body=*/"",
      /*use_credentials=*/false, /*use_api_key=*/false, future.GetCallback(),
      /*headers=*/base::flat_map<std::string, std::string>(),
      /*account_email*/ kInvalidTestUserEmail);
  VerifySendRequestFuture(
      future, "", projector::mojom::XhrResponseCode::kInvalidAccountEmail);
}

TEST_F(ProjectorXhrSenderTest, SuccessWithSecondaryEmail) {
  SendRequestFuture future;

  const std::string& test_response_body = "{}";
  sender()->Send(
      GURL(kTestDriveRequestUrl), projector::mojom::RequestType::kGet,
      /*request_body=*/"",
      /*use_credentials=*/false,
      /*use_api_key=*/false, future.GetCallback(),
      base::flat_map<std::string, std::string>(), kTestUserSecondaryEmail);

  mock_app_client().test_url_loader_factory().AddResponse(kTestDriveRequestUrl,
                                                          test_response_body);

  mock_app_client().GrantOAuthTokenFor(
      kTestUserSecondaryEmail,
      /* expiry_time = */ base::Time::Now() + kExpiryTimeFromNow);
  VerifySendRequestFuture(future, test_response_body,
                          projector::mojom::XhrResponseCode::kSuccess);
}

}  // namespace ash
