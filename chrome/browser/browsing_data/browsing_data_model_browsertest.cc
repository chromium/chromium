// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/browsing_data_model.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_model_delegate.h"
#include "chrome/browser/media/clear_key_cdm_test_helper.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations_mixin.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browsing_data/content/browsing_data_model_test_util.h"
#include "components/browsing_data/content/browsing_data_test_util.h"
#include "components/browsing_data/content/shared_worker_info.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/public/browser/attribution_data_model.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/private_aggregation_data_model.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "media/base/media_switches.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "net/dns/mock_host_resolver.h"
#include "net/shared_dictionary/shared_dictionary_isolation_key.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/test/trust_token_request_handler.h"
#include "services/network/test/trust_token_test_server_handler_registration.h"
#include "services/network/test/trust_token_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::test::FeatureRef;
using base::test::FeatureRefAndParams;
using net::test_server::BasicHttpResponse;
using net::test_server::HttpMethod;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace {

constexpr char kTestHost[] = "a.test";
constexpr char kTestHost2[] = "b.test";
constexpr char kTestHost3[] = "c.test";

// FedCM constants
constexpr char kAccountId[] = "carl21334213";
constexpr char kIdpOrigin[] = "https://127.0.0.1";
constexpr char kExpectedConfigPath[] = "/fedcm.json";
constexpr char kExpectedWellKnownPath[] = "/.well-known/web-identity";
constexpr char kTestContentType[] = "application/json";
constexpr char kIdpForbiddenHeader[] = "Sec-FedCM-CSRF";
static constexpr char kSetLoginHeader[] = "Set-Login";
static constexpr char kLoggedInHeaderValue[] = "logged-in";
static constexpr char kLoggedOutHeaderValue[] = "logged-out";
constexpr char kToken[] = "[not a real token]";

void ProvideRequestHandlerKeyCommitmentsToNetworkService(
    std::string_view host,
    net::EmbeddedTestServer* https_server,
    const network::test::TrustTokenRequestHandler& request_handler) {
  base::flat_map<url::Origin, std::string_view> origins_and_commitments;
  std::string key_commitments = request_handler.GetKeyCommitmentRecord();

  GURL::Replacements replacements;
  replacements.SetHostStr(host);
  origins_and_commitments.insert_or_assign(
      url::Origin::Create(
          https_server->base_url().ReplaceComponents(replacements)),
      key_commitments);

  base::RunLoop run_loop;
  content::GetNetworkService()->SetTrustTokenKeyCommitments(
      network::WrapKeyCommitmentsForIssuers(std::move(origins_and_commitments)),
      run_loop.QuitClosure());
  run_loop.Run();
}

void JoinInterestGroup(const content::ToRenderFrameHost& adapter,
                       net::EmbeddedTestServer* https_server,
                       const std::string& owner_host) {
  // join interest group
  auto command = content::JsReplace(
      R"(
    (async () => {
      try {
        navigator.joinAdInterestGroup(
            {
              name: 'cars',
              owner: $1,
              biddingLogicURL: $2,
              trustedBiddingSignalsURL: $3,
              trustedBiddingSignalsKeys: ['key1'],
              userBiddingSignals: {some: 'json', data: {here: [1, 2, 3]}},
              ads: [{
                renderURL: $4,
                metadata: {ad: 'metadata', here: [1, 2, 3]},
              }],
            },
            /*joinDurationSec=*/ 1000);
      } catch (e) {
        return e.toString();
      }
      return "Success";
    })())",
      https_server->GetURL(owner_host, "/"),
      https_server->GetURL(owner_host, "/interest_group/bidding_logic.js"),
      https_server->GetURL(owner_host,
                           "/interest_group/trusted_bidding_signals.json"),
      GURL("https://example.com/render"));
  EXPECT_EQ("Success", EvalJs(adapter, command));
}

void RunAdAuction(const content::ToRenderFrameHost& adapter,
                  net::EmbeddedTestServer* https_server,
                  const std::string& seller_host,
                  const std::string& buyer_host) {
  std::string command = content::JsReplace(
      R"(
      (async function() {
        try {
          await navigator.runAdAuction({
            seller: $1,
            decisionLogicURL: $2,
            interestGroupBuyers: [$3],
          });
        } catch (e) {
          return e.toString();
        }
        return "Success";
      })())",
      https_server->GetURL(seller_host, "/"),
      https_server->GetURL(seller_host, "/interest_group/decision_logic.js"),
      https_server->GetURL(buyer_host, "/"));
  EXPECT_EQ("Success", EvalJs(adapter, command));
}

void ExecuteScriptInSharedStorageWorklet(
    const content::ToRenderFrameHost& execution_target,
    const std::string& script,
    GURL* out_module_script_url,
    net::EmbeddedTestServer* https_server) {
  CHECK(out_module_script_url);

  base::StringPairs run_function_body_replacement;
  run_function_body_replacement.emplace_back("{{RUN_FUNCTION_BODY}}", script);

  std::string host =
      execution_target.render_frame_host()->GetLastCommittedOrigin().host();

  *out_module_script_url =
      https_server->GetURL(host, net::test_server::GetFilePathWithReplacements(
                                     "/shared_storage/customizable_module.js",
                                     run_function_body_replacement));

  EXPECT_TRUE(ExecJs(execution_target,
                     content::JsReplace("sharedStorage.worklet.addModule($1)",
                                        *out_module_script_url)));

  testing::AssertionResult result =
      ExecJs(execution_target,
             "sharedStorage.run('test-operation', {keepAlive: true});");
}

void AccessTopics(const content::ToRenderFrameHost& adapter) {
  std::string command =
      R"(
    (async () => {
      try {
        document.browsingTopics();
      } catch (e) {
        return e.toString();
      }
      return "Success";
    })())";
  EXPECT_EQ("Success", EvalJs(adapter, command));
}

class IdpTestServer {
 public:
  struct ConfigDetails {
    net::HttpStatusCode status_code;
    std::string content_type;
    std::string accounts_endpoint_url;
    std::string client_metadata_endpoint_url;
    std::string id_assertion_endpoint_url;
    std::string login_url;
    std::map<std::string,
             base::RepeatingCallback<std::unique_ptr<HttpResponse>(
                 const HttpRequest&)>>
        servlets;
  };

  IdpTestServer() = default;
  ~IdpTestServer() = default;

  IdpTestServer(const IdpTestServer&) = delete;
  IdpTestServer& operator=(const IdpTestServer&) = delete;

  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    // RP files are fetched from the /test base directory. Assume anything
    // to other paths is directed to the IdP.
    if (request.relative_url.rfind("/test", 0) == 0) {
      return nullptr;
    }

    if (request.relative_url.rfind("/header/", 0) == 0) {
      return BuildIdpHeaderResponse(request);
    }

    if (request.all_headers.find(kIdpForbiddenHeader) != std::string::npos) {
      EXPECT_EQ(request.headers.at(kIdpForbiddenHeader), "?1");
    }

    auto response = std::make_unique<BasicHttpResponse>();
    if (IsGetRequestWithPath(request, kExpectedConfigPath)) {
      BuildConfigResponseFromDetails(*response.get(), config_details_);
      return response;
    }

    if (IsGetRequestWithPath(request, kExpectedWellKnownPath)) {
      BuildWellKnownResponse(*response.get());
      return response;
    }

    if (config_details_.servlets[request.relative_url]) {
      return config_details_.servlets[request.relative_url].Run(request);
    }

