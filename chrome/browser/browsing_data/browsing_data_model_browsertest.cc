// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_model_delegate.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browsing_data/content/browsing_data_model.h"
#include "components/browsing_data/content/browsing_data_model_test_util.h"
#include "components/browsing_data/core/features.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
#include "components/services/storage/shared_storage/shared_storage_manager.h"
#include "content/public/browser/attribution_data_model.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/test/trust_token_request_handler.h"
#include "services/network/test/trust_token_test_server_handler_registration.h"
#include "services/network/test/trust_token_test_util.h"
#include "third_party/blink/public/common/features.h"
#include "url/origin.h"

using base::test::FeatureRef;
using base::test::FeatureRefAndParams;

namespace {

constexpr char kTestHost[] = "a.test";
constexpr char kTestHost2[] = "b.test";

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
            decisionLogicUrl: $2,
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

void AddLocalStorageUsage(content::RenderFrameHost* render_frame_host,
                          int size) {
  auto command =
      content::JsReplace("localStorage.setItem('key', '!'.repeat($1))", size);
  EXPECT_TRUE(ExecJs(render_frame_host, command));
  base::RunLoop run_loop;
  render_frame_host->GetStoragePartition()->GetLocalStorageControl()->Flush(
      run_loop.QuitClosure());
  run_loop.Run();
}

void WaitForModelUpdate(BrowsingDataModel* model, size_t expected_size) {
  while (model->size() != expected_size) {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }
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
using browsing_data_model_test_util::ValidateBrowsingDataEntriesIgnoreUsage;
using OperationResult = storage::SharedStorageDatabase::OperationResult;

class BrowsingDataModelBrowserTest
    : public InProcessBrowserTest,
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
        {blink::features::kAdInterestGroupAPI, {}},
        {blink::features::kFledge, {}},
        {blink::features::kFencedFrames, {}},
        {blink::features::kBrowsingTopics, {}}};
    std::vector<FeatureRef> disabled_features = {};

    if (GetParam()) {
      enabled_features.push_back(
          {browsing_data::features::kDeprecateCookiesTreeModel, {}});
    } else {
      disabled_features.emplace_back(
          browsing_data::features::kDeprecateCookiesTreeModel);
    }

    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
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

  network::test::TrustTokenRequestHandler request_handler_;

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

IN_PROC_BROWSER_TEST_P(BrowsingDataModelBrowserTest,
                       SharedStorageHandledCorrectly) {
  // Add origin shared storage.
  auto* shared_storage_manager =
      default_storage_partition()->GetSharedStorageManager();
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
        blink::StorageKey::CreateFirstParty(testOrigin),
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

IN_PROC_BROWSER_TEST_P(BrowsingDataModelBrowserTest,
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
                                blink::StorageKey::CreateFirstParty(testOrigin),
                                {BrowsingDataModel::StorageType::kSharedStorage,
                                 /*storage_size=*/0, /*cookie_count=*/0}}});
}

IN_PROC_BROWSER_TEST_P(BrowsingDataModelBrowserTest, TrustTokenIssuance) {
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

IN_PROC_BROWSER_TEST_P(BrowsingDataModelBrowserTest,
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

IN_PROC_BROWSER_TEST_P(BrowsingDataModelBrowserTest,
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
  ValidateBrowsingDataEntries(allowed_browsing_data_model,
                              {{kTestHost,
                                data_key,
                                {BrowsingDataModel::StorageType::kInterestGroup,
                                 /*storage_size=*/0, /*cookie_count=*/0}}});
}

IN_PROC_BROWSER_TEST_P(BrowsingDataModelBrowserTest,
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
  ValidateBrowsingDataEntries(allowed_browsing_data_model,
                              {{kTestHost,
                                data_key,
                                {BrowsingDataModel::StorageType::kInterestGroup,
                                 /*storage_size=*/0, /*cookie_count=*/0}}});
}

IN_PROC_BROWSER_TEST_P(BrowsingDataModelBrowserTest,
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
                                                          register_url))
    );

    WaitForModelUpdate(allowed_browsing_data_model, 1);

    // Validate that an attribution reporting datakey is reported to the
    // browsing data model.
    url::Origin testOrigin = https_test_server()->GetOrigin(kTestHost);
    content::AttributionDataModel::DataKey data_key{testOrigin};
    ValidateBrowsingDataEntries(
        allowed_browsing_data_model,
        {{kTestHost,
          data_key,
          {BrowsingDataModel::StorageType::kAttributionReporting,
           /*storage_size=*/0, /*cookie_count=*/0}}});
  }
}

IN_PROC_BROWSER_TEST_P(BrowsingDataModelBrowserTest,
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
        {static_cast<BrowsingDataModel::StorageType>(
             ChromeBrowsingDataModelDelegate::StorageType::kTopics),
         /*storage_size=*/0, /*cookie_count=*/0}}});
  ASSERT_EQ(allowed_browsing_data_model->size(), 1u);

  // Clear Topic via BDM
  {
    base::RunLoop run_loop;
    allowed_browsing_data_model->RemoveBrowsingData(kTestHost,
                                                    run_loop.QuitClosure());
    run_loop.Run();
  }

  // Validate that the allowed browsing data model is cleared.
  ValidateBrowsingDataEntries(allowed_browsing_data_model, {});
  ASSERT_EQ(allowed_browsing_data_model->size(), 0u);
}

IN_PROC_BROWSER_TEST_P(BrowsingDataModelBrowserTest,
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
      {{iwa_url_info1.origin().host(),
        iwa_url_info1.origin(),
        {static_cast<BrowsingDataModel::StorageType>(
             ChromeBrowsingDataModelDelegate::StorageType::kIsolatedWebApp),
         /*storage_size=*/105, /*cookie_count=*/0}},
       {iwa_url_info2.origin().host(),
        iwa_url_info2.origin(),
        {static_cast<BrowsingDataModel::StorageType>(
             ChromeBrowsingDataModelDelegate::StorageType::kIsolatedWebApp),
         /*storage_size=*/505, /*cookie_count=*/0}}});
}

IN_PROC_BROWSER_TEST_P(BrowsingDataModelBrowserTest,
                       QuotaManagedDataHandledCorrectly) {
  // Ensure that there isn't any data fetched.
  std::unique_ptr<BrowsingDataModel> browsing_data_model =
      BuildBrowsingDataModel();
  ValidateBrowsingDataEntries(browsing_data_model.get(), {});
  ASSERT_EQ(browsing_data_model->size(), 0u);

  AccessStorage();

  // Ensure that quota data is fetched
  browsing_data_model = BuildBrowsingDataModel();
  bool is_cookies_tree_model_deprecated = GetParam();
  if (is_cookies_tree_model_deprecated) {
    // Validate that quota data is fetched to browsing data model.
    url::Origin testOrigin = https_test_server()->GetOrigin(kTestHost);
    auto data_key = blink::StorageKey::CreateFirstParty(testOrigin);
    ValidateBrowsingDataEntriesIgnoreUsage(
        browsing_data_model.get(),
        {{kTestHost,
          data_key,
          {BrowsingDataModel::StorageType::kUnpartitionedQuotaStorage,
           /*storage_size=*/0, /*cookie_count=*/0}}});

    ASSERT_EQ(browsing_data_model->size(), 1u);
  } else {
    ValidateBrowsingDataEntries(browsing_data_model.get(), {});
    ASSERT_EQ(browsing_data_model->size(), 0u);
  }
}

INSTANTIATE_TEST_SUITE_P(All, BrowsingDataModelBrowserTest, ::testing::Bool());
