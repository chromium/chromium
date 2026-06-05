// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_dialog_controller.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/test/mock_realtime_reporting_client.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/test_support/new_glic_api_test.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/drag_and_drop_test_utils.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/window.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace glic {
namespace {

class GlicDragAndDropPolicyTest : public GlicApiBrowserTest {
 public:
  using InProcessBrowserTest::browser;

  GlicDragAndDropPolicyTest()
      : GlicApiBrowserTest("./glic_drag_and_drop_interactive_uitest.js") {
    feature_list_.InitWithFeatures({features::kGlicDragAndDropFileUpload,
                                    features::kGlicWebDragAndDropFileUpload},
                                   {});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    GlicApiBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(::switches::kGlicDev);
    // Skips FRE experience.
    command_line->AppendSwitch(::switches::kGlicAutomation);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("a.com", "127.0.0.1");
    host_resolver()->AddRule("b.com", "127.0.0.1");

    GlicApiBrowserTest::SetUpOnMainThread();

    enterprise_connectors::RealtimeReportingClientFactory::GetInstance()
        ->SetTestingFactory(
            browser()->profile(),
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
        browser()->profile()->GetPrefs(), enterprise_connectors::FILE_ATTACHED,
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

    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(browser()->profile());
    signin::MakePrimaryAccountAvailable(identity_manager, "foo@google.com",
                                        signin::ConsentLevel::kSignin);
    signin::SetRefreshTokenForPrimaryAccount(identity_manager);
  }

  void TearDownOnMainThread() override {
    enterprise_connectors::test::SetOnSecurityEventReporting(
        browser()->profile()->GetPrefs(), false);
    enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
        browser()->profile())
        ->SetBrowserCloudPolicyClientForTesting(nullptr);
    GlicApiBrowserTest::TearDownOnMainThread();
  }

 protected:
  void PrepareGuestForDrag(Host& glic_host) {
    content::WebContents* guest_contents = nullptr;
    ASSERT_TRUE(base::test::RunUntil([&]() {
      guest_contents = GetGlicGuestWebContents(glic_host.webui_contents());
      return guest_contents != nullptr;
    }));
    EXPECT_TRUE(content::WaitForLoadStop(guest_contents));
    ExecuteJsTest();

    content::RenderWidgetHost* rwh =
        glic_host.GetGuestMainFrame()->GetRenderWidgetHost();
    ASSERT_TRUE(rwh);

    ASSERT_TRUE(base::test::RunUntil([&]() {
      return !rwh->GetView()->GetViewBounds().IsEmpty() &&
             !glic_host.webui_contents()
                  ->GetRenderWidgetHostView()
                  ->GetViewBounds()
                  .IsEmpty();
    }));
    // Ensure hit test data is ready for the guest.
    content::WaitForHitTestData(glic_host.GetGuestMainFrame());
  }

  gfx::Point GetGuestCenterInHost(Host& glic_host) {
    auto* guest_view =
        glic_host.GetGuestMainFrame()->GetRenderWidgetHost()->GetView();
    auto* host_view = glic_host.webui_contents()->GetRenderWidgetHostView();

    gfx::Rect guest_bounds = guest_view->GetViewBounds();

    gfx::Point center(guest_bounds.width() / 2, guest_bounds.height() / 2);

    aura::Window::ConvertPointToTarget(guest_view->GetNativeView(),
                                       host_view->GetNativeView(), &center);
    return center;
  }

  base::FilePath CreateTestFile(base::ScopedTempDir& temp_dir,
                                const std::string& name,
                                const std::string& contents) {
    EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
    base::FilePath test_file = temp_dir.GetPath().AppendASCII(name);
    base::WriteFile(test_file, base::as_byte_span(contents));
    return test_file;
  }