    return nullptr;
  }

  std::unique_ptr<HttpResponse> BuildIdpHeaderResponse(
      const HttpRequest& request) {
    auto response = std::make_unique<BasicHttpResponse>();
    if (request.relative_url.find("/header/signin") != std::string::npos) {
      response->AddCustomHeader(kSetLoginHeader, kLoggedInHeaderValue);
    } else if (request.relative_url.find("/header/signout") !=
               std::string::npos) {
      response->AddCustomHeader(kSetLoginHeader, kLoggedOutHeaderValue);
    } else {
      return nullptr;
    }
    response->set_code(net::HTTP_OK);
    response->set_content_type("text/plain");
    response->set_content("Header sent.");
    return response;
  }

  void SetConfigResponseDetails(ConfigDetails details) {
    config_details_ = details;
  }

 private:
  void BuildConfigResponseFromDetails(BasicHttpResponse& response,
                                      const ConfigDetails& details) {
    std::string content = ConvertToJsonDictionary(
        {{"accounts_endpoint", details.accounts_endpoint_url},
         {"client_metadata_endpoint", details.client_metadata_endpoint_url},
         {"id_assertion_endpoint", details.id_assertion_endpoint_url},
         {"login_url", details.login_url}});
    response.set_code(details.status_code);
    response.set_content(content);
    response.set_content_type(details.content_type);
  }

  void BuildWellKnownResponse(BasicHttpResponse& response) {
    std::string content = base::StringPrintf("{\"provider_urls\": [\"%s\"]}",
                                             kExpectedConfigPath);
    response.set_code(net::HTTP_OK);
    response.set_content(content);
    response.set_content_type("application/json");
  }

  std::string ConvertToJsonDictionary(
      const std::map<std::string, std::string>& data) {
    std::string out = "{";
    for (auto it : data) {
      out += "\"" + it.first + "\":\"" + it.second + "\",";
    }
    if (!out.empty()) {
      out[out.length() - 1] = '}';
    }
    return out;
  }

  bool IsGetRequestWithPath(const HttpRequest& request,
                            const std::string& expected_path) {
    return request.method == net::test_server::HttpMethod::METHOD_GET &&
           request.relative_url == expected_path;
  }

  ConfigDetails config_details_;
};

std::string GetIdpConfigUrl(net::EmbeddedTestServer* https_server) {
  return std::string(kIdpOrigin) + ":" +
         base::NumberToString(https_server->port()) + "/fedcm.json";
}

IdpTestServer::ConfigDetails BuildValidConfigDetails() {
  std::string accounts_endpoint_url = "/fedcm/accounts_endpoint.json";
  std::string client_metadata_endpoint_url =
      "/fedcm/client_metadata_endpoint.json";
  std::string id_assertion_endpoint_url = "/fedcm/id_assertion_endpoint.json";
  std::string login_url = "/fedcm/login.html";
  std::map<std::string, base::RepeatingCallback<std::unique_ptr<HttpResponse>(
                            const HttpRequest&)>>
      servlets;
  servlets[id_assertion_endpoint_url] = base::BindRepeating(
      [](const HttpRequest& request) -> std::unique_ptr<HttpResponse> {
        EXPECT_EQ(request.method, HttpMethod::METHOD_POST);
        EXPECT_EQ(request.has_content, true);
        auto response = std::make_unique<BasicHttpResponse>();
        response->set_code(net::HTTP_OK);
        response->set_content_type("text/json");
        DCHECK(request.headers.contains("Origin"));
        response->AddCustomHeader(
            network::cors::header_names::kAccessControlAllowOrigin,
            request.headers.at("Origin"));
        response->AddCustomHeader(
            network::cors::header_names::kAccessControlAllowCredentials,
            "true");
        // Standard scopes were used, so no extra permission needed.
        // Return a token immediately.
        response->set_content(R"({"token": ")" + std::string(kToken) + R"("})");
        return response;
      });
  return {net::HTTP_OK,
          kTestContentType,
          accounts_endpoint_url,
          client_metadata_endpoint_url,
          id_assertion_endpoint_url,
          login_url,
          servlets};
}

void RunFedCm(const content::ToRenderFrameHost& adapter,
              net::EmbeddedTestServer* https_server) {
  std::string command = content::JsReplace(
      R"(
    (async () => {
      try {
        let cred = await navigator.credentials.get({
          identity: {
            providers: [{
              configURL: $1,
              clientId: '123',
              nonce: '2',
            }]
          },
          mediation: 'required'
        });
        return cred.token;
      } catch (e) {
        return e.toString();
      }
    })())",
      GetIdpConfigUrl(https_server));
  EXPECT_EQ(kToken, EvalJs(adapter, command));
}

void AddLocalStorageUsage(content::RenderFrameHost* render_frame_host,
                          int size) {
  auto command =
      content::JsReplace("localStorage.setItem('key', '!'.repeat($1))", size);
  EXPECT_TRUE(ExecJs(render_frame_host, command));
}

void WaitForModelUpdate(BrowsingDataModel* model, size_t expected_size) {
  while (model->size() != expected_size) {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }
}

void RemoveBrowsingDataForDataOwner(BrowsingDataModel* model,
                                    BrowsingDataModel::DataOwner data_owner) {
  base::RunLoop run_loop;
  model->RemoveBrowsingData(data_owner, run_loop.QuitClosure());
  run_loop.Run();
}

// Calls the accessStorage javascript function and awaits its completion for
// each frame in the active web contents for |browser|.
void EnsurePageAccessedStorage(content::WebContents* web_contents) {
  web_contents->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [](content::RenderFrameHost* frame) {
        EXPECT_TRUE(
            content::EvalJs(frame,
                            "(async () => { return await accessStorage();})()")
                .value.GetBool());
      });
}
}  // namespace

using browsing_data_model_test_util::ValidateBrowsingDataEntries;
using browsing_data_model_test_util::ValidateBrowsingDataEntriesNonZeroUsage;
using OperationResult = storage::SharedStorageDatabase::OperationResult;
using browsing_data_test_util::HasDataForType;
using browsing_data_test_util::SetDataForType;

