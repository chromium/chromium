// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/print_content_analysis_utils.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/printing/print_preview_test.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "printing/printing_features.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/enterprise/connectors/analysis/fake_content_analysis_sdk_manager.h"  // nogncheck
#endif

namespace enterprise_connectors {

namespace {

ContentAnalysisDelegate* test_delegate_ = nullptr;

constexpr char kDmToken[] = "dm_token";

constexpr char kLocalPolicy[] = R"(
{
  "service_provider": "local_user_agent",
  "block_until_verdict": 1,
  "enable": [
    {
      "url_list": ["*"],
      "tags": ["dlp"]
    }
  ]
})";

// TODO(b/281087582): Add this once cloud is supported.
// constexpr char kCloudPolicy[] = R"(
//{
//  "service_provider": "google",
//  "block_until_verdict": 1,
//  "enable": [
//    {
//      "url_list": ["*"],
//      "tags": ["dlp"]
//    }
//  ]
//})";

constexpr char kScanId[] = "scan_id";

constexpr char kTestData[] = "lalilulelo";

constexpr char kPrinterName[] = "my_printer";

constexpr char kUserName[] = "test-user@chromium.org";

scoped_refptr<base::RefCountedMemory> CreateData() {
  return base::MakeRefCounted<base::RefCountedStaticMemory>(
      reinterpret_cast<const unsigned char*>(kTestData), sizeof(kTestData) - 1);
}

const std::set<std::string>* PrintMimeTypes() {
  static std::set<std::string> set = {""};
  return &set;
}

ContentAnalysisResponse::Result CreateResult(
    ContentAnalysisResponse::Result::TriggeredRule::Action action) {
  ContentAnalysisResponse::Result result;
  result.set_tag("dlp");
  result.set_status(ContentAnalysisResponse::Result::SUCCESS);

  if (action !=
      ContentAnalysisResponse::Result::TriggeredRule::ACTION_UNSPECIFIED) {
    auto* rule = result.add_triggered_rules();
    rule->set_rule_name("print_rule_name");
    rule->set_action(action);
  }
  return result;
}

ContentAnalysisResponse CreateResponse(
    ContentAnalysisResponse::Result::TriggeredRule::Action action) {
  ContentAnalysisResponse response;
  response.set_request_token(kScanId);

  auto* result = response.add_results();
  *result = CreateResult(action);
  return response;
}

class PrintTestContentAnalysisDelegate : public ContentAnalysisDelegate {
 public:
  PrintTestContentAnalysisDelegate(
      ContentAnalysisResponse::Result::TriggeredRule::Action action,
      content::WebContents* contents,
      ContentAnalysisDelegate::Data data,
      ContentAnalysisDelegate::CompletionCallback callback)
      : ContentAnalysisDelegate(contents,
                                std::move(data),
                                std::move(callback),
                                safe_browsing::DeepScanAccessPoint::PRINT),
        action_(action) {}

  static std::unique_ptr<ContentAnalysisDelegate> Create(
      ContentAnalysisResponse::Result::TriggeredRule::Action action,
      content::WebContents* contents,
      ContentAnalysisDelegate::Data data,
      ContentAnalysisDelegate::CompletionCallback callback) {
    auto delegate = std::make_unique<PrintTestContentAnalysisDelegate>(
        action, contents, std::move(data), std::move(callback));
    test_delegate_ = delegate.get();
    return delegate;
  }

 private:
  void UploadPageForDeepScanning(
      std::unique_ptr<safe_browsing::BinaryUploadService::Request> request)
      override {
    ASSERT_EQ(request->printer_name(), kPrinterName);
    PageRequestCallback(safe_browsing::BinaryUploadService::Result::SUCCESS,
                        CreateResponse(action_));
  }

  ContentAnalysisResponse::Result::TriggeredRule::Action action_;
};

class PrintContentAnalysisUtilsTest
    : public PrintPreviewTest,
      public testing::WithParamInterface<const char*> {
 public:
  PrintContentAnalysisUtilsTest() {
    scoped_feature_list_.InitAndEnableFeature(
        printing::features::kEnableLocalScanAfterPreview);
    ContentAnalysisDelegate::DisableUIForTesting();
  }

  void SetUp() override {
    PrintPreviewTest::SetUp();
    chrome::NewTab(browser());

    SetDMTokenForTesting(policy::DMToken::CreateValidToken(kDmToken));

    client_ = std::make_unique<policy::MockCloudPolicyClient>();

    extensions::SafeBrowsingPrivateEventRouterFactory::GetInstance()
        ->SetTestingFactory(
            profile(),
            base::BindRepeating([](content::BrowserContext* context) {
              return std::unique_ptr<KeyedService>(
                  new extensions::SafeBrowsingPrivateEventRouter(context));
            }));
    RealtimeReportingClientFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating([](content::BrowserContext* context) {
          return std::unique_ptr<KeyedService>(
              new RealtimeReportingClient(context));
        }));

    RealtimeReportingClientFactory::GetForProfile(profile())
        ->SetBrowserCloudPolicyClientForTesting(client_.get());
    identity_test_environment_.MakePrimaryAccountAvailable(
        kUserName, signin::ConsentLevel::kSync);
    RealtimeReportingClientFactory::GetForProfile(profile())
        ->SetIdentityManagerForTesting(
            identity_test_environment_.identity_manager());

    enterprise_connectors::test::SetAnalysisConnector(profile()->GetPrefs(),
                                                      PRINT, GetParam());
    test::SetOnSecurityEventReporting(profile()->GetPrefs(), true);
  }

