// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/pickle.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_dialog_controller.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/test/mock_realtime_reporting_client.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/service/metrics/glic_invoke_metrics.h"
#include "chrome/browser/glic/service/metrics/metrics_types.h"
#include "chrome/browser/glic/test_support/glic_drag_and_drop_test_base.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/drag_and_drop_test_utils.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/geometry/point.h"

namespace glic {
namespace {

class GlicDragAndDropPolicyTest : public GlicDragAndDropTestBase {
 public:
  GlicDragAndDropPolicyTest()
      : GlicDragAndDropTestBase(
            GlicTestJsPath("./glic_drag_and_drop_policy_browsertest.js")) {}

  void SetUpOnMainThread() override {
    GlicDragAndDropTestBase::SetUpOnMainThread();

    enterprise_connectors::RealtimeReportingClientFactory::GetInstance()
        ->SetTestingFactory(
            GetProfile(),
            base::BindRepeating(
                &enterprise_connectors::test::MockRealtimeReportingClient::
                    CreateMockRealtimeReportingClient));

    enterprise_connectors::test::FakeContentAnalysisDelegate::
        ResetStaticDialogFlagsAndTotalRequestsCount();

    // Ensure the dialog appears immediately for any scan.
    enterprise_connectors::test::FakeContentAnalysisDelegate::SetResponseDelay(
        base::Milliseconds(0));

    // Ensure the dialog appears immediately for any pending scan.
    enterprise_connectors::ContentAnalysisDialogController::
        SetShowDialogDelayForTesting(base::Milliseconds(0));

    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidToken("fake-dm-token"));

    enterprise_connectors::test::SetAnalysisConnector(
        GetProfile()->GetPrefs(), enterprise_connectors::FILE_ATTACHED,
        R"(
        {
          "service_provider": "google",
          "enable": [
            {
              "url_list": ["*"],
              "tags": ["dlp"]
            }
          ],
          "block_until_verdict": 1,
          "minimum_data_size": 1
        })");
  }

  void TearDownOnMainThread() override {
    enterprise_connectors::ContentAnalysisDelegate::ResetFactoryForTesting();
    enterprise_connectors::test::FakeContentAnalysisDelegate::
        ResetStaticDialogFlagsAndTotalRequestsCount();
    enterprise_connectors::test::SetOnSecurityEventReporting(
        GetProfile()->GetPrefs(), false);
    enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
        GetProfile())
        ->SetBrowserCloudPolicyClientForTesting(nullptr);
    GlicDragAndDropTestBase::TearDownOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(GlicDragAndDropPolicyTest, testDragAndDropDlp) {
  enterprise_connectors::ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(
          &enterprise_connectors::test::FakeContentAnalysisDelegate::Create,
          base::DoNothing(),
          base::BindRepeating(
              [](const std::string& contents, const base::FilePath& path) {
                return enterprise_connectors::test::
                    FakeContentAnalysisDelegate::SuccessfulResponse({"dlp"});
              }),
          "fake-dm-token"));

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * glic_instance,
                       OpenGlicForActiveTab());
  Host* glic_host = &glic_instance->host();
  PrepareGuestForDrag(*glic_host);
  gfx::Point host_relative_point = GetGuestCenterInHost(*glic_host);

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  base::FilePath test_file = CreateTestFile(
      temp_dir, "test.txt", "This is some test content for DLP scanning.");

  drag_and_drop_test_utils::DragAndDropSimulator simulator(
      glic_host->webui_contents());
  ASSERT_TRUE(simulator.SimulateDragEnter(host_relative_point, test_file));

  ContinueJsTest();

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return glic_host->GetGuestMainFrame()->GetRenderWidgetHost()->GetView() &&
           !glic_host->GetGuestMainFrame()
                ->GetRenderWidgetHost()
                ->GetView()
                ->GetViewBounds()
                .IsEmpty();
  }));
  host_relative_point = GetGuestCenterInHost(*glic_host);
  ASSERT_TRUE(simulator.SimulateDrop(host_relative_point));

  ContinueJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicDragAndDropPolicyTest, testDragAndDropDlpBlocked) {
  enterprise_connectors::ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(
          &enterprise_connectors::test::FakeContentAnalysisDelegate::Create,
          base::DoNothing(),
          base::BindRepeating([](const std::string& contents,
                                 const base::FilePath& path) {
            return enterprise_connectors::test::FakeContentAnalysisDelegate::
                DlpResponse(enterprise_connectors::ContentAnalysisResponse::
                                Result::SUCCESS,
                            "block-rule",
                            enterprise_connectors::ContentAnalysisResponse::
                                Result::TriggeredRule::BLOCK);
          }),
          "fake-dm-token"));

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * glic_instance,
                       OpenGlicForActiveTab());
  Host* glic_host = &glic_instance->host();
  PrepareGuestForDrag(*glic_host);
  gfx::Point host_relative_point = GetGuestCenterInHost(*glic_host);

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  base::FilePath test_file =
      CreateTestFile(temp_dir, "test_blocked.txt",
                     "This content should be blocked by DLP policy.");

  drag_and_drop_test_utils::DragAndDropSimulator simulator(
      glic_host->webui_contents());
  ASSERT_TRUE(simulator.SimulateDragEnter(host_relative_point, test_file));

  ContinueJsTest();

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return glic_host->GetGuestMainFrame()->GetRenderWidgetHost()->GetView() &&
           !glic_host->GetGuestMainFrame()
                ->GetRenderWidgetHost()
                ->GetView()
                ->GetViewBounds()
                .IsEmpty();
  }));
  host_relative_point = GetGuestCenterInHost(*glic_host);
  ASSERT_TRUE(simulator.SimulateDrop(host_relative_point));

  ContinueJsTest();

  ASSERT_TRUE(base::test::RunUntil([]() -> bool {
    return enterprise_connectors::test::FakeContentAnalysisDelegate::
               WasDialogShown() &&
           enterprise_connectors::test::FakeContentAnalysisDelegate::
                   GetTotalAnalysisRequestsCount() >= 1;
  }));

  ContinueJsTest();
}