class BrowsingDataModelBrowserTest
    : public MixinBasedInProcessBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  BrowsingDataModelBrowserTest() {
    auto& field_trial_param =
        network::features::kTrustTokenOperationsRequiringOriginTrial;
    std::vector<FeatureRefAndParams> enabled_features = {
        {network::features::kPrivateStateTokens,
         {{field_trial_param.name,
           field_trial_param.GetName(
               network::features::TrustTokenOriginTrialSpec::
                   kOriginTrialNotRequired)}}},
        {features::kPrivacySandboxAdsAPIsOverride, {}},
        {features::kIsolatedWebApps, {}},
        {features::kIsolatedWebAppDevMode, {}},
        {blink::features::kSharedStorageAPI, {}},
        {blink::features::kInterestGroupStorage, {}},
        {blink::features::kPrivateAggregationApi, {}},
        {blink::features::kAdInterestGroupAPI, {}},
        {blink::features::kFledge, {}},
        {blink::features::kFencedFrames, {}},
        {blink::features::kBrowsingTopics, {}},
        {net::features::kThirdPartyStoragePartitioning, {}},
        {network::features::kCompressionDictionaryTransportBackend, {}},
        {network::features::kCompressionDictionaryTransport, {}},
        // Need to enable CompressionDictionaryTransportOverHttp1 because
        // EmbeddedTestServer uses HTTP/1.1 by default.
        {net::features::kCompressionDictionaryTransportOverHttp1, {}},
    };

    std::vector<FeatureRef> disabled_features = {
        // Need to disable kCompressionDictionaryTransportRequireKnownRootCert
        // because EmbeddedTestServer's certificate is not rooted at a standard
        // CA root.
        net::features::kCompressionDictionaryTransportRequireKnownRootCert};

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
    enabled_features.push_back({media::kExternalClearKeyForTesting, {}});
#endif

    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
  }

  ~BrowsingDataModelBrowserTest() override = default;

  void SetUpOnMainThread() override {
    PrivacySandboxSettingsFactory::GetForProfile(browser()->profile())
        ->SetAllPrivacySandboxAllowedForTesting();
    // Mark all Privacy Sandbox APIs as attested since the test cases are
    // testing behaviors not related to attestations.
    privacy_sandbox::PrivacySandboxAttestations::GetInstance()
        ->SetAllPrivacySandboxAttestedForTesting(true);
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_->AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    network::test::RegisterTrustTokenTestHandlers(https_test_server(),
                                                  &request_handler_);
    idp_server_ = std::make_unique<IdpTestServer>();
    https_server_->RegisterRequestHandler(base::BindRepeating(
        &IdpTestServer::HandleRequest, base::Unretained(idp_server_.get())));
    ASSERT_TRUE(https_server_->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
    // Testing MediaLicenses requires additional command line parameters as
    // it uses the External Clear Key CDM.
    RegisterClearKeyCdm(command_line);
#endif
    // These switches are needed to run FedCM and auto-select the first account.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
    command_line->AppendSwitchASCII(switches::kUseFakeUIForFedCM, kAccountId);
  }

 protected:
  std::unique_ptr<BrowsingDataModel> BuildBrowsingDataModel() {
    base::test::TestFuture<std::unique_ptr<BrowsingDataModel>>
        browsing_data_model;
    BrowsingDataModel::BuildFromDisk(
        browser()->profile(),
        ChromeBrowsingDataModelDelegate::CreateForProfile(browser()->profile()),
        browsing_data_model.GetCallback());
    return browsing_data_model.Take();
  }

  content::StoragePartition* default_storage_partition() {
    return browser()->profile()->GetDefaultStoragePartition();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  net::EmbeddedTestServer* https_test_server() { return https_server_.get(); }

  GURL test_url() { return https_server_->GetURL(kTestHost, "/echo"); }

  void AccessStorage() {
    ASSERT_TRUE(content::NavigateToURL(
        chrome_test_utils::GetActiveWebContents(this), storage_accessor_url()));
    base::RunLoop().RunUntilIdle();
    EnsurePageAccessedStorage(chrome_test_utils::GetActiveWebContents(this));
  }

  GURL storage_accessor_url() {
    auto host_port_pair =
        net::HostPortPair::FromURL(https_test_server()->GetURL(kTestHost, "/"));
    base::StringPairs replacement_text = {
        {"REPLACE_WITH_HOST_AND_PORT", host_port_pair.ToString()}};
    auto replaced_path = net::test_server::GetFilePathWithReplacements(
        "/browsing_data/storage_accessor.html", replacement_text);
    return https_test_server()->GetURL(kTestHost, replaced_path);
  }

  IdpTestServer* idp_server() { return idp_server_.get(); }

  network::test::TrustTokenRequestHandler request_handler_;

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  privacy_sandbox::PrivacySandboxAttestationsMixin
      privacy_sandbox_attestations_mixin_{&mixin_host_};

  // Stop test from installing OS hooks.
  web_app::OsIntegrationManager::ScopedSuppressForTesting os_hooks_suppress_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<IdpTestServer> idp_server_;
};

IN_PROC_BROWSER_TEST_F(BrowsingDataModelBrowserTest,
                       SharedStorageHandledCorrectly) {
  // Add origin shared storage.
  auto* shared_storage_manager =
      default_storage_partition()->GetSharedStorageManager();
  ASSERT_NE(nullptr, shared_storage_manager);

  base::test::TestFuture<OperationResult> future;
  url::Origin testOrigin = url::Origin::Create(GURL("https://a.test"));
  shared_storage_manager->Set(
      testOrigin, u"key", u"value", future.GetCallback(),
      storage::SharedStorageDatabase::SetBehavior::kDefault);
  EXPECT_EQ(OperationResult::kSet, future.Get());

  std::unique_ptr<BrowsingDataModel> browsing_data_model =
      BuildBrowsingDataModel();
  // Validate shared storage entry saved correctly.
  base::test::TestFuture<uint64_t> test_entry_storage_size;
  shared_storage_manager->FetchOrigins(base::BindLambdaForTesting(
      [&](std::vector<::storage::mojom::StorageUsageInfoPtr>
              storage_usage_info) {
        ASSERT_EQ(1U, storage_usage_info.size());
        test_entry_storage_size.SetValue(
            storage_usage_info[0]->total_size_bytes);
      }));

  ValidateBrowsingDataEntries(
      browsing_data_model.get(),
      {{kTestHost,
        blink::StorageKey::CreateFirstParty(testOrigin),
        {{BrowsingDataModel::StorageType::kSharedStorage},
         test_entry_storage_size.Get(),
         /*cookie_count=*/0}}});

  // Remove origin.
  RemoveBrowsingDataForDataOwner(browsing_data_model.get(), kTestHost);

  // Rebuild Browsing Data Model and verify entries are empty.
  browsing_data_model = BuildBrowsingDataModel();
  ValidateBrowsingDataEntries(browsing_data_model.get(), {});
}

IN_PROC_BROWSER_TEST_F(BrowsingDataModelBrowserTest,
                       SharedStorageAccessReportedCorrectly) {
  // Navigate to test page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));
  auto* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());

  // Validate that the allowed browsing data model is empty.
  ValidateBrowsingDataEntries(content_settings->allowed_browsing_data_model(),
                              {});

  // Create a SharedStorage entry.
  std::string command = R"(
  (async () => {
    try {
      await window.sharedStorage.set('age-group', 1);
      return true;
    } catch {
      return false;
    }
  })();)";
  EXPECT_EQ(true, EvalJs(web_contents(), command));

  // Validate that the allowed browsing data model is populated with
  // SharedStorage entry for `kTestHost`.
  url::Origin testOrigin = https_test_server()->GetOrigin(kTestHost);
  ValidateBrowsingDataEntries(
      content_settings->allowed_browsing_data_model(),
      {{kTestHost,
        blink::StorageKey::CreateFirstParty(testOrigin),
        {{BrowsingDataModel::StorageType::kSharedStorage},
         /*storage_size=*/0,
         /*cookie_count=*/0}}});
}

IN_PROC_BROWSER_TEST_F(BrowsingDataModelBrowserTest, TrustTokenIssuance) {
  // Setup the test server to be able to issue trust tokens, and have it issue
  // some to the profile.
  ProvideRequestHandlerKeyCommitmentsToNetworkService(
      kTestHost, https_test_server(), request_handler_);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server()->GetURL(kTestHost, "/title1.html")));

  std::string issuance_origin =
      url::Origin::Create(https_test_server()->GetURL(kTestHost, "/"))
          .Serialize();

  std::string command = content::JsReplace(R"(
  (async () => {
    try {
      await fetch("/issue", {privateToken: {version: 1,
                                          operation: 'token-request'}});
      return await document.hasPrivateToken($1);
    } catch {
      return false;
    }
  })();)",
                                           issuance_origin);

  EXPECT_EQ(true, EvalJs(web_contents(), command));

  browser()
      ->profile()
      ->GetDefaultStoragePartition()
      ->FlushNetworkInterfaceForTesting();

  // Confirm that a BrowsingDataModel built from disk contains the issued token
  // information.
  std::unique_ptr<BrowsingDataModel> browsing_data_model =
      BuildBrowsingDataModel();

  ValidateBrowsingDataEntries(
      browsing_data_model.get(),
      {{kTestHost,
        https_test_server()->GetOrigin(kTestHost),
        {{BrowsingDataModel::StorageType::kTrustTokens}, 100, 0}}});

  // Remove data for the host, and confirm the model updates appropriately.
  RemoveBrowsingDataForDataOwner(browsing_data_model.get(), kTestHost);
  ValidateBrowsingDataEntries(browsing_data_model.get(), {});

  // Build another model from disk, ensuring the data is no longer present.
  browsing_data_model = BuildBrowsingDataModel();
  ValidateBrowsingDataEntries(browsing_data_model.get(), {});
}

IN_PROC_BROWSER_TEST_F(BrowsingDataModelBrowserTest,
                       InterestGroupsHandledCorrectly) {
  // Check that no interest groups are joined at the beginning of the test.
  std::unique_ptr<BrowsingDataModel> browsing_data_model =
      BuildBrowsingDataModel();
  ValidateBrowsingDataEntries(browsing_data_model.get(), {});
  ASSERT_EQ(browsing_data_model->size(), 0u);

  // Join an interest group.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));
  JoinInterestGroup(web_contents(), https_test_server(), kTestHost);

  // Waiting for the browsing data model to be populated, otherwise the test is
  // flaky.
  do {
    browsing_data_model = BuildBrowsingDataModel();
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  } while (browsing_data_model->size() != 1);

  // Validate that an interest group is added.
  url::Origin testOrigin = https_test_server()->GetOrigin(kTestHost);
  content::InterestGroupManager::InterestGroupDataKey data_key{testOrigin,
                                                               testOrigin};
  ValidateBrowsingDataEntries(
      browsing_data_model.get(),
      {{kTestHost,
        data_key,
        {{BrowsingDataModel::StorageType::kInterestGroup},
         /*storage_size=*/1024,
         /*cookie_count=*/0}}});

  // Remove Interest Group.
  RemoveBrowsingDataForDataOwner(browsing_data_model.get(), kTestHost);

  // Rebuild Browsing Data Model and verify entries are empty.
  browsing_data_model = BuildBrowsingDataModel();
  ValidateBrowsingDataEntries(browsing_data_model.get(), {});
}

