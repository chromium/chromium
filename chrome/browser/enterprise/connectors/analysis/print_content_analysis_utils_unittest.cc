// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/print_content_analysis_utils.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
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
#include "components/enterprise/buildflags/buildflags.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "printing/printing_features.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
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

constexpr char kCloudPolicy[] = R"(
{
  "service_provider": "google",
  "block_until_verdict": 1,
  "enable": [
    {
      "url_list": ["*"],
      "tags": ["dlp"]
    }
  ]
})";

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
      public testing::WithParamInterface<
          testing::tuple<const char*, bool, bool>> {
 public:
  PrintContentAnalysisUtilsTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (local_scan_after_preview_feature_enabled()) {
      enabled_features.push_back(
          {printing::features::kEnableLocalScanAfterPreview, {}});
    } else {
      disabled_features.push_back(
          printing::features::kEnableLocalScanAfterPreview);
    }
    if (cloud_scan_after_preview_feature_enabled()) {
      enabled_features.push_back(
          {printing::features::kEnableCloudScanAfterPreview, {}});
    } else {
      disabled_features.push_back(
          printing::features::kEnableCloudScanAfterPreview);
    }
    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
    ContentAnalysisDelegate::DisableUIForTesting();
  }

  const char* policy_value() const { return std::get<0>(GetParam()); }
  bool local_scan_after_preview_feature_enabled() const {
    return std::get<1>(GetParam());
  }
  bool cloud_scan_after_preview_feature_enabled() const {
    return std::get<2>(GetParam());
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
                                                      PRINT, policy_value());
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

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

  // Helper that returns true when the policy/feature values should return in
  // pre-dialog content analysis.
  bool ExpectPostDialogAnalysis() {
    return (policy_value() == kLocalPolicy &&
            local_scan_after_preview_feature_enabled()) ||
           (policy_value() == kCloudPolicy &&
            cloud_scan_after_preview_feature_enabled());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  signin::IdentityTestEnvironment identity_test_environment_;
  base::HistogramTester histogram_tester_;

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
  // This installs a fake SDK manager that creates fake SDK clients when
  // its GetClient() method is called. This is needed so that calls to
  // ContentAnalysisSdkManager::Get()->GetClient() do not fail.
  FakeContentAnalysisSdkManager sdk_manager_;
#endif
};

}  // namespace

TEST_P(PrintContentAnalysisUtilsTest, GetPrintAnalysisData_BeforeSystemDialog) {
  auto data = GetPrintAnalysisData(contents(),
                                   PrintScanningContext::kBeforeSystemDialog);

  ASSERT_TRUE(data);
  ASSERT_EQ(data->settings.block_until_verdict, BlockUntilVerdict::kBlock);
  ASSERT_EQ(data->reason, ContentAnalysisRequest::SYSTEM_DIALOG_PRINT);

  ASSERT_EQ(policy_value() == kLocalPolicy,
            data->settings.cloud_or_local_settings.is_local_analysis());
  ASSERT_EQ(policy_value() == kCloudPolicy,
            data->settings.cloud_or_local_settings.is_cloud_analysis());

  if (policy_value() == kLocalPolicy) {
    histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType",
                                        1);
    histogram_tester().ExpectUniqueSample(
        "Enterprise.OnPrint.Local.PrintType",
        PrintScanningContext::kBeforeSystemDialog, 1);
    histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType",
                                        0);
  } else {
    histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType",
                                        1);
    histogram_tester().ExpectUniqueSample(
        "Enterprise.OnPrint.Cloud.PrintType",
        PrintScanningContext::kBeforeSystemDialog, 1);
    histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType",
                                        0);
  }
}

TEST_P(PrintContentAnalysisUtilsTest, GetPrintAnalysisData_BeforePreview) {
  auto data =
      GetPrintAnalysisData(contents(), PrintScanningContext::kBeforePreview);

  if (ExpectPostDialogAnalysis()) {
    ASSERT_FALSE(data);
    histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType",
                                        0);
    histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType",
                                        0);
  } else {
    ASSERT_TRUE(data);
    ASSERT_EQ(data->settings.block_until_verdict, BlockUntilVerdict::kBlock);
    ASSERT_EQ(data->reason, ContentAnalysisRequest::PRINT_PREVIEW_PRINT);

    ASSERT_EQ(policy_value() == kLocalPolicy,
              data->settings.cloud_or_local_settings.is_local_analysis());
    ASSERT_EQ(policy_value() == kCloudPolicy,
              data->settings.cloud_or_local_settings.is_cloud_analysis());

    if (policy_value() == kLocalPolicy) {
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType",
                                          1);
      histogram_tester().ExpectUniqueSample(
          "Enterprise.OnPrint.Local.PrintType",
          PrintScanningContext::kBeforePreview, 1);
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType",
                                          0);
    } else {
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType",
                                          1);
      histogram_tester().ExpectUniqueSample(
          "Enterprise.OnPrint.Cloud.PrintType",
          PrintScanningContext::kBeforePreview, 1);
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType",
                                          0);
    }
  }
}

