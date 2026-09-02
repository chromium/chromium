// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/private_verification_tokens/private_verification_tokens_service.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "chrome/browser/private_verification_tokens/private_verification_tokens_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/private_verification_tokens/common/athm_ffi/athm_ffi.h"
#include "components/private_verification_tokens/common/private_verification_tokens_issuer_config.h"
#include "components/private_verification_tokens/common/private_verification_tokens_test_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/crubit/support/rs_std/slice_ref.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

using ::private_verification_tokens::test::CreateTestIssuer;

class PrivateVerificationTokensServiceBrowserTest : public PlatformBrowserTest {
 public:
  PrivateVerificationTokensServiceBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kEnablePrivateVerificationTokens);
  }

  Profile* GetProfile() { return chrome_test_utils::GetProfile(this); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivateVerificationTokensServiceBrowserTest,
                       GetForProfile_FeatureEnabled_ReturnsInstance) {
  PrivateVerificationTokensService* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(GetProfile());
  EXPECT_TRUE(service);
}

class PrivateVerificationTokensServiceDisabledBrowserTest
    : public PlatformBrowserTest {
 public:
  PrivateVerificationTokensServiceDisabledBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(
        net::features::kEnablePrivateVerificationTokens);
  }

  Profile* GetProfile() { return chrome_test_utils::GetProfile(this); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivateVerificationTokensServiceDisabledBrowserTest,
                       GetForProfile_FeatureDisabled_ReturnsNull) {
  PrivateVerificationTokensService* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(GetProfile());
  EXPECT_FALSE(service);
}

class PrivateVerificationTokensServiceCustomIssuerBrowserTest
    : public PlatformBrowserTest {
 public:
  PrivateVerificationTokensServiceCustomIssuerBrowserTest() {
    const std::vector<uint8_t> serialized_public_key = {1, 2, 3, 4};
    const std::string encoded_public_key =
        base::Base64Encode(serialized_public_key);
    const std::vector<uint8_t> serialized_proof = {5, 6, 7};
    const std::string encoded_proof = base::Base64Encode(serialized_proof);
    const std::string custom_issuer_json = base::StringPrintf(
        R"({
          "issuerRequestUrl": "https://commandline.example.com/issue",
          "version": 1,
          "publicKey": "%s",
          "publicKeyProof": "%s",
          "batchSize": 7,
          "expiration": "2147483647",
          "redeemers": ["https://s1.commandline.example.com"],
          "deploymentId": "cmd-deployment-id"
        })",
        encoded_public_key.c_str(), encoded_proof.c_str());

    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        net::features::kEnablePrivateVerificationTokens,
        {{net::features::kPrivateVerificationTokensCustomIssuer.name,
          custom_issuer_json}});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivateVerificationTokensServiceCustomIssuerBrowserTest,
                       GlobalIssuerConfig_CustomIssuerFromCommandLine) {
  PrivateVerificationTokensServiceFactory::SetGlobalIssuerConfig(nullptr);
  auto config =
      PrivateVerificationTokensServiceFactory::GetGlobalIssuerConfig();
  ASSERT_TRUE(config);
  const url::Origin expected_origin =
      url::Origin::Create(GURL("https://commandline.example.com"));
  EXPECT_TRUE(config->config().contains(expected_origin));

  const auto& issuer_config = config->config().at(expected_origin);
  EXPECT_EQ(issuer_config.issuer_request_url,
            GURL("https://commandline.example.com/issue"));
  EXPECT_EQ(issuer_config.batch_size, 7);
  EXPECT_EQ(issuer_config.deployment_id, "cmd-deployment-id");
  EXPECT_THAT(issuer_config.redeemers,
              testing::ElementsAre(url::Origin::Create(
                  GURL("https://s1.commandline.example.com"))));

  const private_verification_tokens::PrivateVerificationTokensPublicKey
      expected_public_key{expected_origin,
                          {1, 2, 3, 4},
                          {5, 6, 7},
                          base::Time::UnixEpoch() + base::Seconds(2147483647),
                          1};
  EXPECT_EQ(issuer_config.public_key, expected_public_key);
}