IN_PROC_BROWSER_TEST_F(BrowsingDataModelBrowserTest,
                       InterestGroupsAccessReportedCorrectly) {
  // Navigate to test page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));
  auto* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());

  // Validate that the allowed browsing data model is empty.
  auto* allowed_browsing_data_model =
      content_settings->allowed_browsing_data_model();
  ValidateBrowsingDataEntries(allowed_browsing_data_model, {});
  ASSERT_EQ(allowed_browsing_data_model->size(), 0u);

  // Join an interest group.
  JoinInterestGroup(web_contents(), https_test_server(), kTestHost);
  WaitForModelUpdate(allowed_browsing_data_model, 1);

  // Validate that an interest group is reported to the browsing data model.
  url::Origin testOrigin = https_test_server()->GetOrigin(kTestHost);
  content::InterestGroupManager::InterestGroupDataKey data_key{testOrigin,
                                                               testOrigin};
  ValidateBrowsingDataEntries(
      allowed_browsing_data_model,
      {{kTestHost,
        data_key,
        {{BrowsingDataModel::StorageType::kInterestGroup},
         /*storage_size=*/0,
         /*cookie_count=*/0}}});
}

IN_PROC_BROWSER_TEST_F(BrowsingDataModelBrowserTest,
                       AuctionWinReportedCorrectly) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));
  JoinInterestGroup(web_contents(), https_test_server(), kTestHost);

  // Run an auction on `kTestHost2`. A different host is used to ensure the
  // correct host (`kTestHost`) is reported as having accessed storage.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server()->GetURL(kTestHost2, "/echo")));

  auto* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());

  // Validate that the allowed browsing data model is empty.
  auto* allowed_browsing_data_model =
      content_settings->allowed_browsing_data_model();
  ValidateBrowsingDataEntries(allowed_browsing_data_model, {});
  ASSERT_EQ(allowed_browsing_data_model->size(), 0u);

  RunAdAuction(web_contents(), https_test_server(), /*seller_host=*/kTestHost2,
               /*buyer_host=*/kTestHost);
  WaitForModelUpdate(allowed_browsing_data_model, 1);

  url::Origin testOrigin = https_test_server()->GetOrigin(kTestHost);
  content::InterestGroupManager::InterestGroupDataKey data_key{testOrigin,
                                                               testOrigin};
  ValidateBrowsingDataEntries(
      allowed_browsing_data_model,
      {{kTestHost,
        data_key,
        {{BrowsingDataModel::StorageType::kInterestGroup},
         /*storage_size=*/0,
         /*cookie_count=*/0}}});
}

IN_PROC_BROWSER_TEST_F(BrowsingDataModelBrowserTest,
                       AttributionReportingAccessReportedCorrectly) {
  const GURL kTestCases[] = {
      https_test_server()->GetURL(
          "a.test", "/attribution_reporting/register_source_headers.html"),
      https_test_server()->GetURL(
          "a.test", "/attribution_reporting/register_trigger_headers.html")};

  for (const auto& register_url : kTestCases) {
    // Navigate to test page.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));
    auto* content_settings =
        content_settings::PageSpecificContentSettings::GetForFrame(
            web_contents()->GetPrimaryMainFrame());

    // Validate that the allowed browsing data model is empty.
    auto* allowed_browsing_data_model =
        content_settings->allowed_browsing_data_model();
    ValidateBrowsingDataEntries(allowed_browsing_data_model, {});
    ASSERT_EQ(allowed_browsing_data_model->size(), 0u);

    // Register a source.
    ASSERT_TRUE(ExecJs(web_contents(), content::JsReplace(R"(
      const img = document.createElement('img');
      img.attributionSrc = $1;)",
                                                          register_url)));

    WaitForModelUpdate(allowed_browsing_data_model, 1);

    // Validate that an attribution reporting datakey is reported to the
    // browsing data model.
    url::Origin testOrigin = https_test_server()->GetOrigin(kTestHost);
    content::AttributionDataModel::DataKey data_key{testOrigin};
    ValidateBrowsingDataEntries(
        allowed_browsing_data_model,
        {{kTestHost,
          data_key,
          {{BrowsingDataModel::StorageType::kAttributionReporting},
           /*storage_size=*/0,
           /*cookie_count=*/0}}});
  }
}

IN_PROC_BROWSER_TEST_F(BrowsingDataModelBrowserTest,
                       PrivateAggregationHandledCorrectly) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));

  // Validate that there are no entries in the browsing data model.
  std::unique_ptr<BrowsingDataModel> browsing_data_model =
      BuildBrowsingDataModel();
  ValidateBrowsingDataEntries(browsing_data_model.get(), {});

  GURL out_script_url;
  ExecuteScriptInSharedStorageWorklet(web_contents(), R"(
      privateAggregation.contributeToHistogram({bucket: 1n, value: 2});
    )",
                                      &out_script_url, https_test_server());

  do {
    browsing_data_model = BuildBrowsingDataModel();
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  } while (browsing_data_model->size() < 1);

  // Validate that a private aggregation data key is added.
  url::Origin test_origin = https_test_server()->GetOrigin(kTestHost);
  content::PrivateAggregationDataModel::DataKey data_key{test_origin};

  ValidateBrowsingDataEntries(
      browsing_data_model.get(),
      {{kTestHost,
        data_key,
        {{BrowsingDataModel::StorageType::kPrivateAggregation},
         /*storage_size=*/100,
         /*cookie_count=*/0}}});

  // Remove datakey from aggregation service and private budgeter.
  RemoveBrowsingDataForDataOwner(browsing_data_model.get(), kTestHost);

  // Rebuild Browsing Data Model and verify entries are empty.
  browsing_data_model = BuildBrowsingDataModel();
  ValidateBrowsingDataEntries(browsing_data_model.get(), {});
}

IN_PROC_BROWSER_TEST_F(BrowsingDataModelBrowserTest,
                       TopicsAccessReportedCorrectly) {
  // Navigate to test page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));

  auto* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());

  // Validate that the allowed browsing data model is empty.
  auto* allowed_browsing_data_model =
      content_settings->allowed_browsing_data_model();
  ValidateBrowsingDataEntries(allowed_browsing_data_model, {});
  ASSERT_EQ(allowed_browsing_data_model->size(), 0u);

  // Get Topics
  AccessTopics(web_contents());

  WaitForModelUpdate(allowed_browsing_data_model, 1);

  // Validate Topics are reported correctly
  url::Origin testOrigin = https_test_server()->GetOrigin(kTestHost);
  ValidateBrowsingDataEntries(
      allowed_browsing_data_model,
      {{kTestHost,
        testOrigin,
        {{static_cast<BrowsingDataModel::StorageType>(
             ChromeBrowsingDataModelDelegate::StorageType::kTopics)},
         /*storage_size=*/0,
         /*cookie_count=*/0}}});
  ASSERT_EQ(allowed_browsing_data_model->size(), 1u);

  // Clear Topic via BDM
  RemoveBrowsingDataForDataOwner(allowed_browsing_data_model, kTestHost);

  // Validate that the allowed browsing data model is cleared.
  ValidateBrowsingDataEntries(allowed_browsing_data_model, {});
  ASSERT_EQ(allowed_browsing_data_model->size(), 0u);
}

IN_PROC_BROWSER_TEST_F(BrowsingDataModelBrowserTest,
                       IsolatedWebAppUsageInDefaultStoragePartitionModel) {
  // Check that no IWAs are installed at the beginning of the test.
  std::unique_ptr<BrowsingDataModel> browsing_data_model =
      BuildBrowsingDataModel();
  ValidateBrowsingDataEntries(browsing_data_model.get(), {});
  ASSERT_EQ(browsing_data_model->size(), 0u);

  Profile* profile = browser()->profile();
  auto dev_server = web_app::CreateAndStartDevServer(
      FILE_PATH_LITERAL("web_apps/simple_isolated_app"));

  auto iwa_url_info1 = web_app::InstallDevModeProxyIsolatedWebApp(
      profile, dev_server->GetOrigin());
  auto* iwa_frame1 =
      web_app::OpenIsolatedWebApp(profile, iwa_url_info1.app_id());
  AddLocalStorageUsage(iwa_frame1, 100);

  auto iwa_url_info2 = web_app::InstallDevModeProxyIsolatedWebApp(
      profile, dev_server->GetOrigin());
  auto* iwa_frame2 =
      web_app::OpenIsolatedWebApp(profile, iwa_url_info2.app_id());
  AddLocalStorageUsage(iwa_frame2, 500);

  browsing_data_model = BuildBrowsingDataModel();

  ValidateBrowsingDataEntries(
      browsing_data_model.get(),
      {{iwa_url_info1.origin(),
        iwa_url_info1.origin(),
        {{static_cast<BrowsingDataModel::StorageType>(
             ChromeBrowsingDataModelDelegate::StorageType::kIsolatedWebApp)},
         /*storage_size=*/105,
         /*cookie_count=*/0}},
       {iwa_url_info2.origin(),
        iwa_url_info2.origin(),
        {{static_cast<BrowsingDataModel::StorageType>(
             ChromeBrowsingDataModelDelegate::StorageType::kIsolatedWebApp)},
         /*storage_size=*/505,
         /*cookie_count=*/0}}});
}