TEST_P(PrintContentAnalysisUtilsTest,
       GetPrintAnalysisData_SystemPrintAfterPreview) {
  auto data = GetPrintAnalysisData(
      contents(), PrintScanningContext::kSystemPrintAfterPreview);

  // This enum value should never return a populated `data` since scanning
  // should either take place before the system dialog with the
  // `kBeforeSystemDialog` context, or right after it with the
  // `kSystemPrintBeforePrintDocument` context.
  ASSERT_FALSE(data);

  histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType", 0);
  histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType", 0);
}

TEST_P(PrintContentAnalysisUtilsTest,
       GetPrintAnalysisData_NormalPrintAfterPreview) {
  auto data = GetPrintAnalysisData(
      contents(), PrintScanningContext::kNormalPrintAfterPreview);

  if (ExpectPostDialogAnalysis()) {
    ASSERT_TRUE(data);
    ASSERT_EQ(data->settings.block_until_verdict, BlockUntilVerdict::kBlock);
    ASSERT_EQ(data->reason, ContentAnalysisRequest::PRINT_PREVIEW_PRINT);

    ASSERT_EQ(policy_value() == kLocalPolicy,
              data->settings.cloud_or_local_settings.is_local_analysis());
    ASSERT_EQ(policy_value() == kCloudPolicy,
              data->settings.cloud_or_local_settings.is_cloud_analysis());

    if (policy_value() == kLocalPolicy) {
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType",
                                          1);
      histogram_tester().ExpectUniqueSample(
          "Enterprise.OnPrint.Local.PrintType",
          PrintScanningContext::kNormalPrintAfterPreview, 1);
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType",
                                          0);
    } else {
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType",
                                          1);
      histogram_tester().ExpectUniqueSample(
          "Enterprise.OnPrint.Cloud.PrintType",
          PrintScanningContext::kNormalPrintAfterPreview, 1);
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType",
                                          0);
    }
  } else {
    ASSERT_FALSE(data);
    histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType",
                                        0);
    histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType",
                                        0);
  }
}

TEST_P(PrintContentAnalysisUtilsTest,
       GetPrintAnalysisData_NormalPrintBeforePrintDocument) {
  auto data = GetPrintAnalysisData(
      contents(), PrintScanningContext::kNormalPrintBeforePrintDocument);

  // This enum value should never return a populated `data` since scanning
  // should either take place before the preview dialog with the
  // `kBeforePreview` context, or right after it with the
  // `kNormalPrintAfterPreview` context.
  ASSERT_FALSE(data);

  histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType", 0);
  histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType", 0);
}

TEST_P(PrintContentAnalysisUtilsTest,
       GetPrintAnalysisData_SystemPrintBeforePrintDocument) {
  auto data = GetPrintAnalysisData(
      contents(), PrintScanningContext::kSystemPrintBeforePrintDocument);

  if (ExpectPostDialogAnalysis()) {
    ASSERT_TRUE(data);
    ASSERT_EQ(data->settings.block_until_verdict, BlockUntilVerdict::kBlock);
    ASSERT_EQ(data->reason, ContentAnalysisRequest::SYSTEM_DIALOG_PRINT);

    ASSERT_EQ(policy_value() == kLocalPolicy,
              data->settings.cloud_or_local_settings.is_local_analysis());
    ASSERT_EQ(policy_value() == kCloudPolicy,
              data->settings.cloud_or_local_settings.is_cloud_analysis());

    if (policy_value() == kLocalPolicy) {
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType",
                                          1);
      histogram_tester().ExpectUniqueSample(
          "Enterprise.OnPrint.Local.PrintType",
          PrintScanningContext::kSystemPrintBeforePrintDocument, 1);
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType",
                                          0);
    } else {
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType",
                                          1);
      histogram_tester().ExpectUniqueSample(
          "Enterprise.OnPrint.Cloud.PrintType",
          PrintScanningContext::kSystemPrintBeforePrintDocument, 1);
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType",
                                          0);
    }
  } else {
    ASSERT_FALSE(data);
    histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType",
                                        0);
    histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType",
                                        0);
  }
}

