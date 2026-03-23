// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
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
#include "chrome/browser/glic/test_support/glic_api_test.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/drag_and_drop_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
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
#include "third_party/blink/public/common/page/drag_operation.h"
#include "ui/aura/window.h"
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

class GlicDragAndDropPolicyTest : public InteractiveGlicApiTest {
 public:
  GlicDragAndDropPolicyTest()
      : InteractiveGlicApiTest("./glic_drag_and_drop_interactive_uitest.js") {
    feature_list_.InitAndEnableFeature(features::kGlicDragAndDropFileUpload);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InteractiveGlicApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(::switches::kGlicDev);
    // Skips FRE experience.
    command_line->AppendSwitch(::switches::kGlicAutomation);
  }

  void SetUpOnMainThread() override {
    InteractiveGlicApiTest::SetUpOnMainThread();

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
    InteractiveGlicApiTest::TearDownOnMainThread();
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

    guest_contents->Focus();

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

  RunTestSequence(OpenGlic());
  Host* glic_host = GetHost();
  ASSERT_TRUE(glic_host);
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
  ASSERT_TRUE(step_data().has_value());
  EXPECT_EQ(step_data()->GetString(), "drag-ready");

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
  ASSERT_TRUE(step_data().has_value());
  ASSERT_TRUE(step_data()->is_string());
  EXPECT_EQ(step_data()->GetString(), "test.txt");

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

  RunTestSequence(OpenGlic());
  Host* glic_host = GetHost();
  ASSERT_TRUE(glic_host);
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
  ASSERT_TRUE(step_data().has_value());
  EXPECT_EQ(step_data()->GetString(), "drag-ready");

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
  ASSERT_TRUE(step_data().has_value());
  EXPECT_EQ(step_data()->GetString(), "final-check");
}

}  // namespace
}  // namespace glic
