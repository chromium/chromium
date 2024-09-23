// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/k_anonymity_service/k_anonymity_service_client.h"

#include "base/json/json_string_value_serializer.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/k_anonymity_service/k_anonymity_service_urls.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "content/public/browser/k_anonymity_service_delegate.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/third_party/quiche/src/quiche/binary_http/binary_http_message.h"
#include "services/network/public/cpp/features.h"
#include "services/network/test/oblivious_http_request_test_helper.h"
#include "services/network/test/trust_token_request_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

std::unique_ptr<HttpResponse> MakeTrustTokenFailureResponse() {
  // No need to report a failure HTTP code here: returning a vanilla OK should
  // fail the Trust Tokens operation client-side.
  auto ret = std::make_unique<net::test_server::BasicHttpResponse>();
  return ret;
}

// Constructs and returns an HTTP response bearing the given base64-encoded
// Trust Tokens issuance or redemption protocol response message.
std::unique_ptr<net::test_server::HttpResponse> MakeTrustTokenResponse(
    std::string_view contents) {
  CHECK([&]() {
    std::string temp;
    return base::Base64Decode(contents, &temp);
  }());

  auto ret = std::make_unique<net::test_server::BasicHttpResponse>();
  ret->AddCustomHeader("Sec-Private-State-Token", std::string(contents));
  return ret;
}

void OnCreateBrowserContextServices(content::BrowserContext* context) {
  // Sets all required testing factories to have control over identity
  // environment during test. Effectively, this substitutes the real identity
  // environment with identity test environment, taking care to fulfill all
  // required dependencies.
  IdentityTestEnvironmentProfileAdaptor::
      SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
}

// Uses an embedded test server to act like a fake K-anonymity service for all
// of the endpoints.
class TestKAnonymityServiceMixin : public InProcessBrowserTestMixin {
 public:
  explicit TestKAnonymityServiceMixin(InProcessBrowserTestMixinHost* host)
      : InProcessBrowserTestMixin(host) {}
  TestKAnonymityServiceMixin(const TestKAnonymityServiceMixin&) = delete;
  TestKAnonymityServiceMixin& operator=(const TestKAnonymityServiceMixin&) =
      delete;
  ~TestKAnonymityServiceMixin() override = default;

  void SetUp() override {
    https_server_ = std::make_unique<net::test_server::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS);

    https_server_->RegisterRequestHandler(base::BindRepeating(
        &TestKAnonymityServiceMixin::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE((https_server_handle_ = https_server_->StartAndReturnHandle()));

    GURL ohttp_relay = https_server_->GetURL("/ohttpRelay");

    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{{network::features::kPrivateStateTokens, {}},
                              {features::kKAnonymityService,
                               {{"KAnonymityServiceAuthServer",
                                 https_server_->base_url().spec()},
                                {"KAnonymityServiceJoinServer",
                                 https_server_->base_url().spec()},
                                {"KAnonymityServiceJoinRelayServer",
                                 ohttp_relay.spec()},
                                {"KAnonymityServiceQueryServer",
                                 https_server_->base_url().spec()},
                                {"KAnonymityServiceQueryRelayServer",
                                 ohttp_relay.spec()}}}},
        /*disabled_features=*/{});
  }

  std::unique_ptr<HttpResponse> HandleGenerateShortIdentifierRequest(
      const HttpRequest& request) {
    auto http_response = std::make_unique<BasicHttpResponse>();
    if (!base::Contains(request.headers, "Authorization")) {
      http_response->set_code(net::HTTP_UNAUTHORIZED);
      ADD_FAILURE() << "Missing authorization in Short Identifier request.";
      return http_response;
    }
    http_response->set_code(net::HTTP_OK);
    http_response->set_content("{\"shortClientIdentifier\": 1}");
    http_response->set_content_type("application/json");
    return http_response;
  }