IN_PROC_BROWSER_TEST_F(BrowsingDataModelBrowserTest,
                       QuotaStorageHandledCorrectly) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_test_server()->GetURL(kTestHost, "/browsing_data/site_data.html")));
  // Ensure that there isn't any data fetched.
  std::unique_ptr<BrowsingDataModel> browsing_data_model =
      BuildBrowsingDataModel();
  ValidateBrowsingDataEntries(browsing_data_model.get(), {});
  ASSERT_EQ(browsing_data_model->size(), 0u);

  std::vector<std::string> quota_storage_data_types = {
      "ServiceWorker", "IndexedDb", "FileSystem"};

  for (auto data_type : quota_storage_data_types) {
    SetDataForType(data_type, web_contents());
    ASSERT_TRUE(HasDataForType(data_type, web_contents()));

    // Ensure that quota data is fetched
    browsing_data_model = BuildBrowsingDataModel();

    // Validate that quota data is fetched to browsing data model.
    url::Origin testOrigin = https_test_server()->GetOrigin(kTestHost);
    auto data_key = blink::StorageKey::CreateFirstParty(testOrigin);
      ValidateBrowsingDataEntriesNonZeroUsage(
          browsing_data_model.get(),
          {{kTestHost,
            data_key,
            {{BrowsingDataModel::StorageType::kQuotaStorage},
             /*storage_size=*/0,
             /*cookie_count=*/0}}});

    ASSERT_EQ(browsing_data_model->size(), 1u);

    // Remove quota entry.
    RemoveBrowsingDataForDataOwner(browsing_data_model.get(), kTestHost);

    // Rebuild Browsing Data Model and verify entries are empty.
    browsing_data_model = BuildBrowsingDataModel();
    ValidateBrowsingDataEntries(browsing_data_model.get(), {});
    ASSERT_EQ(browsing_data_model->size(), 0u);
  }
}

IN_PROC_BROWSER_TEST_F(BrowsingDataModelBrowserTest,
                       LocalStorageHandledCorrectly) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_test_server()->GetURL(kTestHost, "/browsing_data/site_data.html")));
  // Ensure that there isn't any data fetched.
  std::unique_ptr<BrowsingDataModel> browsing_data_model =
      BuildBrowsingDataModel();
  ValidateBrowsingDataEntries(browsing_data_model.get(), {});
  ASSERT_EQ(browsing_data_model->size(), 0u);

  SetDataForType("LocalStorage", web_contents());

  //  Flush storage size to disk.
  auto* storage_partition = browser()->profile()->GetDefaultStoragePartition();
  storage_partition->Flush();

  // To ensure that flushing is completed.
  base::RunLoop().RunUntilIdle();

  auto* dom_storage_context = storage_partition->GetDOMStorageContext();

  // Fetch local storage size from backend.
  base::test::TestFuture<uint64_t> test_entry_storage_size;
  dom_storage_context->GetLocalStorageUsage(base::BindLambdaForTesting(
      [&](const std::vector<content::StorageUsageInfo>& storage_usage_info) {
        ASSERT_EQ(1U, storage_usage_info.size());
        test_entry_storage_size.SetValue(
            storage_usage_info[0].total_size_bytes);
      }));

  // Ensure that local storage is fetched
  browsing_data_model = BuildBrowsingDataModel();

  // Validate that local storage is fetched to browsing data model.
  url::Origin testOrigin = https_test_server()->GetOrigin(kTestHost);
  auto data_key = blink::StorageKey::CreateFirstParty(testOrigin);
  ValidateBrowsingDataEntries(
      browsing_data_model.get(),
      {{kTestHost,
        data_key,
        {{BrowsingDataModel::StorageType::kLocalStorage},
         test_entry_storage_size.Get(),
         /*cookie_count=*/0}}});
  ASSERT_EQ(browsing_data_model->size(), 1u);

  // Remove local storage entry.
  RemoveBrowsingDataForDataOwner(browsing_data_model.get(), kTestHost);

  // Rebuild Browsing Data Model and verify entries are empty.
  browsing_data_model = BuildBrowsingDataModel();
  ValidateBrowsingDataEntries(browsing_data_model.get(), {});
  ASSERT_EQ(browsing_data_model->size(), 0u);
}

IN_PROC_BROWSER_TEST_F(BrowsingDataModelBrowserTest,
                       LocalStorageAccessReportedCorrectly) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_test_server()->GetURL(kTestHost, "/browsing_data/site_data.html")));

  auto* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());

  // Validate that the allowed browsing data model is empty.
  auto* allowed_browsing_data_model =
      content_settings->allowed_browsing_data_model();
  ValidateBrowsingDataEntries(allowed_browsing_data_model, {});
  ASSERT_EQ(allowed_browsing_data_model->size(), 0u);

  SetDataForType("LocalStorage", web_contents());
  WaitForModelUpdate(allowed_browsing_data_model, /*expected_size=*/1);

  // Validate Local Storage is reported.
  url::Origin testOrigin = https_test_server()->GetOrigin(kTestHost);
  auto data_key = blink::StorageKey::CreateFirstParty(testOrigin);
  ValidateBrowsingDataEntries(
      allowed_browsing_data_model,
      {{kTestHost,
        data_key,
        {{BrowsingDataModel::StorageType::kLocalStorage},
         /*storage_size=*/0,
         /*cookie_count=*/0}}});
  ASSERT_EQ(allowed_browsing_data_model->size(), 1u);

  // Delete Local Storage
  RemoveBrowsingDataForDataOwner(allowed_browsing_data_model, kTestHost);
  // Validate that the allowed browsing data model is empty.
  ValidateBrowsingDataEntries(allowed_browsing_data_model, {});
  ASSERT_EQ(allowed_browsing_data_model->size(), 0u);
}

IN_PROC_BROWSER_TEST_F(BrowsingDataModelBrowserTest,
                       SessionStorageAccessReportedCorrectly) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_test_server()->GetURL(kTestHost, "/browsing_data/site_data.html")));

  auto* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());

  // Validate that the allowed browsing data model is empty.
  auto* allowed_browsing_data_model =
      content_settings->allowed_browsing_data_model();
  ValidateBrowsingDataEntries(allowed_browsing_data_model, {});
  ASSERT_EQ(allowed_browsing_data_model->size(), 0u);

  SetDataForType("SessionStorage", web_contents());
  WaitForModelUpdate(allowed_browsing_data_model, /*expected_size=*/1);

  // Validate Session Storage is reported.
  url::Origin testOrigin = https_test_server()->GetOrigin(kTestHost);
  auto storage_key = blink::StorageKey::CreateFirstParty(testOrigin);

  // Obtaining Session Storage namespace_id from the navigation controller.
  const auto& session_storage_namespace_map =
      web_contents()->GetController().GetSessionStorageNamespaceMap();
  const auto& storage_partition_config =
      content::StoragePartitionConfig::CreateDefault(
          web_contents()->GetBrowserContext());
  const auto& namespace_id =
      session_storage_namespace_map.at(storage_partition_config);

  content::SessionStorageUsageInfo data_key{storage_key, namespace_id->id()};

  ValidateBrowsingDataEntries(
      allowed_browsing_data_model,
      {{kTestHost,
        data_key,
        {{BrowsingDataModel::StorageType::kSessionStorage},
         /*storage_size=*/0,
         /*cookie_count=*/0}}});
  ASSERT_EQ(allowed_browsing_data_model->size(), 1u);

  // Delete Session Storage
  RemoveBrowsingDataForDataOwner(allowed_browsing_data_model, kTestHost);

  // Validate that the allowed browsing data model is empty.
  ValidateBrowsingDataEntries(allowed_browsing_data_model, {});
  ASSERT_EQ(allowed_browsing_data_model->size(), 0u);
  // Reloading the page to ensure the renderer reflects deletion before checking
  // if the storage type still exists on disk.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_test_server()->GetURL(kTestHost, "/browsing_data/site_data.html")));
  ASSERT_FALSE(HasDataForType("SessionStorage", web_contents()));
}

