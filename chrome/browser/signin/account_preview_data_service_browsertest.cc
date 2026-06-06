// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/account_preview_data_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/ui/browser.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_metrics_id_allocator.h"
#include "components/signin/core/browser/account_preview_data_service_impl.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/signin_constants.h"
#include "components/sync/base/data_type.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/url_loader_interceptor.h"
#include "google_apis/gaia/gaia_id.h"

namespace {

class AccountPreviewDataServiceBrowserTest : public SigninBrowserTestBase {
 public:
  AccountPreviewDataServiceBrowserTest() {
    feature_list_.InitAndEnableFeature(switches::kEnableAccountPreviewData);
  }
  ~AccountPreviewDataServiceBrowserTest() override = default;

  std::unique_ptr<content::URLLoaderInterceptor>
  CreateMockNetworkInterceptor() {
    return std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
        [](content::URLLoaderInterceptor::RequestParams* params) {
          if (params->url_request.url.path() ==
              "/v1/dataTypes/-/dataTypesStatistics") {
            std::string response = R"({
               "dataTypeStatistics": [
                 {
                   "name": "dataTypes/32904/dataTypeStatistics",
                   "count": "10"
                 },
                 {
                   "name": "dataTypes/45873/dataTypeStatistics",
                   "count": "5"
                 },
                 {
                   "name": "dataTypes/1164238/dataTypeStatistics",
                   "count": "3"
                 },
                 {
                   "name": "dataTypes/330441/dataTypeStatistics",
                   "count": "2"
                 }
               ]
             })";
            content::URLLoaderInterceptor::WriteResponse(
                "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n",
                response, params->client.get());
            return true;
          }
          if (params->url_request.url.path() ==
              "/v1/dataTypes/154522/entitiesPreviews") {
            std::string response = R"({
               "entitiesPreviews": []
             })";
            content::URLLoaderInterceptor::WriteResponse(
                "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n",
                response, params->client.get());
            return true;
          }
          return false;
        }));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AccountPreviewDataServiceBrowserTest, LogMetrics) {
  base::HistogramTester histogram_tester;

  // Setup URL interception.
  std::unique_ptr<content::URLLoaderInterceptor> interceptor =
      CreateMockNetworkInterceptor();

  auto* data_service = static_cast<signin::AccountPreviewDataServiceImpl*>(
      AccountPreviewDataServiceFactory::GetForProfile(GetProfile()));
  ASSERT_TRUE(data_service);

  base::RunLoop run_loop;
  data_service->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());

  signin::SetAutomaticIssueOfAccessTokens(
      identity_test_env()->identity_manager(), true);

  AccountInfo account_info =
      identity_test_env()->MakeAccountAvailable("user@example.com");

  account_info = AccountInfo::Builder(account_info)
                     .SetHostedDomain(signin::constants::kNoHostedDomainFound)
                     .SetIsChildAccount(signin::Tribool::kFalse)
                     .Build();
  identity_test_env()->UpdateAccountInfoForAccount(account_info);

  run_loop.Run();

  size_t profile_index = 0;

  std::string prefix = "Signin.SmartAccountSelection.OnSyncPreviewFetched.";
  std::string account_suffix = ".Account0";
  std::string profile_suffix = ".Profile" + base::NumberToString(profile_index);

  // Expect aggregated histograms.
  histogram_tester.ExpectUniqueSample(prefix + "IsManaged" + account_suffix,
                                      false, 1);
  histogram_tester.ExpectUniqueSample(prefix + "IsSupervised" + account_suffix,
                                      false, 1);
  histogram_tester.ExpectUniqueSample(prefix + "IsPrimary" + account_suffix,
                                      false, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SmartAccountSelection.OnSyncPreviewFetched.BOOKMARK" +
          account_suffix,
      10, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SmartAccountSelection.OnSyncPreviewFetched.PASSWORD" +
          account_suffix,
      5, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SmartAccountSelection.OnSyncPreviewFetched.AUTOFILL_WALLET_"
      "CREDENTIAL" +
          account_suffix,
      3, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SmartAccountSelection.OnSyncPreviewFetched.WALLET_METADATA" +
          account_suffix,
      2, 1);

  // Expect per-profile histograms.
  histogram_tester.ExpectUniqueSample(
      prefix + "IsManaged" + account_suffix + profile_suffix, false, 1);
  histogram_tester.ExpectUniqueSample(
      prefix + "IsSupervised" + account_suffix + profile_suffix, false, 1);
  histogram_tester.ExpectUniqueSample(
      prefix + "IsPrimary" + account_suffix + profile_suffix, false, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SmartAccountSelection.OnSyncPreviewFetched.BOOKMARK" +
          account_suffix + profile_suffix,
      10, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SmartAccountSelection.OnSyncPreviewFetched.PASSWORD" +
          account_suffix + profile_suffix,
      5, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SmartAccountSelection.OnSyncPreviewFetched.AUTOFILL_WALLET_"
      "CREDENTIAL" +
          account_suffix + profile_suffix,
      3, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SmartAccountSelection.OnSyncPreviewFetched.WALLET_METADATA" +
          account_suffix + profile_suffix,
      2, 1);
}