  content::WebContents* SetupSourceTabWithDraggableImage() {
    if (!ui_test_utils::NavigateToURL(
            browser(), embedded_test_server()->GetURL(
                           "a.com", "/drag_and_drop/image_source.html"))) {
      ADD_FAILURE() << "NavigateToURL failed";
      return nullptr;
    }
    if (!ui_test_utils::BringBrowserWindowToFront(browser())) {
      ADD_FAILURE() << "BringBrowserWindowToFront failed";
      return nullptr;
    }
    content::WebContents* source_wc =
        browser()->tab_strip_model()->GetActiveWebContents();
    if (!source_wc) {
      ADD_FAILURE() << "No active web contents";
      return nullptr;
    }
    source_wc->Focus();

    // Fix the image src so it's draggable.
    GURL img_url = embedded_test_server()->GetURL(
        "a.com", "/drag_and_drop/cors-allowed.jpg");
    if (!content::ExecJs(
            source_wc,
            content::JsReplace("document.querySelector('img').src = $1",
                               img_url.spec()))) {
      ADD_FAILURE() << "ExecJs to set image src failed";
      return nullptr;
    }

    // Wait for the image to load and lay out before querying its bounds.
    auto result = content::EvalJs(
        source_wc,
        "const img = document.querySelector('img');"
        "new Promise(resolve => {"
        "  if (img.complete && img.naturalWidth > 0) { resolve(true); }"
        "  else { img.onload = () => resolve(true); img.onerror = () => "
        "resolve(false); }"
        "})");
    if (!result.is_ok() || !result.ExtractBool()) {
      ADD_FAILURE() << "Image did not load successfully";
      return nullptr;
    }
    return source_wc;
  }