class PrivateVerificationTokensEndToEndBrowserTest
    : public PlatformBrowserTest {
 public:
  PrivateVerificationTokensEndToEndBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kEnablePrivateVerificationTokens);
  }

  void SetUpOnMainThread() override {
    PlatformBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &PrivateVerificationTokensEndToEndBrowserTest::HandleRequest,
        base::Unretained(this)));
    ASSERT_TRUE(https_server_.Start());
  }

  Profile* GetProfile() { return chrome_test_utils::GetProfile(this); }
  net::EmbeddedTestServer& https_server() { return https_server_; }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    auto token_it = request.headers.find(
        net::HttpRequestHeaders::kSecPrivateVerificationToken);
    if (token_it != request.headers.end()) {
      last_redeemed_token_header_ = token_it->second;
      std::optional<std::vector<uint8_t>> decoded =
          base::Base64Decode(token_it->second);
      if (decoded.has_value()) {
        redemption_count_++;
        auto metadata = test_issuer_->verify_wire_token(
            rs_std::SliceRef<const uint8_t>(*decoded));
        last_recovered_metadata_ =
            metadata.has_value() ? std::make_optional(*metadata) : std::nullopt;
      }
    }

    if (base::StartsWith(request.relative_url, "/server-redirect?")) {
      std::string destination = request.relative_url.substr(
          std::string_view("/server-redirect?").length());
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_FOUND);
      response->AddCustomHeader("Location", destination);
      response->set_content("<html><body>Redirecting...</body></html>");
      response->set_content_type("text/html");
      return response;
    }

    if (request.relative_url == "/issue") {
      issuance_count_++;
      auto content_type_it =
          request.headers.find(net::HttpRequestHeaders::kContentType);
      if (content_type_it != request.headers.end()) {
        last_issuance_request_content_type_ = content_type_it->second;
      }
      auto accept_it = request.headers.find(net::HttpRequestHeaders::kAccept);
      if (accept_it != request.headers.end()) {
        last_issuance_request_accept_ = accept_it->second;
      }
      auto batch_response = test_issuer_->issue_batch_from_bytes(
          rs_std::SliceRef<const uint8_t>(base::as_byte_span(request.content)),
          /*hidden_metadata=*/0);
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      if (batch_response.has_value()) {
        response->set_content(
            std::string(batch_response->begin(), batch_response->end()));
      }
      response->set_content_type("application/private-token-response");
      return response;
    }

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content("<html><body>Home</body></html>");
    response->set_content_type("text/html");
    return response;
  }

  void WaitForInitialization(PrivateVerificationTokensService* target_service) {
    if (target_service->is_initialized()) {
      return;
    }
    base::test::TestFuture<void> init_future;
    class Waiter : public PrivateVerificationTokensService::Observer {
     public:
      explicit Waiter(base::OnceClosure callback)
          : callback_(std::move(callback)) {}
      void OnInitializationComplete() override { std::move(callback_).Run(); }

     private:
      base::OnceClosure callback_;
    };
    Waiter waiter(init_future.GetCallback());
    base::ScopedObservation<PrivateVerificationTokensService,
                            PrivateVerificationTokensService::Observer>
        observation(&waiter);
    observation.Observe(target_service);
    EXPECT_TRUE(init_future.Wait());
  }

 protected:
  int issuance_count_ = 0;
  int redemption_count_ = 0;
  std::optional<std::string> last_redeemed_token_header_;
  std::optional<uint8_t> last_recovered_metadata_;
  std::optional<std::string> last_issuance_request_content_type_;
  std::optional<std::string> last_issuance_request_accept_;
  std::optional<private_verification_tokens::PrivacyPassAthmIssuer>
      test_issuer_ = CreateTestIssuer(/*num_buckets=*/2, "test-deployment");

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_;
};

