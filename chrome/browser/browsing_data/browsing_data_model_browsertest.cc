// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browsing_data/content/browsing_data_model.h"
#include "components/browsing_data/content/browsing_data_model_test_util.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "services/network/test/trust_token_request_handler.h"
#include "services/network/test/trust_token_test_server_handler_registration.h"
#include "services/network/test/trust_token_test_util.h"

namespace {

constexpr char kTestHost[] = "a.test";

}

using browsing_data_model_test_util::ValidateBrowsingDataEntries;

class BrowsingDataModelTrustTokenBrowserTest : public InProcessBrowserTest {
 public:
  BrowsingDataModelTrustTokenBrowserTest() {
    auto& field_trial_param =
        network::features::kTrustTokenOperationsRequiringOriginTrial;
    feature_list_.InitWithFeaturesAndParameters(
        // Enabled Features:
        {{network::features::kTrustTokens,
          {{field_trial_param.name,
            field_trial_param.GetName(
                network::features::TrustTokenOriginTrialSpec::
                    kOriginTrialNotRequired)}}}},
        {});
  }
  ~BrowsingDataModelTrustTokenBrowserTest() override = default;

  void SetUpOnMainThread() override {
    PrivacySandboxSettingsFactory::GetForProfile(browser()->profile())
        ->SetPrivacySandboxEnabled(true);

    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_->AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    network::test::RegisterTrustTokenTestHandlers(https_server_.get(),
                                                  &request_handler_);
    ASSERT_TRUE(https_server_->Start());
  }

 protected:
  void ProvideRequestHandlerKeyCommitmentsToNetworkService(
      base::StringPiece host) {
    base::flat_map<url::Origin, base::StringPiece> origins_and_commitments;
    std::string key_commitments = request_handler_.GetKeyCommitmentRecord();

    GURL::Replacements replacements;
    replacements.SetHostStr(host);
    origins_and_commitments.insert_or_assign(
        url::Origin::Create(
            https_server_->base_url().ReplaceComponents(replacements)),
        key_commitments);

    base::RunLoop run_loop;
    content::GetNetworkService()->SetTrustTokenKeyCommitments(
        network::WrapKeyCommitmentsForIssuers(
            std::move(origins_and_commitments)),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
  net::EmbeddedTestServer* https_test_server() { return https_server_.get(); }

 private:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  network::test::TrustTokenRequestHandler request_handler_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowsingDataModelTrustTokenBrowserTest,
                       TrustTokenIssuance) {
  // Setup the test server to be able to issue trust tokens, and have it issue
  // some to the profile.
  ProvideRequestHandlerKeyCommitmentsToNetworkService(kTestHost);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server()->GetURL(kTestHost, "/title1.html")));

  std::string issuance_origin =
      url::Origin::Create(https_test_server()->GetURL(kTestHost, "/"))
          .Serialize();

  std::string command = content::JsReplace(R"(
  (async () => {
    try {
      await fetch("/issue", {trustToken: {type: 'token-request'}});
      return await document.hasTrustToken($1);
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
  std::unique_ptr<BrowsingDataModel> browsing_data_model;
  base::RunLoop run_loop;
  BrowsingDataModel::BuildFromDisk(
      browser()->profile(),
      base::BindLambdaForTesting([&](std::unique_ptr<BrowsingDataModel> model) {
        browsing_data_model = std::move(model);
        run_loop.QuitWhenIdle();
      }));
  run_loop.Run();

  ValidateBrowsingDataEntries(
      browsing_data_model.get(),
      {{kTestHost,
        https_test_server()->GetOrigin(kTestHost),
        {BrowsingDataModel::StorageType::kTrustTokens, 100, 0}}});

  // Remove data for the host, and confirm the model updates appropriately.
  base::RunLoop run_loop2;
  browsing_data_model->RemoveBrowsingData(kTestHost,
                                          run_loop2.QuitWhenIdleClosure());
  run_loop2.Run();

  ValidateBrowsingDataEntries(browsing_data_model.get(), {});

  // Build another model from disk, ensuring the data is no longer present.
  browsing_data_model.reset();
  base::RunLoop run_loop3;
  BrowsingDataModel::BuildFromDisk(
      browser()->profile(),
      base::BindLambdaForTesting([&](std::unique_ptr<BrowsingDataModel> model) {
        browsing_data_model = std::move(model);
        run_loop3.QuitWhenIdle();
      }));

  run_loop3.Run();
  ValidateBrowsingDataEntries(browsing_data_model.get(), {});
}