  std::unique_ptr<HttpResponse> HandleTrustTokenKeyRequest(
      const HttpRequest& request) {
    std::string commitment_in = trust_token_handler_.GetKeyCommitmentRecord();
    // Parse and reformat JSON for the key commitment.

    base::Value::Dict commitment_in_dict =
        base::test::ParseJsonDict(commitment_in);

    // Commitments are keyed by protocol version, and we don't care which
    // version we get, so just take the first one.
    base::Value::Dict* commitment_type_in_dict =
        commitment_in_dict.begin()->second.GetIfDict();
    base::Value::Dict commitment_out_dict;
    commitment_out_dict.Set(
        "protocolVersion",
        *commitment_type_in_dict->FindString("protocol_version"));
    commitment_out_dict.Set("id", *commitment_type_in_dict->FindInt("id"));
    commitment_out_dict.Set("batchSize",
                            *commitment_type_in_dict->FindInt("batchsize"));

    base::Value::List keys_out;
    base::Value::Dict* commitment_keys_in_dict =
        commitment_type_in_dict->FindDict("keys");
    for (auto idx : *commitment_keys_in_dict) {
      base::Value::Dict* idx_dict = idx.second.GetIfDict();
      base::Value::Dict key_out;
      int identifier = 0;
      CHECK(base::StringToInt(idx.first, &identifier));
      key_out.Set("keyIdentifier", identifier);
      key_out.Set("keyMaterial", *idx_dict->FindString("Y"));
      key_out.Set("expirationTimestampUsec", *idx_dict->FindString("expiry"));
      keys_out.Append(base::Value(std::move(key_out)));
    }
    commitment_out_dict.Set("keys", base::Value(std::move(keys_out)));

    std::string commitment_out;
    JSONStringValueSerializer serializer(&commitment_out);
    serializer.Serialize(base::Value(std::move(commitment_out_dict)));
    auto http_response = std::make_unique<BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content(commitment_out);
    http_response->set_content_type("application/json");
    return http_response;
  }

  std::unique_ptr<HttpResponse> HandleIssueRequest(const HttpRequest& request) {
    if (!base::Contains(request.headers, "Authorization") ||
        !base::Contains(request.headers, "Sec-Private-State-Token") ||
        !base::Contains(request.headers,
                        "Sec-Private-State-Token-Crypto-Version")) {
      ADD_FAILURE()
          << "Trust token issue request missing required headers. Got: "
          << request.all_headers;
      return MakeTrustTokenFailureResponse();
    }
    std::optional<std::string> operation_result = trust_token_handler_.Issue(
        request.headers.at("Sec-Private-State-Token"));
    if (!operation_result) {
      ADD_FAILURE() << "Trust token issue request operation failed.";
      return MakeTrustTokenFailureResponse();
    }

    return MakeTrustTokenResponse(*operation_result);
  }

  std::unique_ptr<HttpResponse> HandleOHttpKeyRequest(
      const HttpRequest& request) {
    auto http_response = std::make_unique<BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content(ohttp_request_handler_.GetPublicKeyConfigs());
    http_response->set_content_type("application/ohttp-keys");
    return http_response;
  }

  quiche::BinaryHttpResponse HandleBhttpQueryRequest(
      const quiche::BinaryHttpRequest& request) {
    quiche::BinaryHttpResponse response(net::HTTP_OK);
    // Request looks like this:
    // { setsForType: [
    //  { type: "t1", hashes: ["a", "b", "c"]},
    //  { type: "t1", hashes: ["d', "e", "f", "f", "c"]},
    //  { type: "t2"}
    // ]}

    // Response has the form:
    // { kAnonymousSets: [
    //  { type: "t1", hashes: ["c", "f"]}
    // ]}

    // As a hack we can just replace "setsForType" with "kAnonymousSets".
    std::string response_body = std::string(request.body());
    base::ReplaceSubstringsAfterOffset(&response_body, 0, "setsForType",
                                       "kAnonymousSets");
    response.AddHeaderField({"Content-Type", "application/json"});
    response.set_body(response_body);

    return response;
  }

  quiche::BinaryHttpResponse HandleBhttpJoinRequest(
      const quiche::BinaryHttpRequest& request) {
    quiche::BinaryHttpResponse response(net::HTTP_OK);

    std::string path = request.control_data().path;
    std::optional<std::string> redemption_result;
    for (const auto& header : request.GetHeaderFields()) {
      if (base::EqualsCaseInsensitiveASCII(header.name,
                                           "Sec-Private-State-Token")) {
        redemption_result = trust_token_handler_.Redeem(header.value);
        break;
      }
    }
    if (redemption_result) {
      response.AddHeaderField({"Sec-Private-State-Token", *redemption_result});
    }

    {
      base::AutoLock lock(mutex_);
      join_called_ = true;
    }

    response.AddHeaderField({"Content-Type", "application/json"});
    response.set_body("{}");

    return response;
  }