  void SimulateMouseDragFromImage(content::WebContents* source_wc) {
    // We use Javascript to find the image center and then click+drag it.
    double img_x = content::EvalJs(source_wc,
                                   "const img = document.querySelector('img');"
                                   "const rect = img.getBoundingClientRect();"
                                   "rect.left + rect.width / 2")
                       .ExtractDouble();
    double img_y = content::EvalJs(source_wc,
                                   "const img = document.querySelector('img');"
                                   "const rect = img.getBoundingClientRect();"
                                   "rect.top + rect.height / 2")
                       .ExtractDouble();

    gfx::Rect container_bounds = source_wc->GetContainerBounds();
    gfx::Point drag_start_point(container_bounds.x() + img_x,
                                container_bounds.y() + img_y);
    gfx::Point drag_end_point = drag_start_point + gfx::Vector2d(100, 100);

    // Start Gtest's real OS mouse drag events.
    base::RunLoop drag_start_loop;
    ui_controls::SendMouseMoveNotifyWhenDone(
        drag_start_point.x(), drag_start_point.y(),
        base::BindLambdaForTesting([&]() {
          ui_controls::SendMouseEventsNotifyWhenDone(
              ui_controls::LEFT, ui_controls::DOWN,
              base::BindLambdaForTesting([&]() {
                // Move in multiple steps to ensure Blink detects the drag.
                ui_controls::SendMouseMove(drag_start_point.x() + 20,
                                           drag_start_point.y() + 20);
                ui_controls::SendMouseMove(drag_start_point.x() + 40,
                                           drag_start_point.y() + 40);
                ui_controls::SendMouseMoveNotifyWhenDone(
                    drag_end_point.x(), drag_end_point.y(),
                    base::BindLambdaForTesting([&]() {
                      ui_controls::SendMouseEventsNotifyWhenDone(
                          ui_controls::LEFT, ui_controls::UP,
                          drag_start_loop.QuitClosure());
                    }));
              }));
        }));
    drag_start_loop.Run();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
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
// inside OSExchangeData. Web-to-Glic drag-and-drop is fully supported on macOS,
// Windows and ChromeOS, so this specific materialization test is disabled on
// Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_testWebToGlicDragMaterialization \
  DISABLED_testWebToGlicDragMaterialization
#else
#define MAYBE_testWebToGlicDragMaterialization testWebToGlicDragMaterialization
#endif
IN_PROC_BROWSER_TEST_F(GlicDragAndDropPolicyTest,
                       MAYBE_testWebToGlicDragMaterialization) {
  enterprise_connectors::ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(
          &enterprise_connectors::test::FakeContentAnalysisDelegate::Create,
          base::DoNothing(),
          base::BindRepeating([](const std::string&, const base::FilePath&) {
            return enterprise_connectors::test::FakeContentAnalysisDelegate::
                SuccessfulResponse({"dlp"});
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

  // 3. Setup the drop simulation to run WHILE the source drag is active.
  drag_and_drop_test_utils::DragAndDropSimulator simulator(
      glic_host->webui_contents());
  gfx::Point host_relative_point = GetGuestCenterInHost(*glic_host);

  // 4. Start waiting for a drag to initiate.
  drag_and_drop_test_utils::DragStartWaiter waiter(
      source_wc, base::BindLambdaForTesting([&]() {
        base::ScopedClosureRunner release_runner(base::BindOnce(
            &drag_and_drop_test_utils::DragStartWaiter::ReleaseDrag,
            base::Unretained(&waiter)));

        // Programmatically simulate the DragEnter with Blink's real captured
        // drag data (preserving the custom Glic drag source ID pickle
        // chromium/x-drag-id).
        simulator.SimulateDragEnter(host_relative_point,
                                    waiter.TakeCapturedData());

        // Programmatically simulate the Drop immediately inside Gtest's drag
        // callback context.
        simulator.SimulateDrop(host_relative_point);
      }));
  waiter.SuppressPassingStartDragFurther();

  // 5. Simulate a real drag starting in the source tab.
  SimulateMouseDragFromImage(source_wc);

  // 6. Wait for the entire drag-and-drop sequence to finish.
  waiter.WaitUntilDragStart();

  // 7. Gtest's main thread is now fully unblocked.
  // Resume TS and wait for the test to complete.
  ContinueJsTest();
}

// Linux does not natively support direct in-memory FileContents retrieval
// inside OSExchangeData. Web-to-Glic drag-and-drop is fully supported on macOS,
// Windows and ChromeOS, so this specific DLP blocking test is disabled on
// Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_testWebToGlicDragDlpBlocked DISABLED_testWebToGlicDragDlpBlocked
#else
#define MAYBE_testWebToGlicDragDlpBlocked testWebToGlicDragDlpBlocked
#endif
IN_PROC_BROWSER_TEST_F(GlicDragAndDropPolicyTest,
                       MAYBE_testWebToGlicDragDlpBlocked) {
  enterprise_connectors::test::SetAnalysisConnector(
      browser()->profile()->GetPrefs(), enterprise_connectors::BULK_DATA_ENTRY,
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

  // 4. Setup Glic host Drop simulator.
  drag_and_drop_test_utils::DragAndDropSimulator simulator(
      glic_host->webui_contents());
  gfx::Point host_relative_point = GetGuestCenterInHost(*glic_host);

  // 5. Start waiting for Gtest's real drag to initiate.
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

        // Clone Glic's custom x-drag-id custom web data format from Blink's
        // payload.
        std::optional<base::Pickle> pickled_data = task_data->GetPickledData(
            ui::ClipboardFormatType::DataTransferCustomType());
        if (pickled_data.has_value()) {
          augmented_data->SetPickledData(
              ui::ClipboardFormatType::DataTransferCustomType(), *pickled_data);
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

  // 6. Simulate a real drag starting in the source tab.
  SimulateMouseDragFromImage(source_wc);

  // 7. Wait for the entire drag-and-drop sequence to finish.
  waiter.WaitUntilDragStart();

  ContinueJsTest();

  // 8. Verify that the ContentAnalysisDelegate DLP scan triggered and blocked
  // it.
  ASSERT_TRUE(base::test::RunUntil([]() -> bool {
    return enterprise_connectors::test::FakeContentAnalysisDelegate::
               WasDialogShown() &&
           enterprise_connectors::test::FakeContentAnalysisDelegate::
                   GetTotalAnalysisRequestsCount() >= 1;
  }));

  ContinueJsTest();
}

}  // namespace
}  // namespace glic