// Linux does not natively support direct in-memory FileContents retrieval
// inside OSExchangeData.
// Web-to-Glic drag-and-drop is fully supported on macOS, Windows, and
// ChromeOS. On Windows, this test is currently disabled due to test simulation
// flakiness in headless CI runners.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#define MAYBE_testWebToGlicDragDlpBlocked DISABLED_testWebToGlicDragDlpBlocked
#else
#define MAYBE_testWebToGlicDragDlpBlocked testWebToGlicDragDlpBlocked
#endif
IN_PROC_BROWSER_TEST_F(GlicDragAndDropPolicyTest,
                       MAYBE_testWebToGlicDragDlpBlocked) {
  enterprise_connectors::test::SetAnalysisConnector(
      GetProfile()->GetPrefs(), enterprise_connectors::BULK_DATA_ENTRY,
      R"(
      {
        "service_provider": "google",
        "enable": [
          {
            "url_list": ["*"],
            "tags": ["dlp"]
          }
        ],
        "block_until_verdict": 1,
        "minimum_data_size": 1
      })");

  base::HistogramTester histogram_tester;
  enterprise_connectors::ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(
          &enterprise_connectors::test::FakeContentAnalysisDelegate::Create,
          base::DoNothing(),
          base::BindRepeating([](const std::string&, const base::FilePath&) {
            return enterprise_connectors::test::FakeContentAnalysisDelegate::
                DlpResponse(enterprise_connectors::ContentAnalysisResponse::
                                Result::SUCCESS,
                            "block-rule",
                            enterprise_connectors::ContentAnalysisResponse::
                                Result::TriggeredRule::BLOCK);
          }),
          "fake-dm-token"));
  // 1. Open GLIC and prepare the guest.
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * glic_instance,
                       OpenGlicForActiveTab());
  Host* glic_host = &glic_instance->host();
  PrepareGuestForDrag(*glic_host);

  // 2. Setup Source Tab with an image.
  content::WebContents* source_wc = SetupSourceTabWithDraggableImage();
  ASSERT_TRUE(source_wc);

  // 3. Setup Glic host Drop simulator.
  drag_and_drop_test_utils::DragAndDropSimulator simulator(
      glic_host->webui_contents());
  gfx::Point host_relative_point = GetGuestCenterInHost(*glic_host);

  // 4. Start waiting for Gtest's real drag to initiate.
  drag_and_drop_test_utils::DragStartWaiter waiter(
      source_wc, base::BindLambdaForTesting([&]() {
        base::ScopedClosureRunner release_runner(base::BindOnce(
            &drag_and_drop_test_utils::DragStartWaiter::ReleaseDrag,
            base::Unretained(&waiter)));

        std::unique_ptr<ui::OSExchangeData> task_data =
            waiter.TakeCapturedData();

        // Create a clean OSExchangeData to avoid duplicate format registry
        // crashes.
        auto augmented_data = std::make_unique<ui::OSExchangeData>();

        // Package the raw file contents bytes directly.
        augmented_data->SetFileContents(
            base::FilePath(FILE_PATH_LITERAL("cors-allowed.jpg")),
            base::as_byte_span(
                "This content should be blocked by DLP policy."));

        augmented_data->SetURL(embedded_test_server()->GetURL(
                                   "a.com", "/drag_and_drop/cors-allowed.jpg"),
                               u"cors-allowed.jpg");

        // Clone the bespoke Chrome drag ID from Blink's payload.
        if (std::optional<base::UnguessableToken> drag_id =
                task_data->GetChromeDragId()) {
          augmented_data->SetChromeDragId(*drag_id);
        }

        if (task_data->GetSource()) {
          augmented_data->SetSource(std::make_unique<ui::DataTransferEndpoint>(
              *task_data->GetSource()));
        }

        // Programmatically simulate Glic host DragEnter. Glic successfully
        // resolves the drag ID.
        ASSERT_TRUE(simulator.SimulateDragEnter(host_relative_point,
                                                std::move(augmented_data)));

        // Programmatically simulate Glic host Drop. DLP scanning intercepts it.
        ASSERT_TRUE(simulator.SimulateDrop(host_relative_point));
      }));
  waiter.SuppressPassingStartDragFurther();

  // 5. Simulate a real drag starting in the source tab.
  SimulateMouseDragFromImage(source_wc);

  // 6. Wait for the entire drag-and-drop sequence to finish.
  waiter.WaitUntilDragStart();

  ContinueJsTest();

  // 7. Verify that the ContentAnalysisDelegate DLP scan triggered and blocked
  // it.
  ASSERT_TRUE(base::test::RunUntil([]() -> bool {
    return enterprise_connectors::test::FakeContentAnalysisDelegate::
               WasDialogShown() &&
           enterprise_connectors::test::FakeContentAnalysisDelegate::
                   GetTotalAnalysisRequestsCount() >= 1;
  }));

  EXPECT_OK(RunUntilEqual(
      [&]() {
        return histogram_tester.GetBucketCount(
            "Glic.InvokeResult.WebDragDrop",
            static_cast<int>(
                GlicInvokeError::kAdditionalContextFailedPastePolicy));
      },
      1));
  histogram_tester.ExpectUniqueSample("Glic.DragAndDrop.ValidationResult",
                                      GlicDragAndDropValidationResult::kSuccess,
                                      1);
}

}  // namespace
}  // namespace glic