  void TearDown() override {
    RealtimeReportingClientFactory::GetForProfile(profile())
        ->SetBrowserCloudPolicyClientForTesting(nullptr);
    SetDMTokenForTesting(policy::DMToken::CreateEmptyToken());
    PrintPreviewTest::TearDown();
  }

  content::WebContents* contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  signin::IdentityTestEnvironment identity_test_environment_;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // This installs a fake SDK manager that creates fake SDK clients when
  // its GetClient() method is called. This is needed so that calls to
  // ContentAnalysisSdkManager::Get()->GetClient() do not fail.
  FakeContentAnalysisSdkManager sdk_manager_;
#endif
};

}  // namespace

TEST_P(PrintContentAnalysisUtilsTest, Allowed) {
  ContentAnalysisDelegate::SetFactoryForTesting(base::BindRepeating(
      &PrintTestContentAnalysisDelegate::Create,
      ContentAnalysisResponse::Result::TriggeredRule::ACTION_UNSPECIFIED));

  test::EventReportValidator validator(client_.get());
  validator.ExpectNoReport();

  auto data = CreateData();
  base::RunLoop run_loop;
  auto on_verdict = base::BindLambdaForTesting([&run_loop](bool allowed) {
    ASSERT_TRUE(allowed);
    run_loop.Quit();
  });

  PrintIfAllowedByPolicy(data, contents(), kPrinterName, std::move(on_verdict),
                         /*hide_preview=*/base::DoNothing());
  run_loop.Run();
}

TEST_P(PrintContentAnalysisUtilsTest, ReportOnly) {
  ContentAnalysisDelegate::SetFactoryForTesting(base::BindRepeating(
      &PrintTestContentAnalysisDelegate::Create,
      ContentAnalysisResponse::Result::TriggeredRule::REPORT_ONLY));

  test::EventReportValidator validator(client_.get());
  validator.ExpectSensitiveDataEvent(
      /*url*/ "",
      /*source*/ "",
      /*destination*/ kPrinterName,
      /*filename*/ "New Tab",
      /*sha*/ "",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerPagePrint,
      /*dlp_verdict*/
      CreateResult(ContentAnalysisResponse::Result::TriggeredRule::REPORT_ONLY),
      /*mimetype*/ PrintMimeTypes(),
      /*size*/ absl::nullopt,
      /*result*/
      safe_browsing::EventResultToString(safe_browsing::EventResult::ALLOWED),
      /*username*/ kUserName,
      /*profile_identifier*/ profile()->GetPath().AsUTF8Unsafe(),
      /*scan_id*/ kScanId);

  auto data = CreateData();
  base::RunLoop run_loop;
  auto on_verdict = base::BindLambdaForTesting([&run_loop](bool allowed) {
    ASSERT_TRUE(allowed);
    run_loop.Quit();
  });

  PrintIfAllowedByPolicy(data, contents(), kPrinterName, std::move(on_verdict),
                         /*hide_preview=*/base::DoNothing());
  run_loop.Run();
}

TEST_P(PrintContentAnalysisUtilsTest, WarnThenCancel) {
  ContentAnalysisDelegate::SetFactoryForTesting(base::BindRepeating(
      &PrintTestContentAnalysisDelegate::Create,
      ContentAnalysisResponse::Result::TriggeredRule::WARN));

  test::EventReportValidator validator(client_.get());
  validator.SetDoneClosure(base::BindLambdaForTesting([this, &validator]() {
    testing::Mock::VerifyAndClearExpectations(client_.get());
    validator.ExpectNoReport();
    ASSERT_TRUE(test_delegate_);
    test_delegate_->Cancel(/*warning=*/true);
  }));
  validator.ExpectSensitiveDataEvent(
      /*url*/ "",
      /*source*/ "",
      /*destination*/ kPrinterName,
      /*filename*/ "New Tab",
      /*sha*/ "",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerPagePrint,
      /*dlp_verdict*/
      CreateResult(ContentAnalysisResponse::Result::TriggeredRule::WARN),
      /*mimetype*/ PrintMimeTypes(),
      /*size*/ absl::nullopt,
      /*result*/
      safe_browsing::EventResultToString(safe_browsing::EventResult::WARNED),
      /*username*/ kUserName,
      /*profile_identifier*/ profile()->GetPath().AsUTF8Unsafe(),
      /*scan_id*/ kScanId);

  auto data = CreateData();
  base::RunLoop run_loop;
  auto on_verdict = base::BindLambdaForTesting([&run_loop](bool allowed) {
    ASSERT_FALSE(allowed);
    run_loop.Quit();
  });

  PrintIfAllowedByPolicy(data, contents(), kPrinterName, std::move(on_verdict),
                         /*hide_preview=*/base::DoNothing());
  run_loop.Run();
}

