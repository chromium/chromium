// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/projector/projector_controller_impl.h"
#include "ash/webui/media_app_ui/buildflags.h"
#include "ash/webui/media_app_ui/test/media_app_ui_browsertest.h"
#include "ash/webui/projector_app/buildflags.h"
#include "ash/webui/projector_app/projector_app_client.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "chrome/browser/ui/ash/projector/projector_app_client_impl.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "content/public/test/browser_test.h"

namespace {

#if BUILDFLAG(ENABLE_CROS_MEDIA_APP) && BUILDFLAG(ENABLE_CROS_PROJECTOR_APP)
// File containing the test utility library
constexpr base::FilePath::CharType kTestLibraryPath[] =
    FILE_PATH_LITERAL("ash/webui/system_apps/public/js/dom_testing_helpers.js");

void PrepareAppForTest(content::WebContents* web_contents) {
  EXPECT_TRUE(WaitForLoadStop(web_contents));
  EXPECT_EQ(nullptr,
            SandboxedWebUiAppTestBase::EvalJsInAppFrame(
                web_contents, SandboxedWebUiAppTestBase::LoadJsTestLibrary(
                                  base::FilePath(kTestLibraryPath))));
}
#endif  // BUILDFLAG(ENABLE_CROS_MEDIA_APP) &&
        // BUILDFLAG(ENABLE_CROS_PROJECTOR_APP)

}  // namespace

class ProjectorAppIntegrationTest : public ash::SystemWebAppIntegrationTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch("projector-extended-features-disabled");
  }
};

IN_PROC_BROWSER_TEST_P(ProjectorAppIntegrationTest, ProjectorApp) {
  const GURL url(ash::kChromeUITrustedProjectorUrl);
  EXPECT_NO_FATAL_FAILURE(ExpectSystemWebAppValid(
      ash::SystemWebAppType::PROJECTOR, url, "Screencast"));
}

// These tests try to load files bundled in our CIPD package. The CIPD package
// is included in the `linux-chromeos-chrome` trybot but not in
// `linux-chromeos-rel` trybot. Our CIPD package is only present when both the
// media app and Projector are enabled. We disable the tests rather than comment
// them out entirely so that they are still subject to compilation on
// open-source builds.
#if BUILDFLAG(ENABLE_CROS_MEDIA_APP) && BUILDFLAG(ENABLE_CROS_PROJECTOR_APP)
IN_PROC_BROWSER_TEST_P(ProjectorAppIntegrationTest,
                       LoadsInkForProjectorAnnotator) {
  // Begin Projector screen capture.
  auto* controller = ash::ProjectorControllerImpl::Get();
  auto* capture_mode_controller = ash::CaptureModeController::Get();

  // Set callback that is run when the Ink canvas has been initialized.
  base::RunLoop run_loop;
  base::OnceClosure callback = run_loop.QuitClosure();
  controller->set_canvas_initialized_callback_for_test(std::move(callback));

  controller->StartProjectorSession("projector_data");
  capture_mode_controller->PerformCapture();
  run_loop.Run();

  ProjectorAppClientImpl* projector_app_client =
      static_cast<ProjectorAppClientImpl*>(ash::ProjectorAppClient::Get());
  content::WebContents* annotator_embedder =
      projector_app_client->get_annotator_message_handler_for_test()
          ->get_web_ui_for_test()
          ->GetWebContents();
  PrepareAppForTest(annotator_embedder);

  // Checks ink is loaded by ensuring the ink engine canvas has a non zero width
  // and height attributes (checking <canvas.width/height is insufficient since
  // it has a default width of 300 and height of 150). Note: The loading of ink
  // engine elements can be async.
  constexpr char kCheckInkLoaded[] = R"(
      (async function checkInkLoaded() {
        const inkCanvas = await getNode('canvas',
          ['projector-ink-canvas-wrapper']);
        return !!inkCanvas &&
          !!inkCanvas.getAttribute('height') &&
          inkCanvas.getAttribute('height') !== '0' &&
          !!inkCanvas.getAttribute('width') &&
          inkCanvas.getAttribute('width') !== '0';
      })();
    )";
  EXPECT_EQ(true, SandboxedWebUiAppTestBase::EvalJsInAppFrame(
                      annotator_embedder, kCheckInkLoaded)
                      .ExtractBool());
}
#endif  // BUILDFLAG(ENABLE_CROS_MEDIA_APP) &&
        // BUILDFLAG(ENABLE_CROS_PROJECTOR_APP)

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    ProjectorAppIntegrationTest);