  std::unique_ptr<HttpResponse> HandleOhttpRelayRequest(
      const HttpRequest& request) {
    auto http_response = std::make_unique<BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("message/ohttp-response");

    std::pair<std::string, quiche::ObliviousHttpRequest::Context>
        plaintext_request =
            ohttp_request_handler_.DecryptRequest(request.content);
    quiche::BinaryHttpRequest bhttp_request =
        quiche::BinaryHttpRequest::Create(plaintext_request.first).value();
    std::string path = bhttp_request.control_data().path;

    std::optional<quiche::BinaryHttpResponse> response;
    if (path.starts_with("/v1:query?key=")) {
      response.emplace(HandleBhttpQueryRequest(bhttp_request));
    } else if (path.starts_with("/v1/types/fledge/sets/")) {
      response.emplace(HandleBhttpJoinRequest(bhttp_request));
    }
    CHECK(response) << path;
    http_response->set_content(ohttp_request_handler_.EncryptResponse(
        response->Serialize().value(), /*context=*/plaintext_request.second));

    return http_response;
  }

  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    // Handle all of the URLs in k_anonymity_service_urls.h
    GURL absolute_url = https_server_->GetURL(request.relative_url);
    std::string path = absolute_url.path();
    if (path == "/v1/generateShortIdentifier") {
      return HandleGenerateShortIdentifierRequest(request);
    } else if (path.starts_with("/v1/1/fetchKeys")) {
      return HandleTrustTokenKeyRequest(request);
    } else if (path.starts_with("/v1/1/issueTrustToken")) {
      return HandleIssueRequest(request);
    } else if (path.starts_with("/v1/proxy/keys")) {
      return HandleOHttpKeyRequest(request);
    } else if (path.starts_with("/ohttpRelay")) {
      return HandleOhttpRelayRequest(request);
    } else {
      return nullptr;
    }
  }

  bool JoinWasCalled() const {
    base::AutoLock lock(mutex_);
    return join_called_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  network::test::TrustTokenRequestHandler trust_token_handler_;
  network::test::ObliviousHttpRequestTestHelper ohttp_request_handler_;

  std::unique_ptr<net::test_server::EmbeddedTestServer> https_server_;

  mutable base::Lock mutex_;
  bool join_called_ GUARDED_BY(mutex_);

  // This handle needs to be last since it will shutdown the server when its
  // destructor runs. That prevents any of the handlers (which execute on a
  // separate thread) from accessing internal state that has been destroyed.
  net::test_server::EmbeddedTestServerHandle https_server_handle_;
};

class KAnonymityServiceClientBrowserTest
    : public MixinBasedInProcessBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&OnCreateBrowserContextServices));
  }

  void SetUpOnMainThread() override {
    // Set up all of the Profile, signin, and identity.
    adaptor_ = std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
        browser()->profile());

    signin::IdentityTestEnvironment* identity_env =
        adaptor_->identity_test_env();
    AccountInfo account_info = identity_env->MakePrimaryAccountAvailable(
        "a@gmail.com", signin::ConsentLevel::kSignin);

    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_run_chrome_privacy_sandbox_trials(true);
    identity_env->UpdateAccountInfoForAccount(account_info);
    identity_env->SetAutomaticIssueOfAccessTokens(true);

    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

 protected:
  TestKAnonymityServiceMixin k_anon_service_{&mixin_host_};

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor> adaptor_;
  base::CallbackListSubscription subscription_;
};

IN_PROC_BROWSER_TEST_F(KAnonymityServiceClientBrowserTest, TestJoin) {
  Profile* profile = browser()->profile();
  content::KAnonymityServiceDelegate* delegate =
      profile->GetKAnonymityServiceDelegate();
  ASSERT_TRUE(delegate);
  base::RunLoop run_loop;
  delegate->JoinSet("asdf", base::BindLambdaForTesting([&](bool result) {
                      EXPECT_TRUE(result);
                      run_loop.Quit();
                    }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(KAnonymityServiceClientBrowserTest, TestQuery) {
  Profile* profile = browser()->profile();
  content::KAnonymityServiceDelegate* delegate =
      profile->GetKAnonymityServiceDelegate();
  ASSERT_TRUE(delegate);
  base::RunLoop run_loop;

  std::vector<std::string> set_ids = {"foo", "bar", "baz"};
  delegate->QuerySets(
      set_ids, base::BindLambdaForTesting([&](std::vector<bool> results) {
        EXPECT_THAT(results, testing::ElementsAre(true, true, true));
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace
