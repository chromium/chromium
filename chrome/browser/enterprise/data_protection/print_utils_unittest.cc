// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_protection/print_utils.h"

#include "base/containers/span.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/analysis/page_print_request_handler.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/printing/print_preview_test.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/common.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "printing/printing_features.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_sdk_manager.h"  // nogncheck
#endif

namespace enterprise_data_protection {

namespace {

using ContentAnalysisResponse = enterprise_connectors::ContentAnalysisResponse;
using ContentAnalysisDelegate = enterprise_connectors::ContentAnalysisDelegate;
using RealtimeReportingClientFactory =
    enterprise_connectors::RealtimeReportingClientFactory;

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

constexpr char16_t kUserJustification[] = u"User justification";

scoped_refptr<base::RefCountedMemory> CreateData() {
  return base::MakeRefCounted<base::RefCountedStaticMemory>(
      base::byte_span_from_cstring(kTestData));
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
  using ContentAnalysisDelegate::ContentAnalysisDelegate;

  static std::unique_ptr<ContentAnalysisDelegate> Create(
      content::WebContents* contents,
      ContentAnalysisDelegate::Data data,
      ContentAnalysisDelegate::CompletionCallback callback) {
    auto delegate = base::WrapUnique(new PrintTestContentAnalysisDelegate(
        contents, std::move(data), std::move(callback),
        enterprise_connectors::DeepScanAccessPoint::PRINT));
    test_delegate_ = delegate.get();
    return delegate;
  }
};

class TestPagePrintRequestHandler
    : public enterprise_connectors::PagePrintRequestHandler {
 public:
  static std::unique_ptr<PagePrintRequestHandler> Create(
      ContentAnalysisResponse::Result::TriggeredRule::Action action,
      enterprise_connectors::ContentAnalysisInfo* content_analysis_info,
      safe_browsing::BinaryUploadService* upload_service,
      Profile* profile,
      GURL url,
      const std::string& printer_name,
      const std::string& page_content_type,
      base::ReadOnlySharedMemoryRegion page_region,
      CompletionCallback callback) {
    auto handler = base::WrapUnique(new TestPagePrintRequestHandler(
        content_analysis_info, upload_service, profile, url, printer_name,
        page_content_type, std::move(page_region), std::move(callback)));
    handler->action_ = action;
    return handler;
  }

 protected:
  using PagePrintRequestHandler::PagePrintRequestHandler;

  void UploadForDeepScanning(
      std::unique_ptr<enterprise_connectors::PagePrintAnalysisRequest> request)
      override {
    ASSERT_EQ(request->printer_name(), kPrinterName);
    OnContentAnalysisResponse(
        enterprise_connectors::ScanRequestUploadResult::SUCCESS,
        CreateResponse(action_));
  }

 private:
  ContentAnalysisResponse::Result::TriggeredRule::Action action_;
};

class PrintContentAnalysisUtilsTest
    : public PrintPreviewTest,
      public testing::WithParamInterface<const char*> {
 public:
  PrintContentAnalysisUtilsTest() {
    ContentAnalysisDelegate::DisableUIForTesting();
  }

  const char* policy_value() const { return GetParam(); }

  void SetUp() override {
    PrintPreviewTest::SetUp();
    chrome::NewTab(browser());

    SetDMTokenForTesting(policy::DMToken::CreateValidToken(kDmToken));

    client_ = std::make_unique<policy::MockCloudPolicyClient>();

    RealtimeReportingClientFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating([](content::BrowserContext* context) {
          return std::unique_ptr<KeyedService>(
              new enterprise_connectors::RealtimeReportingClient(context));
        }));

    RealtimeReportingClientFactory::GetForProfile(profile())
        ->SetBrowserCloudPolicyClientForTesting(client_.get());
    identity_test_environment_.MakePrimaryAccountAvailable(
        kUserName, signin::ConsentLevel::kSignin);
    RealtimeReportingClientFactory::GetForProfile(profile())
        ->SetIdentityManagerForTesting(
            identity_test_environment_.identity_manager());

    enterprise_connectors::test::SetAnalysisConnector(
        profile()->GetPrefs(), enterprise_connectors::PRINT, policy_value());
    enterprise_connectors::test::SetOnSecurityEventReporting(
        profile()->GetPrefs(), true);
  }

  void TearDown() override {
    RealtimeReportingClientFactory::GetForProfile(profile())
        ->SetBrowserCloudPolicyClientForTesting(nullptr);
    SetDMTokenForTesting(policy::DMToken::CreateEmptyToken());
    enterprise_connectors::PagePrintRequestHandler::ResetFactoryForTesting();
    PrintPreviewTest::TearDown();
  }

