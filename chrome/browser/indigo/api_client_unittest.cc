// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/api_client.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/test_signin_client.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace indigo {

namespace {

constexpr char kTestGenerateUrl[] = "https://example.com/generate";
constexpr char kTestStatusUrl[] = "https://example.com/status";
constexpr char kTestDeleteUrl[] = "https://example.com/delete";
constexpr uint8_t kTestBytes[] = {1, 2, 3};
constexpr char kTestDataUrl[] =
    "data:image/png;base64,"
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNkYAAAAAYAAjCB0C"
    "8AAAAASUVORK5CYII=";

#if BUILDFLAG(IS_CHROMEOS)
constexpr bool kSignOutSupportedOnPlatform = false;
#else
constexpr bool kSignOutSupportedOnPlatform = true;
#endif  // BUILDFLAG(IS_CHROMEOS)

class TestSigninClientWithIndigoScope : public TestSigninClient {
 public:
  using TestSigninClient::TestSigninClient;

  signin::OAuthConsumer GetOAuthConsumerFromId(
      signin::OAuthConsumerId oauth_consumer_id) const override {
    if (oauth_consumer_id == signin::OAuthConsumerId::kIndigo) {
      return signin::OAuthConsumer(
          signin::oauth_consumer_name::kIndigoName,
          {"https://www.googleapis.com/auth/indigo.test"});
    }
    return TestSigninClient::GetOAuthConsumerFromId(oauth_consumer_id);
  }
};

class IndigoApiClientTest : public testing::Test {
 protected:
  IndigoApiClientTest()
      : test_signin_client_(std::make_unique<TestSigninClientWithIndigoScope>(
            &pref_service_,
            &test_url_loader_factory_)),
        identity_test_env_(nullptr, &pref_service_, test_signin_client_.get()),
        shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kIndigo,
        {{features::kIndigoGenerateUrl.name, kTestGenerateUrl},
         {features::kIndigoStatusUrl.name, kTestStatusUrl},
         {features::kIndigoDeleteUrl.name, kTestDeleteUrl}});
  }

  void WaitForAccessTokenRequestIfNecessaryAndRespondWithToken() {
    identity_test_env_
        .WaitForAccessTokenRequestIfNecessaryAndRespondWithTokenForConsumerId(
            "token", base::Time::Max(), signin::OAuthConsumerId::kIndigo);
  }

  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  std::unique_ptr<TestSigninClientWithIndigoScope> test_signin_client_;
  signin::IdentityTestEnvironment identity_test_env_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
};

TEST_F(IndigoApiClientTest, GenerateSuccess) {
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  ApiClient client(identity_test_env_.identity_manager(),
                   shared_url_loader_factory_);

  base::test::TestFuture<base::expected<GeneratedImage, GenerateImageError>>
      future;
  client.Generate(kTestBytes, future.GetCallback());
  WaitForAccessTokenRequestIfNecessaryAndRespondWithToken();

  // Wait for the request to be sent and verify it.
  test_url_loader_factory_.WaitForRequest(GURL(kTestGenerateUrl));
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(pending_request);
  EXPECT_EQ(pending_request->request.url, GURL(kTestGenerateUrl));

  std::string request_body = network::GetUploadData(pending_request->request);
  std::optional<base::Value> value = base::JSONReader::Read(request_body, 0);
  ASSERT_TRUE(value.has_value());
  ASSERT_TRUE(value->is_dict());
  const std::string* image_bytes_base64 =
      value->GetDict().FindString("productImageBytes");
  ASSERT_TRUE(image_bytes_base64);
  EXPECT_EQ(*image_bytes_base64, base::Base64Encode(kTestBytes));

  const std::string* output_format =
      value->GetDict().FindString("outputFormat");
  ASSERT_TRUE(output_format);
  EXPECT_EQ(*output_format, "OUTPUT_FORMAT_IMAGE_BYTES");

  // Simulate the response.
  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(),
      absl::StrFormat(R"({"result": {"generatedImageUrl": "%s"}})",
                      kTestDataUrl));

  auto result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value().image_url, GURL(kTestDataUrl));
}