TEST_P(PrintContentAnalysisUtilsTest, WarnedThenBypass) {
  ContentAnalysisDelegate::SetFactoryForTesting(base::BindRepeating(
      &PrintTestContentAnalysisDelegate::Create,
      ContentAnalysisResponse::Result::TriggeredRule::WARN));

  bool bypassed = false;
  test::EventReportValidator validator(client_.get());
  validator.SetDoneClosure(base::BindLambdaForTesting([this, &validator,
                                                       &bypassed]() {
    // Only do this once to avoid infinite recursion since bypassing triggers
    // the "normal" report which gets caught by this repeated callback.
    if (!bypassed) {
      bypassed = true;
      testing::Mock::VerifyAndClearExpectations(client_.get());
      validator.ExpectSensitiveDataEvent(
          /*url*/ "",
          /*source*/ "",
          /*destination*/ kPrinterName,
          /*filename*/ "New Tab",
          /*sha*/ "",
          /*trigger*/
          extensions::SafeBrowsingPrivateEventRouter::kTriggerPagePrint,
          /*dlp_verdict*/
          CreateResult(ContentAnalysisResponse::Result::TriggeredRule::WARN),
          /*mimetype*/ PrintMimeTypes(),
          /*size*/ absl::nullopt,
          /*result*/
          safe_browsing::EventResultToString(
              safe_browsing::EventResult::BYPASSED),
          /*username*/ kUserName,
          /*profile_identifier*/ profile()->GetPath().AsUTF8Unsafe(),
          /*scan_id*/ kScanId);
      ASSERT_TRUE(test_delegate_);
      test_delegate_->SetPageWarningForTesting(
          CreateResponse(ContentAnalysisResponse::Result::TriggeredRule::WARN));
      test_delegate_->BypassWarnings(absl::nullopt);
    }
  }));

  validator.ExpectSensitiveDataEvent(
      /*url*/ "",
      /*source*/ "",
      /*destination*/ kPrinterName,
      /*filename*/ "New Tab",
      /*sha*/ "",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerPagePrint,
      /*dlp_verdict*/
      CreateResult(ContentAnalysisResponse::Result::TriggeredRule::WARN),
      /*mimetype*/ PrintMimeTypes(),
      /*size*/ absl::nullopt,
      /*result*/
      safe_browsing::EventResultToString(safe_browsing::EventResult::WARNED),
      /*username*/ kUserName,
      /*profile_identifier*/ profile()->GetPath().AsUTF8Unsafe(),
      /*scan_id*/ kScanId);

  auto data = CreateData();
  base::RunLoop run_loop;
  auto on_verdict = base::BindLambdaForTesting([&run_loop](bool allowed) {
    ASSERT_TRUE(allowed);
    run_loop.Quit();
  });

  PrintIfAllowedByPolicy(data, contents(), kPrinterName, std::move(on_verdict),
                         /*hide_preview=*/base::DoNothing());
  run_loop.Run();
}

TEST_P(PrintContentAnalysisUtilsTest, Blocked) {
  ContentAnalysisDelegate::SetFactoryForTesting(base::BindRepeating(
      &PrintTestContentAnalysisDelegate::Create,
      ContentAnalysisResponse::Result::TriggeredRule::BLOCK));

  test::EventReportValidator validator(client_.get());
  validator.ExpectSensitiveDataEvent(
      /*url*/ "",
      /*source*/ "",
      /*destination*/ kPrinterName,
      /*filename*/ "New Tab",
      /*sha*/ "",
      /*trigger*/
      extensions::SafeBrowsingPrivateEventRouter::kTriggerPagePrint,
      /*dlp_verdict*/
      CreateResult(ContentAnalysisResponse::Result::TriggeredRule::BLOCK),
      /*mimetype*/ PrintMimeTypes(),
      /*size*/ absl::nullopt,
      /*result*/
      safe_browsing::EventResultToString(safe_browsing::EventResult::BLOCKED),
      /*username*/ kUserName,
      /*profile_identifier*/ profile()->GetPath().AsUTF8Unsafe(),
      /*scan_id*/ kScanId);

  auto data = CreateData();
  base::RunLoop run_loop;
  auto on_verdict = base::BindLambdaForTesting([&run_loop](bool allowed) {
    ASSERT_FALSE(allowed);
    run_loop.Quit();
  });

  PrintIfAllowedByPolicy(data, contents(), kPrinterName, std::move(on_verdict),
                         /*hide_preview=*/base::DoNothing());
  run_loop.Run();
}

// TODO(b/281087582): Add the cloud value, aka:
// testing::Values(kLocalPolicy, kCloudPolicy)
INSTANTIATE_TEST_SUITE_P(All,
                         PrintContentAnalysisUtilsTest,
                         testing::Values(kLocalPolicy));

}  // namespace enterprise_connectors
