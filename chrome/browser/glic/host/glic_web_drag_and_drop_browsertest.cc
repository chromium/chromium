// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/threading/thread_restrictions.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_delegate.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/service/metrics/glic_invoke_metrics.h"
#include "chrome/browser/glic/service/metrics/metrics_types.h"
#include "chrome/browser/glic/test_support/glic_drag_and_drop_test_base.h"
#include "chrome/test/base/drag_and_drop_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace glic {
namespace {

class GlicWebDragAndDropBrowserTest : public GlicDragAndDropTestBase,
                                      public testing::WithParamInterface<bool> {
 public:
  GlicWebDragAndDropBrowserTest()
      : GlicDragAndDropTestBase(
            GlicTestJsPath("./glic_web_drag_and_drop_browsertest.js"),
            GetParam()) {}
};

// Linux does not natively support direct in-memory FileContents retrieval
// inside OSExchangeData.
// Web-to-Glic drag-and-drop is fully supported on macOS, Windows, and
// ChromeOS. On Windows, this test is currently disabled due to test simulation
// flakiness in headless CI runners.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#define MAYBE_testWebToGlicDragMaterialization \
  DISABLED_testWebToGlicDragMaterialization
#else
#define MAYBE_testWebToGlicDragMaterialization testWebToGlicDragMaterialization
#endif
IN_PROC_BROWSER_TEST_P(GlicWebDragAndDropBrowserTest,
                       MAYBE_testWebToGlicDragMaterialization) {
  base::HistogramTester histogram_tester;
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
        // drag data (preserving the bespoke Chrome drag source ID
        // drag_id).
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

  EXPECT_OK(RunUntilEqual(
      [&]() {
        return histogram_tester.GetBucketCount(
            "Glic.InvokeResult.WebDragDrop",
            static_cast<int>(GlicInvokeResult::kSuccess));
      },
      1));
  histogram_tester.ExpectUniqueSample("Glic.DragAndDrop.ContentType",
                                      GlicDragAndDropContentType::kImage, 1);
  histogram_tester.ExpectUniqueSample("Glic.DragAndDrop.ValidationResult",
                                      GlicDragAndDropValidationResult::kSuccess,
                                      1);
}

// Linux does not natively support direct in-memory FileContents retrieval
// inside OSExchangeData.
// Web-to-Glic drag-and-drop is fully supported on macOS, Windows, and
// ChromeOS. On Windows, this test is currently disabled due to test simulation
// flakiness in headless CI runners.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
#define MAYBE_testWebToGlicDragMaterializationFromDetached \
  DISABLED_testWebToGlicDragMaterializationFromDetached
#else
#define MAYBE_testWebToGlicDragMaterializationFromDetached \
  testWebToGlicDragMaterializationFromDetached
#endif
IN_PROC_BROWSER_TEST_P(GlicWebDragAndDropBrowserTest,
                       MAYBE_testWebToGlicDragMaterializationFromDetached) {
  base::HistogramTester histogram_tester;
  enterprise_connectors::ContentAnalysisDelegate::SetFactoryForTesting(
      base::BindRepeating(
          &enterprise_connectors::test::FakeContentAnalysisDelegate::Create,
          base::DoNothing(),
          base::BindRepeating([](const std::string&, const base::FilePath&) {
            return enterprise_connectors::test::FakeContentAnalysisDelegate::
                SuccessfulResponse({"dlp"});
          }),
          "fake-dm-token"));

  // 1. Open GLIC and detach it into a separate OS window.
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * glic_instance,
                       OpenGlicForActiveTabAndDetach());
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
        // drag data (preserving the bespoke Chrome drag source ID
        // drag_id).
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

  EXPECT_OK(RunUntilEqual(
      [&]() {
        return histogram_tester.GetBucketCount(
            "Glic.InvokeResult.WebDragDrop",
            static_cast<int>(GlicInvokeResult::kSuccess));
      },
      1));
  histogram_tester.ExpectUniqueSample("Glic.DragAndDrop.ContentType",
                                      GlicDragAndDropContentType::kImage, 1);
  histogram_tester.ExpectUniqueSample("Glic.DragAndDrop.ValidationResult",
                                      GlicDragAndDropValidationResult::kSuccess,
                                      1);
}

// Web-to-Glic drag-and-drop OSExchangeData custom data simulation is supported
// on macOS, Windows, and ChromeOS, and disabled on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_testWebToGlicDragStripsDomDropPayload \
  DISABLED_testWebToGlicDragStripsDomDropPayload
#else
#define MAYBE_testWebToGlicDragStripsDomDropPayload \
  testWebToGlicDragStripsDomDropPayload
#endif
IN_PROC_BROWSER_TEST_P(GlicWebDragAndDropBrowserTest,
                       MAYBE_testWebToGlicDragStripsDomDropPayload) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * glic_instance,
                       OpenGlicForActiveTab());
  Host* glic_host = &glic_instance->host();
  PrepareGuestForDrag(*glic_host);
  gfx::Point host_relative_point = GetGuestCenterInHost(*glic_host);

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  base::FilePath test_file = CreateTestFile(
      temp_dir, "secret.txt", "This content must not reach the guest DOM.");

  auto data = std::make_unique<ui::OSExchangeData>();
  data->SetFilename(test_file);
  data->SetString(u"Sensitive text");
  data->SetURL(GURL("https://a.com/secret.png"), u"secret.png");
  data->SetChromeDragId(base::UnguessableToken::Create());

  drag_and_drop_test_utils::DragAndDropSimulator simulator(
      glic_host->webui_contents());
  ASSERT_TRUE(
      simulator.SimulateDragEnter(host_relative_point, std::move(data)));

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

INSTANTIATE_TEST_SUITE_P(All,
                         GlicWebDragAndDropBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "NoWebview" : "Webview";
                         });

}  // namespace
}  // namespace glic