TEST_F(IndigoApiClientTest, GetStatusSuccess) {
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  ApiClient client(identity_test_env_.identity_manager(),
                   shared_url_loader_factory_);

  base::test::TestFuture<base::expected<StatusResult, StatusError>> future;
  client.GetStatus(future.GetCallback());
  WaitForAccessTokenRequestIfNecessaryAndRespondWithToken();

  test_url_loader_factory_.WaitForRequest(GURL(kTestStatusUrl));
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(pending_request);
  EXPECT_EQ(pending_request->request.url, GURL(kTestStatusUrl));

  std::string request_body = network::GetUploadData(pending_request->request);
  std::optional<base::Value> request_json =
      base::JSONReader::Read(request_body, 0);
  ASSERT_TRUE(request_json.has_value());
  ASSERT_TRUE(request_json->is_dict());
  EXPECT_TRUE(request_json->GetDict()
                  .FindBool("fetchAccountEligibility")
                  .value_or(false));

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(),
      R"({"hasUserImage": true, "accountEligibleForTryOn": true})");

  auto result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.value().has_user_image);
  EXPECT_TRUE(result.value().is_service_supported_for_account);
}

TEST_F(IndigoApiClientTest, GetStatusFalse) {
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  ApiClient client(identity_test_env_.identity_manager(),
                   shared_url_loader_factory_);

  base::test::TestFuture<base::expected<StatusResult, StatusError>> future;
  client.GetStatus(future.GetCallback());
  WaitForAccessTokenRequestIfNecessaryAndRespondWithToken();

  test_url_loader_factory_.WaitForRequest(GURL(kTestStatusUrl));
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(pending_request);

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(),
      R"({"hasUserImage": false, "accountEligibleForTryOn": false})");

  auto result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result.value().has_user_image);
  EXPECT_FALSE(result.value().is_service_supported_for_account);
}

TEST_F(IndigoApiClientTest, GetStatusMixed) {
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  ApiClient client(identity_test_env_.identity_manager(),
                   shared_url_loader_factory_);

  base::test::TestFuture<base::expected<StatusResult, StatusError>> future;
  client.GetStatus(future.GetCallback());
  WaitForAccessTokenRequestIfNecessaryAndRespondWithToken();

  test_url_loader_factory_.WaitForRequest(GURL(kTestStatusUrl));
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(pending_request);

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(),
      R"({"hasUserImage": true, "accountEligibleForTryOn": false})");

  auto result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result.value().has_user_image);
  EXPECT_FALSE(result.value().is_service_supported_for_account);
}

TEST_F(IndigoApiClientTest, GetStatusHttpError) {
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  ApiClient client(identity_test_env_.identity_manager(),
                   shared_url_loader_factory_);

  test_url_loader_factory_.AddResponse(kTestStatusUrl, "",
                                       net::HTTP_INTERNAL_SERVER_ERROR);

  base::test::TestFuture<base::expected<StatusResult, StatusError>> future;
  client.GetStatus(future.GetCallback());
  WaitForAccessTokenRequestIfNecessaryAndRespondWithToken();

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().message, "HTTP error: HTTP_INTERNAL_SERVER_ERROR");
}

TEST_F(IndigoApiClientTest, GetStatusMalformedJson) {
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  ApiClient client(identity_test_env_.identity_manager(),
                   shared_url_loader_factory_);

  test_url_loader_factory_.AddResponse(kTestStatusUrl, "invalid json");

  base::test::TestFuture<base::expected<StatusResult, StatusError>> future;
  client.GetStatus(future.GetCallback());
  WaitForAccessTokenRequestIfNecessaryAndRespondWithToken();

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().message,
            "Invalid JSON response from https://example.com/status: line 1, "
            "column 1: expected value at line 1 column 1");
}

TEST_F(IndigoApiClientTest, GenerateFailure) {
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  ApiClient client(identity_test_env_.identity_manager(),
                   shared_url_loader_factory_);

  test_url_loader_factory_.AddResponse(kTestGenerateUrl, "",
                                       net::HTTP_INTERNAL_SERVER_ERROR);

  base::test::TestFuture<base::expected<GeneratedImage, GenerateImageError>>
      future;
  client.Generate(kTestBytes, future.GetCallback());
  WaitForAccessTokenRequestIfNecessaryAndRespondWithToken();

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().message, "HTTP error: HTTP_INTERNAL_SERVER_ERROR");
}