IN_PROC_BROWSER_TEST_F(BrowsingDataModelBrowserTest,
                       QuotaStorageAccessReportedCorrectly) {
  // Keeping the `ServiceWorker` type last as checking it after deletion counts
  // as a new access report and repopulates the model, this way we keep it from
  // affecting other quota storage types test.
  std::vector<std::string> quota_storage_data_types = {
      "IndexedDb", "FileSystem", "ServiceWorker"};

  for (auto data_type : quota_storage_data_types) {
    // Re-Navigate to the page for every data type, to prevent any cached data
    // access results from impacting whether access is reported or not.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_test_server()->GetURL(
                       kTestHost, "/browsing_data/site_data.html")));

    auto* content_settings =
        content_settings::PageSpecificContentSettings::GetForFrame(
            web_contents()->GetPrimaryMainFrame());

    // Validate that the allowed browsing data model is empty.
    auto* allowed_browsing_data_model =
        content_settings->allowed_browsing_data_model();
    ValidateBrowsingDataEntries(allowed_browsing_data_model, {});
    ASSERT_EQ(allowed_browsing_data_model->size(), 0u);

    SetDataForType(data_type, web_contents());
    WaitForModelUpdate(allowed_browsing_data_model, /*expected_size=*/1);

    // Validate quota storage is reported.
    url::Origin testOrigin = https_test_server()->GetOrigin(kTestHost);
    auto data_key = blink::StorageKey::CreateFirstParty(testOrigin);
    ValidateBrowsingDataEntries(
        allowed_browsing_data_model,
        {{kTestHost,
          data_key,
          {{BrowsingDataModel::StorageType::kQuotaStorage},
           /*storage_size=*/0,
           /*cookie_count=*/0}}});
    ASSERT_EQ(allowed_browsing_data_model->size(), 1u);

    // Delete quota storage
    RemoveBrowsingDataForDataOwner(allowed_browsing_data_model, kTestHost);
    //  Validate that the allowed browsing data model is empty.
    ValidateBrowsingDataEntries(allowed_browsing_data_model, {});
    ASSERT_EQ(allowed_browsing_data_model->size(), 0u);
    ASSERT_FALSE(HasDataForType(data_type, web_contents()));
  }
}

IN_PROC_BROWSER_TEST_F(BrowsingDataModelBrowserTest,
                       SharedWorkerAccessReportedCorrectly) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_test_server()->GetURL(kTestHost, "/browsing_data/site_data.html")));

  auto* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());

  // Validate that the allowed browsing data model is empty.
  auto* allowed_browsing_data_model =
      content_settings->allowed_browsing_data_model();
  ValidateBrowsingDataEntries(allowed_browsing_data_model, {});
  ASSERT_EQ(allowed_browsing_data_model->size(), 0u);

  SetDataForType("SharedWorker", web_contents());
  WaitForModelUpdate(allowed_browsing_data_model, /*expected_size=*/1);

  // Validate Shared Worker is reported.
  url::Origin testOrigin = https_test_server()->GetOrigin(kTestHost);
  GURL::Replacements replacements;
  replacements.SetPathStr("browsing_data/shared_worker.js");
  GURL worker = testOrigin.GetURL().ReplaceComponents(replacements);
  browsing_data::SharedWorkerInfo data_key(
      worker, /*name=*/"", blink::StorageKey::CreateFirstParty(testOrigin),
      blink::mojom::SharedWorkerSameSiteCookies::kAll);
  ValidateBrowsingDataEntries(
      allowed_browsing_data_model,
      {{kTestHost,
        data_key,
        {{BrowsingDataModel::StorageType::kSharedWorker},
         /*storage_size=*/0,
         /*cookie_count=*/0}}});
  ASSERT_EQ(allowed_browsing_data_model->size(), 1u);

  // Delete Shared Worker
  RemoveBrowsingDataForDataOwner(allowed_browsing_data_model, kTestHost);
  // Validate that the allowed browsing data model is empty.
  ValidateBrowsingDataEntries(allowed_browsing_data_model, {});
  ASSERT_EQ(allowed_browsing_data_model->size(), 0u);
}

IN_PROC_BROWSER_TEST_F(BrowsingDataModelBrowserTest,
                       LocalStorageRemovedBasedOnPartition) {
  // Build BDM from disk.
  std::unique_ptr<BrowsingDataModel> browsing_data_model =
      BuildBrowsingDataModel();
  ValidateBrowsingDataEntries(browsing_data_model.get(), {});

  // Navigate to a.test.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server()->GetURL(
                     kTestHost, "/browsing_data/embedded_site_data.html")));

  // Set local storage (a on a).
  SetDataForType("LocalStorage", content::ChildFrameAt(web_contents(), 0));

  // Navigate to b.test.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server()->GetURL(
                     kTestHost2, "/browsing_data/embedded_site_data.html")));

  // Set local storage (a on b).
  SetDataForType("LocalStorage", content::ChildFrameAt(web_contents(), 0));

  // Navigate to c.test.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server()->GetURL(
                     kTestHost3, "/browsing_data/embedded_site_data.html")));

  // Set local storage (a on c).
  SetDataForType("LocalStorage", content::ChildFrameAt(web_contents(), 0));

  //  Flush storage size to disk.
  auto* storage_partition = browser()->profile()->GetDefaultStoragePartition();
  storage_partition->Flush();

  // To ensure that flushing is completed.
  base::RunLoop().RunUntilIdle();

  auto* dom_storage_context = storage_partition->GetDOMStorageContext();

  // Fetch local storage size from backend.
  base::test::TestFuture<uint64_t> test_entry_storage_size[3];
  dom_storage_context->GetLocalStorageUsage(base::BindLambdaForTesting(
      [&](const std::vector<content::StorageUsageInfo>& storage_usage_info) {
        ASSERT_EQ(3U, storage_usage_info.size());
        test_entry_storage_size[0].SetValue(
            storage_usage_info[0].total_size_bytes);

        test_entry_storage_size[1].SetValue(
            storage_usage_info[1].total_size_bytes);

        test_entry_storage_size[2].SetValue(
            storage_usage_info[2].total_size_bytes);
      }));

  // Rebuild from disk.
  browsing_data_model = BuildBrowsingDataModel();

  auto testHostOrigin = https_test_server()->GetOrigin(kTestHost);
  auto top_level_site_a = net::SchemefulSite(GURL("https://a.test"));
  auto storage_key_a =
      blink::StorageKey::Create(testHostOrigin, top_level_site_a,
                                blink::mojom::AncestorChainBit::kSameSite);

  auto top_level_site_b = net::SchemefulSite(GURL("https://b.test"));
  auto storage_key_b =
      blink::StorageKey::Create(testHostOrigin, top_level_site_b,
                                blink::mojom::AncestorChainBit::kCrossSite);

  auto top_level_site_c = net::SchemefulSite(GURL("https://c.test"));
  auto storage_key_c =
      blink::StorageKey::Create(testHostOrigin, top_level_site_c,
                                blink::mojom::AncestorChainBit::kCrossSite);

  // Validate entries {{a on a}, {a on b}, {a on c}}.
  ValidateBrowsingDataEntries(
      browsing_data_model.get(),
      {{kTestHost,
        storage_key_a,
        {{BrowsingDataModel::StorageType::kLocalStorage},
         test_entry_storage_size[0].Get(),
         /*cookie_count=*/0}},
       {kTestHost,
        storage_key_b,
        {{BrowsingDataModel::StorageType::kLocalStorage},
         test_entry_storage_size[1].Get(),
         /*cookie_count=*/0}},
       {kTestHost,
        storage_key_c,
        {{BrowsingDataModel::StorageType::kLocalStorage},
         test_entry_storage_size[2].Get(),
         /*cookie_count=*/0}}});

  // Remove {a on b}.
  {
    base::RunLoop run_loop;
    browsing_data_model->RemovePartitionedBrowsingData(
        kTestHost, top_level_site_b, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Rebuild from disk.
  browsing_data_model = BuildBrowsingDataModel();

  // Validate entries {{a on a}, {a on c}}.
  ValidateBrowsingDataEntries(
      browsing_data_model.get(),
      {{kTestHost,
        storage_key_a,
        {{BrowsingDataModel::StorageType::kLocalStorage},
         test_entry_storage_size[0].Get(),
         /*cookie_count=*/0}},
       {kTestHost,
        storage_key_c,
        {{BrowsingDataModel::StorageType::kLocalStorage},
         test_entry_storage_size[2].Get(),
         /*cookie_count=*/0}}});

  // Remove {a on a}
  {
    base::RunLoop run_loop;
    browsing_data_model->RemoveUnpartitionedBrowsingData(
        kTestHost, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Rebuild from disk.
  browsing_data_model = BuildBrowsingDataModel();

  // Validate entries {{a on c}}.
  ValidateBrowsingDataEntries(
      browsing_data_model.get(),
      {{kTestHost,
        storage_key_c,
        {{BrowsingDataModel::StorageType::kLocalStorage},
         test_entry_storage_size[2].Get(),
         /*cookie_count=*/0}}});
}

IN_PROC_BROWSER_TEST_F(BrowsingDataModelBrowserTest,
                       SharedDictionaryHandledCorrectly) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_test_server()->GetURL(kTestHost, "/browsing_data/site_data.html")));
  // Ensure that there isn't any data fetched.
  std::unique_ptr<BrowsingDataModel> browsing_data_model =
      BuildBrowsingDataModel();
  ValidateBrowsingDataEntries(browsing_data_model.get(), {});
  ASSERT_EQ(browsing_data_model->size(), 0u);

  SetDataForType("SharedDictionary", web_contents());

  // Ensure that shared dictionary is fetched
  browsing_data_model = BuildBrowsingDataModel();

  // Validate that shared dictionary is fetched to browsing data model.
  url::Origin testOrigin = https_test_server()->GetOrigin(kTestHost);
  auto isolation_key = net::SharedDictionaryIsolationKey(
      testOrigin, net::SchemefulSite(testOrigin));
  ValidateBrowsingDataEntriesNonZeroUsage(
      browsing_data_model.get(),
      {{kTestHost,
        isolation_key,
        {{BrowsingDataModel::StorageType::kSharedDictionary},
         /*storage_size=*/0,
         /*cookie_count=*/0}}});
  ASSERT_EQ(browsing_data_model->size(), 1u);

  // Remove shared dictionary entry.
  RemoveBrowsingDataForDataOwner(browsing_data_model.get(), kTestHost);

  // Shared dictionary must have been removed.
  EXPECT_FALSE(HasDataForType("SharedDictionary", web_contents()));

  // Rebuild Browsing Data Model and verify entries are empty.
  browsing_data_model = BuildBrowsingDataModel();
  ValidateBrowsingDataEntries(browsing_data_model.get(), {});
  ASSERT_EQ(browsing_data_model->size(), 0u);
}