IN_PROC_BROWSER_TEST_F(AccountPreviewDataServiceBrowserTest,
                       LogMetricsProfileOverflow) {
  base::HistogramTester histogram_tester;

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile_path =
      profile_manager->user_data_dir().AppendASCII("Profile 20");
  Profile* profile2 =
      &profiles::testing::CreateProfileSync(profile_manager, profile_path);
  ASSERT_TRUE(profile2);

  // Setup URL interception.
  std::unique_ptr<content::URLLoaderInterceptor> interceptor =
      CreateMockNetworkInterceptor();

  auto* data_service = static_cast<signin::AccountPreviewDataServiceImpl*>(
      AccountPreviewDataServiceFactory::GetForProfile(profile2));
  ASSERT_TRUE(data_service);

  base::RunLoop run_loop;
  data_service->SetFetchCompleteCallbackForTesting(run_loop.QuitClosure());

  auto* identity_manager2 = IdentityManagerFactory::GetForProfile(profile2);
  signin::SetAutomaticIssueOfAccessTokens(identity_manager2, true);

  AccountInfo account_info =
      signin::MakeAccountAvailable(identity_manager2, "user@example.com");

  account_info = AccountInfo::Builder(account_info)
                     .SetHostedDomain(signin::constants::kNoHostedDomainFound)
                     .SetIsChildAccount(signin::Tribool::kFalse)
                     .Build();
  signin::UpdateAccountInfoForAccount(identity_manager2, account_info);

  run_loop.Run();

  std::string prefix = "Signin.SmartAccountSelection.OnSyncPreviewFetched.";
  std::string account_suffix = ".Account0";
  std::string profile_suffix = ".Profile20Plus";

  // Expect aggregated histograms.
  histogram_tester.ExpectUniqueSample(prefix + "IsManaged" + account_suffix,
                                      false, 1);
  histogram_tester.ExpectUniqueSample(prefix + "IsSupervised" + account_suffix,
                                      false, 1);
  histogram_tester.ExpectUniqueSample(prefix + "IsPrimary" + account_suffix,
                                      false, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SmartAccountSelection.OnSyncPreviewFetched.BOOKMARK" +
          account_suffix,
      10, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SmartAccountSelection.OnSyncPreviewFetched.PASSWORD" +
          account_suffix,
      5, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SmartAccountSelection.OnSyncPreviewFetched.AUTOFILL_WALLET_"
      "CREDENTIAL" +
          account_suffix,
      3, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SmartAccountSelection.OnSyncPreviewFetched.WALLET_METADATA" +
          account_suffix,
      2, 1);

  // Expect per-profile histograms.
  histogram_tester.ExpectUniqueSample(
      prefix + "IsManaged" + account_suffix + profile_suffix, false, 1);
  histogram_tester.ExpectUniqueSample(
      prefix + "IsSupervised" + account_suffix + profile_suffix, false, 1);
  histogram_tester.ExpectUniqueSample(
      prefix + "IsPrimary" + account_suffix + profile_suffix, false, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SmartAccountSelection.OnSyncPreviewFetched.BOOKMARK" +
          account_suffix + profile_suffix,
      10, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SmartAccountSelection.OnSyncPreviewFetched.PASSWORD" +
          account_suffix + profile_suffix,
      5, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SmartAccountSelection.OnSyncPreviewFetched.AUTOFILL_WALLET_"
      "CREDENTIAL" +
          account_suffix + profile_suffix,
      3, 1);
  histogram_tester.ExpectUniqueSample(
      "Signin.SmartAccountSelection.OnSyncPreviewFetched.WALLET_METADATA" +
          account_suffix + profile_suffix,
      2, 1);
}

}  // namespace
