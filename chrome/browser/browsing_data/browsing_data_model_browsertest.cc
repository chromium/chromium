// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browsing_data/content/browsing_data_model.h"
#include "components/browsing_data/content/browsing_data_model_test_util.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "services/network/test/trust_token_request_handler.h"
#include "services/network/test/trust_token_test_server_handler_registration.h"
#include "services/network/test/trust_token_test_util.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

namespace {

constexpr char kTestHost[] = "a.test";

void ProvideRequestHandlerKeyCommitmentsToNetworkService(
    base::StringPiece host,
    net::EmbeddedTestServer* https_server,
    const network::test::TrustTokenRequestHandler& request_handler) {
  base::flat_map<url::Origin, base::StringPiece> origins_and_commitments;
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
                       net::EmbeddedTestServer* https_server) {
  // join interest group
  auto command = content::JsReplace(
      R"(
    (async () => {
      try {
        navigator.joinAdInterestGroup(
            {
              name: 'cars',
              owner: $1,
              biddingLogicUrl: $2,
              trustedBiddingSignalsUrl: $3,
              trustedBiddingSignalsKeys: ['key1'],
              userBiddingSignals: {some: 'json', data: {here: [1, 2, 3]}},
              ads: [{
                renderUrl: $4,
                metadata: {ad: 'metadata', here: [1, 2, 3]},
              }],
            },
            /*joinDurationSec=*/ 1000);
      } catch (e) {
        return e.toString();
      }
      return "Success";
    })())",
      https_server->GetURL(kTestHost, "/"),
      https_server->GetURL(kTestHost, "/interest_group/bidding_logic.js"),
      https_server->GetURL(kTestHost,
                           "/interest_group/trusted_bidding_signals.json"),
      GURL("https://example.com/render"));
  EXPECT_EQ("Success", EvalJs(adapter, command));
}
}  // namespace

using browsing_data_model_test_util::ValidateBrowsingDataEntries;
using OperationResult = storage::SharedStorageDatabase::OperationResult;

class BrowsingDataModelBrowserTest : public InProcessBrowserTest {
 public:
  BrowsingDataModelBrowserTest() {
    auto& field_trial_param =
        network::features::kTrustTokenOperationsRequiringOriginTrial;
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{network::features::kPrivateStateTokens,
          {{field_trial_param.name,
            field_trial_param.GetName(
                network::features::TrustTokenOriginTrialSpec::
                    kOriginTrialNotRequired)}}},
         {features::kPrivacySandboxAdsAPIsOverride, {}},
         {blink::features::kSharedStorageAPI, {}},
         {blink::features::kInterestGroupStorage, {}},
         {blink::features::kAdInterestGroupAPI, {}},
         {blink::features::kFledge, {}},
         {blink::features::kFencedFrames, {}}},
        /*disabled_features=*/
        {});
  }

  ~BrowsingDataModelBrowserTest() override = default;

  void SetUpOnMainThread() override {
    PrivacySandboxSettingsFactory::GetForProfile(browser()->profile())
        ->SetAllPrivacySandboxAllowedForTesting();
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_->AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));
    network::test::RegisterTrustTokenTestHandlers(https_test_server(),
                                                  &request_handler_);
    ASSERT_TRUE(https_server_->Start());
  }

 protected:
  std::unique_ptr<BrowsingDataModel> BuildBrowsingDataModel() {
    base::test::TestFuture<std::unique_ptr<BrowsingDataModel>>
        browsing_data_model;
    BrowsingDataModel::BuildFromDisk(browser()->profile(),
                                     browsing_data_model.GetCallback());
    return browsing_data_model.Take();
  }

  content::StoragePartition* storage_partition() {
    return browser()->profile()->GetDefaultStoragePartition();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  net::EmbeddedTestServer* https_test_server() { return https_server_.get(); }

  GURL test_url() { return https_server_->GetURL(kTestHost, "/echo"); }

  network::test::TrustTokenRequestHandler request_handler_;

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

IN_PROC_BROWSER_TEST_F(BrowsingDataModelBrowserTest,
                       SharedStorageHandledCorrectly) {
  // Add origin shared storage.
  auto* shared_storage_manager = storage_partition()->GetSharedStorageManager();
  ASSERT_NE(nullptr, shared_storage_manager);

  base::test::TestFuture<OperationResult> future;
  url::Origin testOrigin = url::Origin::Create(GURL("https://a.test"));
  shared_storage_manager->Set(testOrigin, u"key", u"value",
                              future.GetCallback());
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
        blink::StorageKey(testOrigin),
        {BrowsingDataModel::StorageType::kSharedStorage,
         test_entry_storage_size.Get(), /*cookie_count=*/0}}});

  // Remove origin.
  {
    base::RunLoop run_loop;
    browsing_data_model.get()->RemoveBrowsingData(kTestHost,
                                                  run_loop.QuitClosure());
    run_loop.Run();
  }

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
  ValidateBrowsingDataEntries(content_settings->allowed_browsing_data_model(),
                              {{kTestHost,
                                blink::StorageKey(testOrigin),
                                {BrowsingDataModel::StorageType::kSharedStorage,
                                 /*storage_size=*/0, /*cookie_count=*/0}}});
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
      await fetch("/issue", {trustToken: {operation: 'token-request'}});
      return await document.hasPrivateToken($1, 'private-state-token');
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
        {BrowsingDataModel::StorageType::kTrustTokens, 100, 0}}});

  // Remove data for the host, and confirm the model updates appropriately.
  {
    base::RunLoop run_loop;
    browsing_data_model->RemoveBrowsingData(kTestHost,
                                            run_loop.QuitWhenIdleClosure());
    run_loop.Run();
  }

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

  // Join an interest group.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url()));
  JoinInterestGroup(web_contents(), https_test_server());

  // Waiting for the browsing data model to be populated, otherwise the test is
  // flaky.
  do {
    browsing_data_model = BuildBrowsingDataModel();
    base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  } while (std::distance(browsing_data_model->begin(),
                         browsing_data_model->end()) != 1);

  // Validate that an interest group is added.
  url::Origin testOrigin = https_test_server()->GetOrigin(kTestHost);
  content::InterestGroupManager::InterestGroupDataKey data_key{testOrigin,
                                                               testOrigin};
  ValidateBrowsingDataEntries(browsing_data_model.get(),
                              {{kTestHost,
                                data_key,
                                {BrowsingDataModel::StorageType::kInterestGroup,
                                 /*storage_size=*/1024, /*cookie_count=*/0}}});
  // Remove Interest Group.
  {
    base::RunLoop run_loop;
    browsing_data_model.get()->RemoveBrowsingData(kTestHost,
                                                  run_loop.QuitClosure());
    run_loop.Run();
  }

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

  // Join an interest group.
  JoinInterestGroup(web_contents(), https_test_server());
  while (std::distance(allowed_browsing_data_model->begin(),
                       allowed_browsing_data_model->end()) != 1) {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }

  // Validate that an interest group is reported to the browsing data model.
  url::Origin testOrigin = https_test_server()->GetOrigin(kTestHost);
  content::InterestGroupManager::InterestGroupDataKey data_key{testOrigin,
                                                               testOrigin};
  ValidateBrowsingDataEntries(allowed_browsing_data_model,
                              {{kTestHost,
                                data_key,
                                {BrowsingDataModel::StorageType::kInterestGroup,
                                 /*storage_size=*/0, /*cookie_count=*/0}}});
}