#if BUILDFLAG(IS_MAC)
TEST_P(PrintContentAnalysisUtilsTest,
       GetPrintAnalysisData_MacOpenPdfInPreview) {
  auto data =
      GetPrintAnalysisData(contents(), PrintScanningContext::kOpenPdfInPreview);

  if (ExpectPostDialogAnalysis()) {
    ASSERT_TRUE(data);
    ASSERT_EQ(data->settings.block_until_verdict, BlockUntilVerdict::kBlock);
    ASSERT_EQ(data->reason, ContentAnalysisRequest::PRINT_PREVIEW_PRINT);

    ASSERT_EQ(policy_value() == kLocalPolicy,
              data->settings.cloud_or_local_settings.is_local_analysis());
    ASSERT_EQ(policy_value() == kCloudPolicy,
              data->settings.cloud_or_local_settings.is_cloud_analysis());

    if (policy_value() == kLocalPolicy) {
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType",
                                          1);
      histogram_tester().ExpectUniqueSample(
          "Enterprise.OnPrint.Local.PrintType",
          PrintScanningContext::kOpenPdfInPreview, 1);
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType",
                                          0);
    } else {
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType",
                                          1);
      histogram_tester().ExpectUniqueSample(
          "Enterprise.OnPrint.Cloud.PrintType",
          PrintScanningContext::kOpenPdfInPreview, 1);
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType",
                                          0);
    }
  } else {
    ASSERT_FALSE(data);
    histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType",
                                        0);
    histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType",
                                        0);
  }
}
#endif  // BUILDFLAG(IS_MAC)

TEST_P(PrintContentAnalysisUtilsTest, PrintIfAllowedByPolicyAllowed) {
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

  PrintIfAllowedByPolicy(data, contents(), kPrinterName,
                         PrintScanningContext::kNormalPrintAfterPreview,
                         std::move(on_verdict),
                         /*hide_preview=*/base::DoNothing());
  run_loop.Run();

  if (ExpectPostDialogAnalysis()) {
    if (policy_value() == kLocalPolicy) {
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType",
                                          1);
      histogram_tester().ExpectUniqueSample(
          "Enterprise.OnPrint.Local.PrintType",
          PrintScanningContext::kNormalPrintAfterPreview, 1);
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType",
                                          0);
    } else {
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType",
                                          1);
      histogram_tester().ExpectUniqueSample(
          "Enterprise.OnPrint.Cloud.PrintType",
          PrintScanningContext::kNormalPrintAfterPreview, 1);
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType",
                                          0);
    }
  } else {
    histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType",
                                        0);
    histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType",
                                        0);
  }
}

TEST_P(PrintContentAnalysisUtilsTest, PrintIfAllowedByPolicyReportOnly) {
  ContentAnalysisDelegate::SetFactoryForTesting(base::BindRepeating(
      &PrintTestContentAnalysisDelegate::Create,
      ContentAnalysisResponse::Result::TriggeredRule::REPORT_ONLY));

  test::EventReportValidator validator(client_.get());
  if (ExpectPostDialogAnalysis()) {
    validator.ExpectSensitiveDataEvent(
        /*url*/ "",
        /*source*/ "",
        /*destination*/ kPrinterName,
        /*filename*/ "New Tab",
        /*sha*/ "",
        /*trigger*/
        extensions::SafeBrowsingPrivateEventRouter::kTriggerPagePrint,
        /*dlp_verdict*/
        CreateResult(
            ContentAnalysisResponse::Result::TriggeredRule::REPORT_ONLY),
        /*mimetype*/ PrintMimeTypes(),
        /*size*/ absl::nullopt,
        /*result*/
        safe_browsing::EventResultToString(safe_browsing::EventResult::ALLOWED),
        /*username*/ kUserName,
        /*profile_identifier*/ profile()->GetPath().AsUTF8Unsafe(),
        /*scan_id*/ kScanId);
  } else {
    validator.ExpectNoReport();
  }

  auto data = CreateData();
  base::RunLoop run_loop;
  auto on_verdict = base::BindLambdaForTesting([&run_loop](bool allowed) {
    ASSERT_TRUE(allowed);
    run_loop.Quit();
  });

  PrintIfAllowedByPolicy(data, contents(), kPrinterName,
                         PrintScanningContext::kNormalPrintAfterPreview,
                         std::move(on_verdict),
                         /*hide_preview=*/base::DoNothing());
  run_loop.Run();

  if (ExpectPostDialogAnalysis()) {
    if (policy_value() == kLocalPolicy) {
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType",
                                          1);
      histogram_tester().ExpectUniqueSample(
          "Enterprise.OnPrint.Local.PrintType",
          PrintScanningContext::kNormalPrintAfterPreview, 1);
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType",
                                          0);
    } else {
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType",
                                          1);
      histogram_tester().ExpectUniqueSample(
          "Enterprise.OnPrint.Cloud.PrintType",
          PrintScanningContext::kNormalPrintAfterPreview, 1);
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType",
                                          0);
    }
  } else {
    histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType",
                                        0);
    histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType",
                                        0);
  }
}