TEST_F(IndigoApiClientTest, GenerateApiError) {
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  ApiClient client(identity_test_env_.identity_manager(),
                   shared_url_loader_factory_);

  test_url_loader_factory_.AddResponse(
      kTestGenerateUrl,
      R"({"error": {"code": "INVALID_ARGUMENT", "message": "Bad bytes"}})");

  base::test::TestFuture<base::expected<GeneratedImage, GenerateImageError>>
      future;
  client.Generate(kTestBytes, future.GetCallback());
  WaitForAccessTokenRequestIfNecessaryAndRespondWithToken();

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().message,
            "API returned error: INVALID_ARGUMENT Bad bytes");
}

TEST_F(IndigoApiClientTest, GenerateInvalidJson) {
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  ApiClient client(identity_test_env_.identity_manager(),
                   shared_url_loader_factory_);

  test_url_loader_factory_.AddResponse(kTestGenerateUrl, "invalid json");

  base::test::TestFuture<base::expected<GeneratedImage, GenerateImageError>>
      future;
  client.Generate(kTestBytes, future.GetCallback());
  WaitForAccessTokenRequestIfNecessaryAndRespondWithToken();

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().message,
            "Invalid JSON response from https://example.com/generate: line 1, "
            "column 1: expected value at line 1 column 1");
}

TEST_F(IndigoApiClientTest, GenerateNotADictionary) {
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  ApiClient client(identity_test_env_.identity_manager(),
                   shared_url_loader_factory_);

  test_url_loader_factory_.AddResponse(kTestGenerateUrl, "[]");

  base::test::TestFuture<base::expected<GeneratedImage, GenerateImageError>>
      future;
  client.Generate(kTestBytes, future.GetCallback());
  WaitForAccessTokenRequestIfNecessaryAndRespondWithToken();

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().message,
            "Invalid JSON response from https://example.com/generate: not a "
            "dictionary");
}

TEST_F(IndigoApiClientTest, GenerateMissingResultOrError) {
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  ApiClient client(identity_test_env_.identity_manager(),
                   shared_url_loader_factory_);

  test_url_loader_factory_.AddResponse(kTestGenerateUrl, "{}");

  base::test::TestFuture<base::expected<GeneratedImage, GenerateImageError>>
      future;
  client.Generate(kTestBytes, future.GetCallback());
  WaitForAccessTokenRequestIfNecessaryAndRespondWithToken();

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      result.error().message,
      "Missing result or error in response from https://example.com/generate");
}

TEST_F(IndigoApiClientTest, GenerateInvalidDataUrl) {
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  ApiClient client(identity_test_env_.identity_manager(),
                   shared_url_loader_factory_);

  test_url_loader_factory_.AddResponse(
      kTestGenerateUrl,
      R"({"result": {"generatedImageUrl": "https://not-a-data-url.com"}})");

  base::test::TestFuture<base::expected<GeneratedImage, GenerateImageError>>
      future;
  client.Generate(kTestBytes, future.GetCallback());
  WaitForAccessTokenRequestIfNecessaryAndRespondWithToken();

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().message,
            "Not a data URL: https://not-a-data-url.com");
}

TEST_F(IndigoApiClientTest, GenerateUnsupportedMimeType) {
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  ApiClient client(identity_test_env_.identity_manager(),
                   shared_url_loader_factory_);

  test_url_loader_factory_.AddResponse(
      kTestGenerateUrl,
      R"({"result": {"generatedImageUrl": "data:image/xyz;base64,YWJj"}})");

  base::test::TestFuture<base::expected<GeneratedImage, GenerateImageError>>
      future;
  client.Generate(kTestBytes, future.GetCallback());
  WaitForAccessTokenRequestIfNecessaryAndRespondWithToken();

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().message, "Unsupported image MIME type: image/xyz");
}

TEST_F(IndigoApiClientTest, GenerateNoUser) {
  ApiClient client(identity_test_env_.identity_manager(),
                   shared_url_loader_factory_);

  base::test::TestFuture<base::expected<GeneratedImage, GenerateImageError>>
      future;
  client.Generate(kTestBytes, future.GetCallback());

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().message, "No signed in user");
}