  content::WebContents* contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  signin::IdentityTestEnvironment identity_test_environment_;

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
  // This installs a fake SDK manager that creates fake SDK clients when
  // its GetClient() method is called. This is needed so that calls to
  // ContentAnalysisSdkManager::Get()->GetClient() do not fail.
  enterprise_connectors::FakeContentAnalysisSdkManager sdk_manager_;
#endif
};

}  // namespace

TEST_P(PrintContentAnalysisUtilsTest, GetPrintAnalysisData_BeforeSystemDialog) {
  auto data = GetPrintAnalysisData(contents(),
                                   PrintScanningContext::kBeforeSystemDialog);

  ASSERT_TRUE(data);
  ASSERT_EQ(data->settings.block_until_verdict,
            enterprise_connectors::BlockUntilVerdict::kBlock);
  ASSERT_EQ(data->reason,
            enterprise_connectors::ContentAnalysisRequest::SYSTEM_DIALOG_PRINT);

  ASSERT_EQ(policy_value() == kLocalPolicy,
            data->settings.cloud_or_local_settings.is_local_analysis());
  ASSERT_EQ(policy_value() == kCloudPolicy,
            data->settings.cloud_or_local_settings.is_cloud_analysis());
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
}

TEST_P(PrintContentAnalysisUtilsTest,
       GetPrintAnalysisData_NormalPrintAfterPreview) {
  auto data = GetPrintAnalysisData(
      contents(), PrintScanningContext::kNormalPrintAfterPreview);

  ASSERT_TRUE(data);
  ASSERT_EQ(data->settings.block_until_verdict,
            enterprise_connectors::BlockUntilVerdict::kBlock);
  ASSERT_EQ(data->reason,
            enterprise_connectors::ContentAnalysisRequest::PRINT_PREVIEW_PRINT);

  ASSERT_EQ(policy_value() == kLocalPolicy,
            data->settings.cloud_or_local_settings.is_local_analysis());
  ASSERT_EQ(policy_value() == kCloudPolicy,
            data->settings.cloud_or_local_settings.is_cloud_analysis());
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
}

TEST_P(PrintContentAnalysisUtilsTest,
       GetPrintAnalysisData_SystemPrintBeforePrintDocument) {
  auto data = GetPrintAnalysisData(
      contents(), PrintScanningContext::kSystemPrintBeforePrintDocument);

  ASSERT_TRUE(data);
  ASSERT_EQ(data->settings.block_until_verdict,
            enterprise_connectors::BlockUntilVerdict::kBlock);
  ASSERT_EQ(data->reason,
            enterprise_connectors::ContentAnalysisRequest::SYSTEM_DIALOG_PRINT);

  ASSERT_EQ(policy_value() == kLocalPolicy,
            data->settings.cloud_or_local_settings.is_local_analysis());
  ASSERT_EQ(policy_value() == kCloudPolicy,
            data->settings.cloud_or_local_settings.is_cloud_analysis());
}

#if BUILDFLAG(IS_MAC)
TEST_P(PrintContentAnalysisUtilsTest,
       GetPrintAnalysisData_MacOpenPdfInPreview) {
  auto data =
      GetPrintAnalysisData(contents(), PrintScanningContext::kOpenPdfInPreview);

  ASSERT_TRUE(data);
  ASSERT_EQ(data->settings.block_until_verdict,
            enterprise_connectors::BlockUntilVerdict::kBlock);
  ASSERT_EQ(data->reason,
            enterprise_connectors::ContentAnalysisRequest::PRINT_PREVIEW_PRINT);

  ASSERT_EQ(policy_value() == kLocalPolicy,
            data->settings.cloud_or_local_settings.is_local_analysis());
  ASSERT_EQ(policy_value() == kCloudPolicy,
            data->settings.cloud_or_local_settings.is_cloud_analysis());
}
#endif  // BUILDFLAG(IS_MAC)

TEST_P(PrintContentAnalysisUtilsTest, PrintIfAllowedByPolicyAllowed) {
  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&PrintTestContentAnalysisDelegate::Create));
  enterprise_connectors::PagePrintRequestHandler::SetFactoryForTesting(
      base::BindRepeating(
          &TestPagePrintRequestHandler::Create,
          ContentAnalysisResponse::Result::TriggeredRule::ACTION_UNSPECIFIED));

  enterprise_connectors::test::EventReportValidator validator(client_.get());
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
}