IN_PROC_BROWSER_TEST_F(PrivateVerificationTokensEndToEndBrowserTest,
                       IssuanceAndIncognitoRedemption_CrossSubdomainSameETLD) {
  auto* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(GetProfile());
  ASSERT_TRUE(service);

  WaitForInitialization(service);

  const GURL issuer_url = https_server().GetURL("issuer.a.test", "/issue");
  const GURL redemption_url =
      https_server().GetURL("redeemer.a.test", "/some/dummy/path");
  const GURL issuer_home_url = https_server().GetURL("issuer.a.test", "/home");

  const url::Origin issuer_origin = url::Origin::Create(issuer_url);
  const url::Origin redeemer_origin = url::Origin::Create(redemption_url);

  const std::string encoded_public_key =
      base::Base64Encode(test_issuer_->public_key_bytes());
  const std::string encoded_public_key_proof =
      base::Base64Encode(test_issuer_->public_key_proof_bytes());
  const std::string json_str = base::StringPrintf(
      R"({
    "issuers": [
      {
        "issuerRequestUrl": "%s",
        "version": 1,
        "publicKey": "%s",
        "publicKeyProof": "%s",
        "batchSize": 2,
        "expiration": "2147483647",
        "redeemers": ["%s"],
        "deploymentId": "test-deployment"
      }
    ]
  })",
      issuer_url.spec().c_str(), encoded_public_key.c_str(),
      encoded_public_key_proof.c_str(), redeemer_origin.Serialize().c_str());

  auto config =
      private_verification_tokens::PrivateVerificationTokensIssuerConfig::
          Create(base::test::ParseJsonDict(json_str));
  ASSERT_TRUE(config);
  service->SetIssuerConfig(config);

  // 1. Issuance Phase: Navigate regular browser to issuer origin.
  {
    base::test::TestFuture<void> tokens_stored_future;
    class StoredWaiter : public PrivateVerificationTokensService::Observer {
     public:
      explicit StoredWaiter(base::OnceClosure callback)
          : callback_(std::move(callback)) {}
      void OnTokensStored() override { std::move(callback_).Run(); }

     private:
      base::OnceClosure callback_;
    };
    StoredWaiter stored_waiter(tokens_stored_future.GetCallback());
    base::ScopedObservation<PrivateVerificationTokensService,
                            PrivateVerificationTokensService::Observer>
        observation(&stored_waiter);
    observation.Observe(service);

    ASSERT_TRUE(content::NavigateToURL(
        chrome_test_utils::GetActiveWebContents(this), issuer_home_url));
    EXPECT_TRUE(tokens_stored_future.Wait());
  }

  EXPECT_GE(issuance_count_, 1);
  EXPECT_EQ(last_issuance_request_content_type_,
            "application/private-token-request");
  EXPECT_EQ(last_issuance_request_accept_,
            "application/private-token-response");

  // Verify token is stored in the database.
  base::test::TestFuture<std::vector<url::Origin>> issuers_future;
  service->GetTokenIssuers(issuers_future.GetCallback());
  EXPECT_THAT(issuers_future.Take(), testing::ElementsAre(issuer_origin));

  // 2. Redemption Phase: Open Incognito WebContents and navigate to
  // redemption_url.
  Profile* incognito_profile =
      GetProfile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_TRUE(incognito_profile);

  std::unique_ptr<content::WebContents> incognito_web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(incognito_profile));
  ASSERT_TRUE(incognito_web_contents);

  ASSERT_TRUE(
      content::NavigateToURL(incognito_web_contents.get(), redemption_url));

  EXPECT_EQ(redemption_count_, 1);
  ASSERT_TRUE(last_redeemed_token_header_.has_value());
  EXPECT_TRUE(last_recovered_metadata_.has_value());
  EXPECT_EQ(last_recovered_metadata_.value(), 0);
}