IN_PROC_BROWSER_TEST_F(BrowsingDataModelBrowserTest,
                       SharedDictionaryAccessReportedCorrectly) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_test_server()->GetURL(kTestHost, "/browsing_data/site_data.html")));
  auto* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  // Validate that the allowed browsing data model is empty.
  ValidateBrowsingDataEntries(content_settings->allowed_browsing_data_model(),
                              {});
  SetDataForType("SharedDictionary", web_contents());
  // Calling SetDataForType("SharedDictionary") registers a shared dictionary.
  // This must be reported to the data model.
  WaitForModelUpdate(content_settings->allowed_browsing_data_model(), 1);

  // Validate that the allowed browsing data model is populated with
  // SharedDictionary entry for `kTestHost`.
  url::Origin testOrigin = https_test_server()->GetOrigin(kTestHost);
  auto isolation_key = net::SharedDictionaryIsolationKey(
      testOrigin, net::SchemefulSite(testOrigin));
  ValidateBrowsingDataEntries(
      content_settings->allowed_browsing_data_model(),
      {{kTestHost,
        isolation_key,
        {{BrowsingDataModel::StorageType::kSharedDictionary},
         /*storage_size=*/0,
         /*cookie_count=*/0}}});

  // Navigate to about:blank to clear the browsing data model state.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  // Validate that the allowed browsing data model is empty.
  ValidateBrowsingDataEntries(
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame())
          ->allowed_browsing_data_model(),
      {});

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_test_server()->GetURL(kTestHost, "/browsing_data/site_data.html")));
  // Validate that the allowed browsing data model is empty.
  ValidateBrowsingDataEntries(
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame())
          ->allowed_browsing_data_model(),
      {});

  // Need this polling because the shared dictionary is used only if the
  // metadata database has been read when sending the HTTP request.
  while (!HasDataForType("SharedDictionary", web_contents())) {
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  }

  // Checking HasDataForType("SharedDictionary") accesses the registered
  // shared dictionary. This must be reported to the data model.
  content_settings = content_settings::PageSpecificContentSettings::GetForFrame(
      web_contents()->GetPrimaryMainFrame());
  WaitForModelUpdate(content_settings->allowed_browsing_data_model(), 1);

  // Validate that the allowed browsing data model is populated with
  // SharedDictionary entry for `kTestHost`.
  ValidateBrowsingDataEntries(
      content_settings->allowed_browsing_data_model(),
      {{kTestHost,
        isolation_key,
        {{BrowsingDataModel::StorageType::kSharedDictionary},
         /*storage_size=*/0,
         /*cookie_count=*/0}}});
}

IN_PROC_BROWSER_TEST_F(BrowsingDataModelBrowserTest,
                       SharedDictionaryAccessForNavigationReportedCorrectly) {
  // Registers a shared dictionary.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_test_server()->GetURL(kTestHost, "/browsing_data/site_data.html")));
  SetDataForType("SharedDictionary", web_contents());

  // Navigate to about:blank to clear the browsing data model state.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  // Validate that the allowed browsing data model is empty.
  ValidateBrowsingDataEntries(
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame())
          ->allowed_browsing_data_model(),
      {});

  // Need this polling because the shared dictionary is used only if the
  // metadata database has been read when sending the HTTP request.
  int retry = 0;
  while (true) {
    const std::string kExpectedResult =
        "This is compressed test data using a test dictionary";
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(),
        https_test_server()->GetURL(
            kTestHost,
            base::StringPrintf(
                "/shared_dictionary/path/compressed.data?retry=%d", ++retry))));
    const std::string innerText =
        EvalJs(web_contents()->GetPrimaryMainFrame(), "document.body.innerText")
            .ExtractString();
    if (innerText == kExpectedResult) {
      break;
    }
  }

  auto* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  WaitForModelUpdate(content_settings->allowed_browsing_data_model(), 1);

  // Validate that the allowed browsing data model is populated with
  // SharedDictionary entry for `kTestHost`.
  url::Origin testOrigin = https_test_server()->GetOrigin(kTestHost);
  auto isolation_key = net::SharedDictionaryIsolationKey(
      testOrigin, net::SchemefulSite(testOrigin));
  ValidateBrowsingDataEntries(
      content_settings->allowed_browsing_data_model(),
      {{kTestHost,
        isolation_key,
        {{BrowsingDataModel::StorageType::kSharedDictionary},
         /*storage_size=*/0,
         /*cookie_count=*/0}}});
}