TEST_P(PrintContentAnalysisUtilsTest, PrintIfAllowedByPolicyReportOnly) {
  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&PrintTestContentAnalysisDelegate::Create));
  enterprise_connectors::PagePrintRequestHandler::SetFactoryForTesting(
      base::BindRepeating(
          &TestPagePrintRequestHandler::Create,
          ContentAnalysisResponse::Result::TriggeredRule::REPORT_ONLY));

  enterprise_connectors::test::EventReportValidator validator(client_.get());
  base::RunLoop validator_run_loop;
  validator.SetDoneClosure(validator_run_loop.QuitClosure());
  validator.ExpectSensitiveDataEvent(
      /*url*/ "",
      /*tab_url*/ "",
      /*source*/ "",
      /*destination*/ kPrinterName,
      /*filename*/ "New Tab",
      /*sha*/ "",
      /*trigger*/
      enterprise_connectors::kPagePrintDataTransferEventTrigger,
      /*dlp_verdict*/
      CreateResult(ContentAnalysisResponse::Result::TriggeredRule::REPORT_ONLY),
      /*mimetype*/ PrintMimeTypes(),
      /*size*/ std::nullopt,
      /*result*/
      enterprise_connectors::EventResultToString(
          enterprise_connectors::EventResult::ALLOWED),
      /*username*/ kUserName,
      /*profile_identifier*/ profile()->GetPath().AsUTF8Unsafe(),
      /*scan_id*/ kScanId,
      /*content_transfer_method*/ std::nullopt,
      /*user_justification*/ std::nullopt);

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
  validator_run_loop.Run();
}

TEST_P(PrintContentAnalysisUtilsTest, PrintIfAllowedByPolicyWarnThenCancel) {
  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&PrintTestContentAnalysisDelegate::Create));
  enterprise_connectors::PagePrintRequestHandler::SetFactoryForTesting(
      base::BindRepeating(
          &TestPagePrintRequestHandler::Create,
          ContentAnalysisResponse::Result::TriggeredRule::WARN));

  // The cancel callback is called after the verdict is received.
  enterprise_connectors::ContentAnalysisDelegate::
      SetOnAckAllRequestsCallbackForTesting(base::BindOnce(
          [](const std::map<std::string,
                            enterprise_connectors::
                                ContentAnalysisAcknowledgement::FinalAction>&
                 map) {
            ASSERT_TRUE(test_delegate_);
            test_delegate_->Cancel(/* warning= */ true);
          }));

  enterprise_connectors::test::EventReportValidator validator(client_.get());
  base::RunLoop validator_run_loop;
  validator.SetDoneClosure(validator_run_loop.QuitClosure());
  validator.ExpectSensitiveDataEvent(
      /*url*/ "",
      /*tab_url*/ "",
      /*source*/ "",
      /*destination*/ kPrinterName,
      /*filename*/ "New Tab",
      /*sha*/ "",
      /*trigger*/
      enterprise_connectors::kPagePrintDataTransferEventTrigger,
      /*dlp_verdict*/
      CreateResult(ContentAnalysisResponse::Result::TriggeredRule::WARN),
      /*mimetype*/ PrintMimeTypes(),
      /*size*/ std::nullopt,
      /*result*/
      enterprise_connectors::EventResultToString(
          enterprise_connectors::EventResult::WARNED),
      /*username*/ kUserName,
      /*profile_identifier*/ profile()->GetPath().AsUTF8Unsafe(),
      /*scan_id*/ kScanId,
      /*content_transfer_method*/ std::nullopt,
      /*user_justification*/ std::nullopt);

  auto data = CreateData();
  base::RunLoop run_loop;
  auto on_verdict = base::BindLambdaForTesting([&run_loop](bool allowed) {
    ASSERT_FALSE(allowed);
    run_loop.Quit();
  });
  PrintIfAllowedByPolicy(data, contents(), kPrinterName,
                         PrintScanningContext::kNormalPrintAfterPreview,
                         std::move(on_verdict),
                         /*hide_preview=*/base::DoNothing());
  run_loop.Run();
  validator_run_loop.Run();
}