IN_PROC_BROWSER_TEST_F(PrivateVerificationTokensEndToEndBrowserTest,
                       Redemption_ServerRedirect_TokenSentOnFirstHopOnly) {
  auto* service =
      PrivateVerificationTokensServiceFactory::GetForProfile(GetProfile());
  ASSERT_TRUE(service);

  WaitForInitialization(service);

  const GURL issuer_url = https_server().GetURL("issuer.a.test", "/issue");
  const GURL issuer_home_url = https_server().GetURL("issuer.a.test", "/home");
  const GURL destination_url =
      https_server().GetURL("destination.b.test", "/final");
  const GURL redirect_url = https_server().GetURL(
      "redeemer.a.test", base::StringPrintf("/server-redirect?%s",
                                            destination_url.spec().c_str()));

  const url::Origin issuer_origin = url::Origin::Create(issuer_url);
  const url::Origin redeemer_origin = url::Origin::Create(redirect_url);

  const std::string encoded_public_key =
      base::Base64Encode(test_issuer_->public_key_bytes());
  const std::string encoded_public_key_proof =
      base::Base64Encode(test_issuer_->public_key_proof_bytes());
  const std::string json_str = base::StringPrintf(
      R"({
    "issuers": [
      {
        "issuerRequestUrl": "%s",
        "version": 1,
        "publicKey": "%s",
        "publicKeyProof": "%s",
        "batchSize": 2,
        "expiration": "2147483647",
        "redeemers": ["%s"],
        "deploymentId": "test-deployment"
      }
    ]
  })",
      issuer_url.spec().c_str(), encoded_public_key.c_str(),
      encoded_public_key_proof.c_str(), redeemer_origin.Serialize().c_str());

  auto config =
      private_verification_tokens::PrivateVerificationTokensIssuerConfig::
          Create(base::test::ParseJsonDict(json_str));
  ASSERT_TRUE(config);
  service->SetIssuerConfig(config);

  // 1. Issuance Phase: Navigate regular browser to issuer origin to get tokens.
  {
    base::test::TestFuture<void> tokens_stored_future;
    class StoredWaiter : public PrivateVerificationTokensService::Observer {
     public:
      explicit StoredWaiter(base::OnceClosure callback)
          : callback_(std::move(callback)) {}
      void OnTokensStored() override { std::move(callback_).Run(); }

     private:
      base::OnceClosure callback_;
    };
    StoredWaiter stored_waiter(tokens_stored_future.GetCallback());
    base::ScopedObservation<PrivateVerificationTokensService,
                            PrivateVerificationTokensService::Observer>
        observation(&stored_waiter);
    observation.Observe(service);

    ASSERT_TRUE(content::NavigateToURL(
        chrome_test_utils::GetActiveWebContents(this), issuer_home_url));
    EXPECT_TRUE(tokens_stored_future.Wait());
  }

  // Verify tokens are stored in the database.
  base::test::TestFuture<std::vector<url::Origin>> issuers_future;
  service->GetTokenIssuers(issuers_future.GetCallback());
  EXPECT_THAT(issuers_future.Take(), testing::ElementsAre(issuer_origin));

  // 2. Redemption Phase: In Incognito, navigate to redeemer URL which redirects
  // to destination URL.
  Profile* incognito_profile =
      GetProfile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_TRUE(incognito_profile);

  std::unique_ptr<content::WebContents> incognito_web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(incognito_profile));
  ASSERT_TRUE(incognito_web_contents);

  ASSERT_TRUE(content::NavigateToURL(incognito_web_contents.get(), redirect_url,
                                     destination_url));

  // Hop 1 (redeemer.a.test) received and redeemed the token. Hop 2
  // (destination.b.test) did NOT receive a token, so redemption_count_ is 1.
  EXPECT_EQ(redemption_count_, 1);
  ASSERT_TRUE(last_redeemed_token_header_.has_value());
  EXPECT_TRUE(last_recovered_metadata_.has_value());
  EXPECT_EQ(last_recovered_metadata_.value(), 0);

  // One token was deleted because it was spent on Hop 1, but one token remains.
  base::test::TestFuture<std::vector<url::Origin>> issuers_after_future;
  service->GetTokenIssuers(issuers_after_future.GetCallback());
  EXPECT_THAT(issuers_after_future.Take(), testing::ElementsAre(issuer_origin));

  // 3. Navigate to a direct URL on redeemer.a.test to consume the remaining
  // token.
  const GURL direct_redeem_url =
      https_server().GetURL("redeemer.a.test", "/direct-redeem");
  ASSERT_TRUE(
      content::NavigateToURL(incognito_web_contents.get(), direct_redeem_url));

  EXPECT_EQ(redemption_count_, 2);

  // Now all tokens have been spent.
  base::test::TestFuture<std::vector<url::Origin>> issuers_final_future;
  service->GetTokenIssuers(issuers_final_future.GetCallback());
  EXPECT_TRUE(issuers_final_future.Take().empty());
  EXPECT_FALSE(service->GetTokenForRedemption(redeemer_origin).has_value());
}

}  // namespace