TEST_P(PrintContentAnalysisUtilsTest, PrintIfAllowedByPolicyWarnThenCancel) {
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
  if (ExpectPostDialogAnalysis()) {
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
  } else {
    validator.ExpectNoReport();
  }

  auto data = CreateData();
  base::RunLoop run_loop;
  auto on_verdict = base::BindLambdaForTesting([this, &run_loop](bool allowed) {
    ASSERT_NE(ExpectPostDialogAnalysis(), allowed);
    run_loop.Quit();
  });

  PrintIfAllowedByPolicy(data, contents(), kPrinterName,
                         PrintScanningContext::kNormalPrintAfterPreview,
                         std::move(on_verdict),
                         /*hide_preview=*/base::DoNothing());
  run_loop.Run();

  if (ExpectPostDialogAnalysis()) {
    if (policy_value() == kLocalPolicy) {
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType",
                                          1);
      histogram_tester().ExpectUniqueSample(
          "Enterprise.OnPrint.Local.PrintType",
          PrintScanningContext::kNormalPrintAfterPreview, 1);
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType",
                                          0);
    } else {
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType",
                                          1);
      histogram_tester().ExpectUniqueSample(
          "Enterprise.OnPrint.Cloud.PrintType",
          PrintScanningContext::kNormalPrintAfterPreview, 1);
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType",
                                          0);
    }
  } else {
    histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType",
                                        0);
    histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType",
                                        0);
  }
}

TEST_P(PrintContentAnalysisUtilsTest, PrintIfAllowedByPolicyWarnedThenBypass) {
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

  if (ExpectPostDialogAnalysis()) {
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
  } else {
    validator.ExpectNoReport();
  }

  auto data = CreateData();
  base::RunLoop run_loop;
  auto on_verdict = base::BindLambdaForTesting([&run_loop](bool allowed) {
    ASSERT_TRUE(allowed);
    run_loop.Quit();
  });

  PrintIfAllowedByPolicy(data, contents(), kPrinterName,
                         PrintScanningContext::kNormalPrintAfterPreview,
                         std::move(on_verdict),
                         /*hide_preview=*/base::DoNothing());
  run_loop.Run();

  if (ExpectPostDialogAnalysis()) {
    if (policy_value() == kLocalPolicy) {
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType",
                                          1);
      histogram_tester().ExpectUniqueSample(
          "Enterprise.OnPrint.Local.PrintType",
          PrintScanningContext::kNormalPrintAfterPreview, 1);
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType",
                                          0);
    } else {
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType",
                                          1);
      histogram_tester().ExpectUniqueSample(
          "Enterprise.OnPrint.Cloud.PrintType",
          PrintScanningContext::kNormalPrintAfterPreview, 1);
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType",
                                          0);
    }
  } else {
    histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType",
                                        0);
    histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType",
                                        0);
  }
}

TEST_P(PrintContentAnalysisUtilsTest, PrintIfAllowedByPolicyBlocked) {
  ContentAnalysisDelegate::SetFactoryForTesting(base::BindRepeating(
      &PrintTestContentAnalysisDelegate::Create,
      ContentAnalysisResponse::Result::TriggeredRule::BLOCK));

  test::EventReportValidator validator(client_.get());
  if (ExpectPostDialogAnalysis()) {
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
  } else {
    validator.ExpectNoReport();
  }

  auto data = CreateData();
  base::RunLoop run_loop;
  auto on_verdict = base::BindLambdaForTesting([this, &run_loop](bool allowed) {
    ASSERT_NE(ExpectPostDialogAnalysis(), allowed);
    run_loop.Quit();
  });

  PrintIfAllowedByPolicy(data, contents(), kPrinterName,
                         PrintScanningContext::kNormalPrintAfterPreview,
                         std::move(on_verdict),
                         /*hide_preview=*/base::DoNothing());
  run_loop.Run();

  if (ExpectPostDialogAnalysis()) {
    if (policy_value() == kLocalPolicy) {
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType",
                                          1);
      histogram_tester().ExpectUniqueSample(
          "Enterprise.OnPrint.Local.PrintType",
          PrintScanningContext::kNormalPrintAfterPreview, 1);
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType",
                                          0);
    } else {
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType",
                                          1);
      histogram_tester().ExpectUniqueSample(
          "Enterprise.OnPrint.Cloud.PrintType",
          PrintScanningContext::kNormalPrintAfterPreview, 1);
      histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType",
                                          0);
    }
  } else {
    histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Local.PrintType",
                                        0);
    histogram_tester().ExpectTotalCount("Enterprise.OnPrint.Cloud.PrintType",
                                        0);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PrintContentAnalysisUtilsTest,
    testing::Combine(
        /*policy_value=*/testing::Values(kLocalPolicy, kCloudPolicy),
        /*local_scan_after_preview_feature_enabled=*/testing::Bool(),
        /*cloud_scan_after_preview_feature_enabled=*/testing::Bool()));

}  // namespace enterprise_connectors