// TODO(http://crbug.com/434099738): Flaky on multiple platforms.
TEST_P(PrintContentAnalysisUtilsTest,
       DISABLED_PrintIfAllowedByPolicyWarnedThenBypass) {
  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&PrintTestContentAnalysisDelegate::Create));
  enterprise_connectors::PagePrintRequestHandler::SetFactoryForTesting(
      base::BindRepeating(
          &TestPagePrintRequestHandler::Create,
          ContentAnalysisResponse::Result::TriggeredRule::WARN));

  // The bypass callback is called after the verdict is received.
  enterprise_connectors::ContentAnalysisDelegate::
      SetOnAckAllRequestsCallbackForTesting(base::BindOnce(
          [](const std::map<std::string,
                            enterprise_connectors::
                                ContentAnalysisAcknowledgement::FinalAction>&
                 map) {
            ASSERT_TRUE(test_delegate_);
            test_delegate_->SetPageWarningForTesting();
            test_delegate_->BypassWarnings(kUserJustification);
          }));

  base::RunLoop validator_warn_run_loop;
  enterprise_connectors::test::EventReportValidator validator(client_.get());
  validator.SetDoneClosure(validator_warn_run_loop.QuitClosure());
  validator.ExpectSensitiveDataEventWarnThenBypass(
      /*url*/ "",
      /*tab_url*/ "",
      /*source*/ "",
      /*destination*/ kPrinterName,
      /*filename*/ "New Tab",
      /*sha*/ "",
      /*trigger*/
      enterprise_connectors::kPagePrintDataTransferEventTrigger,
      /*dlp_verdict*/
      CreateResult(ContentAnalysisResponse::Result::TriggeredRule::WARN),
      /*mimetype*/ PrintMimeTypes(),
      /*size*/ std::nullopt,
      /*username*/ kUserName,
      /*profile_identifier*/ profile()->GetPath().AsUTF8Unsafe(),
      /*scan_id*/ kScanId,
      /*content_transfer_method*/ std::nullopt,
      /*user_justifications*/ {std::nullopt, kUserJustification});

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
  validator_warn_run_loop.Run();
}

TEST_P(PrintContentAnalysisUtilsTest, PrintIfAllowedByPolicyBlocked) {
  ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(&PrintTestContentAnalysisDelegate::Create));
  enterprise_connectors::PagePrintRequestHandler::SetFactoryForTesting(
      base::BindRepeating(
          &TestPagePrintRequestHandler::Create,
          ContentAnalysisResponse::Result::TriggeredRule::BLOCK));

  enterprise_connectors::test::EventReportValidator validator(client_.get());
  base::RunLoop validator_run_loop;
  validator.SetDoneClosure(validator_run_loop.QuitClosure());
  validator.ExpectSensitiveDataEvent(
      /*url*/ "",
      /*tab_url*/ "",
      /*source*/ "",
      /*destination*/ kPrinterName,
      /*filename*/ "New Tab",
      /*sha*/ "",
      /*trigger*/
      enterprise_connectors::kPagePrintDataTransferEventTrigger,
      /*dlp_verdict*/
      CreateResult(ContentAnalysisResponse::Result::TriggeredRule::BLOCK),
      /*mimetype*/ PrintMimeTypes(),
      /*size*/ std::nullopt,
      /*result*/
      enterprise_connectors::EventResultToString(
          enterprise_connectors::EventResult::BLOCKED),
      /*username*/ kUserName,
      /*profile_identifier*/ profile()->GetPath().AsUTF8Unsafe(),
      /*scan_id*/ kScanId,
      /*content_transfer_method*/ std::nullopt,
      /*user_justification*/ std::nullopt);

  auto data = CreateData();
  base::RunLoop run_loop;
  auto on_verdict = base::BindLambdaForTesting([&run_loop](bool allowed) {
    ASSERT_FALSE(allowed);
    run_loop.Quit();
  });

  PrintIfAllowedByPolicy(data, contents(), kPrinterName,
                         PrintScanningContext::kNormalPrintAfterPreview,
                         std::move(on_verdict),
                         /*hide_preview=*/base::DoNothing());
  validator_run_loop.Run();
  run_loop.Run();
}

TEST_P(PrintContentAnalysisUtilsTest, PrintIfAllowedByPolicyNullInitiator) {
  enterprise_connectors::test::EventReportValidator validator(client_.get());
  validator.ExpectNoReport();

  auto data = CreateData();
  base::RunLoop run_loop;
  auto on_verdict = base::BindLambdaForTesting([&run_loop](bool allowed) {
    EXPECT_FALSE(allowed);
    run_loop.Quit();
  });
  PrintIfAllowedByPolicy(data, /*initiator=*/nullptr, kPrinterName,
                         PrintScanningContext::kNormalPrintAfterPreview,
                         std::move(on_verdict),
                         /*hide_preview=*/base::DoNothing());
  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PrintContentAnalysisUtilsTest,
    /*policy_value=*/testing::Values(kLocalPolicy, kCloudPolicy));

}  // namespace enterprise_data_protection