TEST_F(IndigoApiClientTest, GenerateAccountChange) {
  if constexpr (!kSignOutSupportedOnPlatform) {
    GTEST_SKIP() << "Sign out is not supported on this platform.";
  }

  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  ApiClient client(identity_test_env_.identity_manager(),
                   shared_url_loader_factory_);

  // Change account.
  identity_test_env_.ClearPrimaryAccount();
  identity_test_env_.MakePrimaryAccountAvailable("new@example.com",
                                                 signin::ConsentLevel::kSignin);

  test_url_loader_factory_.AddResponse(
      kTestGenerateUrl,
      absl::StrFormat(R"({"result": {"generatedImageUrl": "%s"}})",
                      kTestDataUrl));

  base::test::TestFuture<base::expected<GeneratedImage, GenerateImageError>>
      future;
  client.Generate(kTestBytes, future.GetCallback());
  WaitForAccessTokenRequestIfNecessaryAndRespondWithToken();

  auto result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value().image_url, GURL(kTestDataUrl));
}

TEST_F(IndigoApiClientTest, GenerateSignOutDuringTokenRequest) {
  if constexpr (!kSignOutSupportedOnPlatform) {
    GTEST_SKIP() << "Sign out is not supported on this platform.";
  }

  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  ApiClient client(identity_test_env_.identity_manager(),
                   shared_url_loader_factory_);

  base::test::TestFuture<base::expected<GeneratedImage, GenerateImageError>>
      future;
  client.Generate(kTestBytes, future.GetCallback());

  ASSERT_FALSE(future.IsReady());

  // Sign out.
  identity_test_env_.ClearPrimaryAccount();

  // The request should fail because of sign-out.
  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().message, "Premature failure: HTTP_UNAUTHORIZED");
}

TEST_F(IndigoApiClientTest, GenerateAuthError) {
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  ApiClient client(identity_test_env_.identity_manager(),
                   shared_url_loader_factory_);

  base::test::TestFuture<base::expected<GeneratedImage, GenerateImageError>>
      future;
  client.Generate(kTestBytes, future.GetCallback());

  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
      GoogleServiceAuthError::FromServiceUnavailable(""));

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().message, "Premature failure: HTTP_FORBIDDEN");
}

TEST_F(IndigoApiClientTest, GenerateSignOutDuringMainRequest) {
  if constexpr (!kSignOutSupportedOnPlatform) {
    GTEST_SKIP() << "Sign out is not supported on this platform.";
  }

  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  ApiClient client(identity_test_env_.identity_manager(),
                   shared_url_loader_factory_);

  base::test::TestFuture<base::expected<GeneratedImage, GenerateImageError>>
      future;
  client.Generate(kTestBytes, future.GetCallback());
  WaitForAccessTokenRequestIfNecessaryAndRespondWithToken();

  // Wait for the main request to be sent.
  test_url_loader_factory_.WaitForRequest(GURL(kTestGenerateUrl));
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  EXPECT_EQ(pending_request->request.url, GURL(kTestGenerateUrl));

  ASSERT_FALSE(future.IsReady());

  // Sign out now, while the main request is in flight.
  identity_test_env_.ClearPrimaryAccount();

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().message, "Request cancelled");
}

TEST_F(IndigoApiClientTest, GenerateCancel) {
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  ApiClient client(identity_test_env_.identity_manager(),
                   shared_url_loader_factory_);

  base::test::TestFuture<base::expected<GeneratedImage, GenerateImageError>>
      future;
  base::OnceClosure cancel_closure =
      client.Generate(kTestBytes, future.GetCallback());
  WaitForAccessTokenRequestIfNecessaryAndRespondWithToken();

  // Cancel the request.
  std::move(cancel_closure).Run();

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().message, "Premature failure: CANCELLED");
}

TEST_F(IndigoApiClientTest, GenerateImageTooLarge) {
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  ApiClient client(identity_test_env_.identity_manager(),
                   shared_url_loader_factory_);

  std::vector<uint8_t> large_bytes(4 * 1024 * 1024 + 1, 0);

  base::test::TestFuture<base::expected<GeneratedImage, GenerateImageError>>
      future;
  client.Generate(large_bytes, future.GetCallback());

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().message, "Product image is too large (> 4MB)");
}

TEST_F(IndigoApiClientTest, DeleteSuccess) {
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  ApiClient client(identity_test_env_.identity_manager(),
                   shared_url_loader_factory_);

  base::test::TestFuture<base::expected<void, DeleteError>> future;
  client.Delete(future.GetCallback());
  WaitForAccessTokenRequestIfNecessaryAndRespondWithToken();

  test_url_loader_factory_.WaitForRequest(GURL(kTestDeleteUrl));
  auto* pending_request = test_url_loader_factory_.GetPendingRequest(0);
  ASSERT_TRUE(pending_request);
  EXPECT_EQ(pending_request->request.url, GURL(kTestDeleteUrl));

  std::string request_body = network::GetUploadData(pending_request->request);
  EXPECT_EQ(request_body, "{}");

  test_url_loader_factory_.SimulateResponseForPendingRequest(
      pending_request->request.url.spec(), "{}");

  auto result = future.Get();
  EXPECT_TRUE(result.has_value());
}