IN_PROC_BROWSER_TEST_F(
    BrowsingDataModelBrowserTest,
    SharedDictionaryAccessForIframeNavigationReportedCorrectly) {
  // Registers a shared dictionary.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_test_server()->GetURL(kTestHost, "/browsing_data/site_data.html")));
  SetDataForType("SharedDictionary", web_contents());

  // Navigate to about:blank to clear the browsing data model state.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  // Validate that the allowed browsing data model is empty.
  ValidateBrowsingDataEntries(
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame())
          ->allowed_browsing_data_model(),
      {});
  // Return to the test page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_test_server()->GetURL(kTestHost, "/browsing_data/site_data.html")));
  // Validate that the allowed browsing data model is empty.
  ValidateBrowsingDataEntries(
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame())
          ->allowed_browsing_data_model(),
      {});

  // Need this polling because the shared dictionary is used only if the
  // metadata database has been read when sending the HTTP request.
  while (true) {
    const std::string kExpectedResult =
        "This is compressed test data using a test dictionary";
    const std::string iframeInnerText =
        EvalJs(web_contents()->GetPrimaryMainFrame(), R"(
    (async () => {
      const iframe = document.createElement('iframe');
      iframe.src = '/shared_dictionary/path/compressed.data';
      const promise =
          new Promise(resolve => { iframe.addEventListener('load', resolve); });
      document.body.appendChild(iframe);
      await promise;
      return iframe.contentDocument.body.innerText;
    })())")
            .ExtractString();
    if (iframeInnerText == kExpectedResult) {
      break;
    }
  }

  auto* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());
  WaitForModelUpdate(content_settings->allowed_browsing_data_model(), 1);

  // Validate that the allowed browsing data model is populated with
  // SharedDictionary entry for `kTestHost`.
  url::Origin testOrigin = https_test_server()->GetOrigin(kTestHost);
  auto isolation_key = net::SharedDictionaryIsolationKey(
      testOrigin, net::SchemefulSite(testOrigin));
  ValidateBrowsingDataEntries(
      content_settings->allowed_browsing_data_model(),
      {{kTestHost,
        isolation_key,
        {{BrowsingDataModel::StorageType::kSharedDictionary},
         /*storage_size=*/0,
         /*cookie_count=*/0}}});
}

IN_PROC_BROWSER_TEST_F(BrowsingDataModelBrowserTest, CookiesHandledCorrectly) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_test_server()->GetURL(kTestHost, "/browsing_data/site_data.html")));
  // Ensure that there isn't any data fetched.
  std::unique_ptr<BrowsingDataModel> browsing_data_model =
      BuildBrowsingDataModel();
  ValidateBrowsingDataEntries(browsing_data_model.get(), {});
  ASSERT_EQ(browsing_data_model->size(), 0u);

  SetDataForType("Cookie", web_contents());

  // Ensure that cookie is fetched.
  browsing_data_model = BuildBrowsingDataModel();

  // Validate that cookie is fetched to browsing data model.
  url::Origin testOrigin = https_test_server()->GetOrigin(kTestHost);
  std::unique_ptr<net::CanonicalCookie> data_key =
      net::CanonicalCookie::CreateForTesting(testOrigin.GetURL(),
                                             "foo=bar; Path=/browsing_data",
                                             base::Time::Now());
  ValidateBrowsingDataEntries(browsing_data_model.get(),
                              {{kTestHost,
                                *(data_key.get()),
                                {{BrowsingDataModel::StorageType::kCookie},
                                 /*storage_size=*/0,
                                 /*cookie_count=*/1}}});
  ASSERT_EQ(browsing_data_model->size(), 1u);

  // Remove cookie entry.
  RemoveBrowsingDataForDataOwner(browsing_data_model.get(), kTestHost);

  // Rebuild Browsing Data Model and verify entries are empty.
  browsing_data_model = BuildBrowsingDataModel();
  ValidateBrowsingDataEntries(browsing_data_model.get(), {});
  ASSERT_EQ(browsing_data_model->size(), 0u);
  ASSERT_FALSE(HasDataForType("Cookie", web_contents()));
}

IN_PROC_BROWSER_TEST_F(BrowsingDataModelBrowserTest,
                       CookiesAccessReportedCorrectly) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_test_server()->GetURL(kTestHost, "/browsing_data/site_data.html")));

  auto* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          web_contents()->GetPrimaryMainFrame());

  // Validate that the allowed browsing data model is empty.
  auto* allowed_browsing_data_model =
      content_settings->allowed_browsing_data_model();
  ValidateBrowsingDataEntries(allowed_browsing_data_model, {});
  ASSERT_EQ(allowed_browsing_data_model->size(), 0u);

  SetDataForType("Cookie", web_contents());
  WaitForModelUpdate(allowed_browsing_data_model, /*expected_size=*/1);

  // Validate that cookie is fetched to browsing data model.
  url::Origin testOrigin = https_test_server()->GetOrigin(kTestHost);
  std::unique_ptr<net::CanonicalCookie> data_key =
      net::CanonicalCookie::CreateForTesting(testOrigin.GetURL(),
                                             "foo=bar; Path=/browsing_data",
                                             base::Time::Now());
  ValidateBrowsingDataEntries(allowed_browsing_data_model,
                              {{kTestHost,
                                *(data_key.get()),
                                {{BrowsingDataModel::StorageType::kCookie},
                                 /*storage_size=*/0,
                                 /*cookie_count=*/1}}});
  ASSERT_EQ(allowed_browsing_data_model->size(), 1u);

  // Remove cookie entry.
  RemoveBrowsingDataForDataOwner(allowed_browsing_data_model, kTestHost);
  // Validate that the allowed browsing data model is empty.
  ValidateBrowsingDataEntries(allowed_browsing_data_model, {});
  ASSERT_EQ(allowed_browsing_data_model->size(), 0u);
  ASSERT_FALSE(HasDataForType("Cookie", web_contents()));
}

IN_PROC_BROWSER_TEST_F(BrowsingDataModelBrowserTest,
                       FederatedIdentityHandledCorrectly) {
  // Setup identity provider (IDP).
  idp_server()->SetConfigResponseDetails(BuildValidConfigDetails());

  // Navigate to test page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server()->GetURL(kTestHost, "/title1.html")));

  // Validate that the browsing data model built from disk is empty.
  std::unique_ptr<BrowsingDataModel> browsing_data_model =
      BuildBrowsingDataModel();
  ValidateBrowsingDataEntries(browsing_data_model.get(), {});
  ASSERT_EQ(browsing_data_model->size(), 0u);

  // Run FedCM and grant sharing permission by selecting an account.
  RunFedCm(web_contents(), https_test_server());

  // Waiting for the browsing data model to be populated, otherwise the test is
  // flaky.
  do {
    browsing_data_model = BuildBrowsingDataModel();
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  } while (browsing_data_model->size() != 1);

  // Validate that an entry for FederatedIdentity is added to the browsing data
  // model.
  url::Origin testRpOrigin = https_test_server()->GetOrigin(kTestHost);
  url::Origin testIdpOrigin =
      url::Origin::Create(GURL(GetIdpConfigUrl(https_test_server())));
  webid::FederatedIdentityDataModel::DataKey data_key{
      testRpOrigin, testRpOrigin, testIdpOrigin, kAccountId};
  ValidateBrowsingDataEntries(
      browsing_data_model.get(),
      {{kTestHost,
        data_key,
        {{static_cast<BrowsingDataModel::StorageType>(
             ChromeBrowsingDataModelDelegate::StorageType::kFederatedIdentity)},
         /*storage_size=*/100,
         /*cookie_count=*/0}}});

  // Clear FederatedIdentity in browsing data model using relying party embedder
  // (data owner).
  RemoveBrowsingDataForDataOwner(browsing_data_model.get(), kTestHost);

  // Rebuild browsing data model and verify entries are empty.
  browsing_data_model = BuildBrowsingDataModel();
  ValidateBrowsingDataEntries(browsing_data_model.get(), {});
}

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
IN_PROC_BROWSER_TEST_F(BrowsingDataModelBrowserTest,
                       CdmStorageHandledCorrectly) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_test_server()->GetURL(kTestHost, "/browsing_data/site_data.html")));
  // Ensure that there isn't any data fetched.
  std::unique_ptr<BrowsingDataModel> browsing_data_model =
      BuildBrowsingDataModel();
  ValidateBrowsingDataEntries(browsing_data_model.get(), {});
  ASSERT_EQ(browsing_data_model->size(), 0u);

  SetDataForType("MediaLicense", web_contents());
  browsing_data_model = BuildBrowsingDataModel();

  url::Origin testOrigin = https_test_server()->GetOrigin(kTestHost);
  auto storage_key = blink::StorageKey::CreateFirstParty(testOrigin);

  ValidateBrowsingDataEntries(browsing_data_model.get(),
                              {{kTestHost,
                                storage_key,
                                {{BrowsingDataModel::StorageType::kCdmStorage},
                                 /*storage_size=*/112,
                                 /*cookie_count=*/0}}});

  RemoveBrowsingDataForDataOwner(browsing_data_model.get(), kTestHost);

  // Rebuild browsing data model and verify entries are empty.
  browsing_data_model = BuildBrowsingDataModel();
  ValidateBrowsingDataEntries(browsing_data_model.get(), {});
}
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)