TEST_F(IndigoApiClientTest, DeleteHttpError) {
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  ApiClient client(identity_test_env_.identity_manager(),
                   shared_url_loader_factory_);

  test_url_loader_factory_.AddResponse(kTestDeleteUrl, "",
                                       net::HTTP_INTERNAL_SERVER_ERROR);

  base::test::TestFuture<base::expected<void, DeleteError>> future;
  client.Delete(future.GetCallback());
  WaitForAccessTokenRequestIfNecessaryAndRespondWithToken();

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().message, "HTTP error: HTTP_INTERNAL_SERVER_ERROR");
}

TEST_F(IndigoApiClientTest, DeleteMalformedJson) {
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  ApiClient client(identity_test_env_.identity_manager(),
                   shared_url_loader_factory_);

  test_url_loader_factory_.AddResponse(kTestDeleteUrl, "invalid json");

  base::test::TestFuture<base::expected<void, DeleteError>> future;
  client.Delete(future.GetCallback());
  WaitForAccessTokenRequestIfNecessaryAndRespondWithToken();

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().message,
            "Invalid JSON response from https://example.com/delete: line 1, "
            "column 1: expected value at line 1 column 1");
}

TEST_F(IndigoApiClientTest, DeleteNotADictionary) {
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  ApiClient client(identity_test_env_.identity_manager(),
                   shared_url_loader_factory_);

  test_url_loader_factory_.AddResponse(kTestDeleteUrl, "[]");

  base::test::TestFuture<base::expected<void, DeleteError>> future;
  client.Delete(future.GetCallback());
  WaitForAccessTokenRequestIfNecessaryAndRespondWithToken();

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().message,
            "Invalid JSON response from https://example.com/delete: not a "
            "dictionary");
}

TEST_F(IndigoApiClientTest, DeleteUnexpectedNonEmptyResponse) {
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  ApiClient client(identity_test_env_.identity_manager(),
                   shared_url_loader_factory_);

  test_url_loader_factory_.AddResponse(kTestDeleteUrl, R"({"foo": "bar"})");

  base::test::TestFuture<base::expected<void, DeleteError>> future;
  client.Delete(future.GetCallback());
  WaitForAccessTokenRequestIfNecessaryAndRespondWithToken();

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      result.error().message,
      "Unexpected non-empty JSON response from https://example.com/delete");
}

TEST_F(IndigoApiClientTest, DeleteApiError) {
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  ApiClient client(identity_test_env_.identity_manager(),
                   shared_url_loader_factory_);

  test_url_loader_factory_.AddResponse(
      kTestDeleteUrl, R"({"error": {"message": "Resource not found"}})");

  base::test::TestFuture<base::expected<void, DeleteError>> future;
  client.Delete(future.GetCallback());
  WaitForAccessTokenRequestIfNecessaryAndRespondWithToken();

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().message, "Resource not found");
}

TEST_F(IndigoApiClientTest, DeleteNoUser) {
  ApiClient client(identity_test_env_.identity_manager(),
                   shared_url_loader_factory_);

  base::test::TestFuture<base::expected<void, DeleteError>> future;
  client.Delete(future.GetCallback());

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().message, "No signed in user");
}

TEST_F(IndigoApiClientTest, DeleteEmptyResponse) {
  identity_test_env_.MakePrimaryAccountAvailable("test@example.com",
                                                 signin::ConsentLevel::kSignin);

  ApiClient client(identity_test_env_.identity_manager(),
                   shared_url_loader_factory_);

  test_url_loader_factory_.AddResponse(kTestDeleteUrl, "");

  base::test::TestFuture<base::expected<void, DeleteError>> future;
  client.Delete(future.GetCallback());
  WaitForAccessTokenRequestIfNecessaryAndRespondWithToken();

  auto result = future.Get();
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().message,
            "Unexpected empty response from https://example.com/delete");
}

}  // namespace
}  // namespace indigo
