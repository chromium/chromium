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
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_delegate.h"
#include "chrome/browser/glic/host/glic_drag_and_drop_util.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/service/metrics/glic_invoke_metrics.h"
#include "chrome/browser/glic/service/metrics/metrics_types.h"
#include "chrome/browser/glic/test_support/glic_api_test.h"
#include "chrome/browser/glic/test_support/glic_drag_and_drop_test_base.h"
#include "chrome/browser/glic/test_support/scoped_page_context_eligibility.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/drag_and_drop_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
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

 private:
  ScopedPageContextEligibilityForTesting scoped_eligibility_;
};

// Linux does not natively support direct in-memory FileContents retrieval
// inside OSExchangeData.
#if BUILDFLAG(IS_LINUX)
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
            "Glic.InvokeResult2.WebDragDrop",
            static_cast<int>(GlicInvokeResult::kSuccess));
      },
      1));
  histogram_tester.ExpectUniqueSample("Glic.DragAndDrop.ContentType",
                                      GlicDragAndDropContentType::kImage, 1);
  histogram_tester.ExpectUniqueSample("Glic.DragAndDrop.ValidationResult2",
                                      GlicDragAndDropValidationResult::kSuccess,
                                      1);
}

// Linux does not natively support direct in-memory FileContents retrieval
// inside OSExchangeData.
#if BUILDFLAG(IS_LINUX)
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
            "Glic.InvokeResult2.WebDragDrop",
            static_cast<int>(GlicInvokeResult::kSuccess));
      },
      1));
  histogram_tester.ExpectUniqueSample("Glic.DragAndDrop.ContentType",
                                      GlicDragAndDropContentType::kImage, 1);
  histogram_tester.ExpectUniqueSample("Glic.DragAndDrop.ValidationResult2",
                                      GlicDragAndDropValidationResult::kSuccess,
                                      1);
}

IN_PROC_BROWSER_TEST_P(GlicWebDragAndDropBrowserTest,
                       testWebToGlicDragStripsDomDropPayload) {
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

IN_PROC_BROWSER_TEST_P(GlicWebDragAndDropBrowserTest,
                       testWebToGlicHyperlinkDragRejected) {
  base::HistogramTester histogram_tester;
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * glic_instance,
                       OpenGlicForActiveTab());
  Host* glic_host = &glic_instance->host();
  PrepareGuestForDrag(*glic_host);
  gfx::Point host_relative_point = GetGuestCenterInHost(*glic_host);

  base::UnguessableToken drag_id = base::UnguessableToken::Create();

  auto data = std::make_unique<ui::OSExchangeData>();
  data->SetString(u"Wikipedia hyperlink");
  data->SetURL(GURL("https://en.wikipedia.org/wiki/Main_Page"),
               u"Wikipedia hyperlink");
  data->SetChromeDragId(drag_id);

  drag_and_drop_test_utils::DragAndDropSimulator simulator(
      glic_host->webui_contents());
  ASSERT_TRUE(
      simulator.SimulateDragEnter(host_relative_point, std::move(data)));
  ASSERT_TRUE(simulator.SimulateDrop(host_relative_point));

  // Drain any pending hit-test and renderer round-trips before checking guest
  // drag state.
  ASSERT_TRUE(content::ExecJs(glic_host->GetGuestMainFrame(), ""));

  // Also verify that if StartDragAndDropInvoke is reached directly with a
  // hyperlink DropData (empty `file_contents`), it rejects the payload with
  // `kUnsupportedContentType` and never records `Glic.InvokeResult.WebDragDrop`
  // (`kClipboardMissingMetadata`).
  content::DropData hyperlink_drop_data;
  hyperlink_drop_data.text = u"Wikipedia hyperlink";
  hyperlink_drop_data.url_infos.emplace_back(
      GURL("https://en.wikipedia.org/wiki/Main_Page"), u"Wikipedia hyperlink");
  hyperlink_drop_data.drag_id = drag_id;
  StartDragAndDropInvoke(glic_host->webui_contents(), hyperlink_drop_data);

  ContinueJsTest();

  histogram_tester.ExpectTotalCount("Glic.InvokeResult.WebDragDrop", 0);
  histogram_tester.ExpectUniqueSample("Glic.DragAndDrop.ContentType",
                                      GlicDragAndDropContentType::kUrl, 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.DragAndDrop.ValidationResult2",
      GlicDragAndDropValidationResult::kUnsupportedContentType, 1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlicWebDragAndDropBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "NoWebview" : "Webview";
                         });

class GlicDragEligibilityBrowserTest : public GlicBrowserTest {
 public:
  explicit GlicDragEligibilityBrowserTest(bool enable_web_drag = true) {
    if (enable_web_drag) {
      feature_list_.InitWithFeatures({features::kGlicDragAndDropFileUpload,
                                      features::kGlicWebDragAndDropFileUpload},
                                     {});
    } else {
      feature_list_.InitWithFeatures({features::kGlicDragAndDropFileUpload},
                                     {features::kGlicWebDragAndDropFileUpload});
    }
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    GlicBrowserTest::SetUpOnMainThread();
  }

 private:
  ScopedPageContextEligibilityForTesting scoped_eligibility_;
  base::test::ScopedFeatureList feature_list_;
};

class GlicDragEligibilityDisabledBrowserTest
    : public GlicDragEligibilityBrowserTest {
 public:
  GlicDragEligibilityDisabledBrowserTest()
      : GlicDragEligibilityBrowserTest(/*enable_web_drag=*/false) {}
};

IN_PROC_BROWSER_TEST_F(GlicDragEligibilityBrowserTest,
                       AllowDragFromEligibleTab) {
  base::HistogramTester histogram_tester;
  const GURL url = embedded_test_server()->GetURL(
      "a.com", "/drag_and_drop/image_source.html");
  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(content::NavigateToURL(active_tab->GetContents(), url));

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * glic_instance,
                       OpenGlicForActiveTab());
  content::WebContents* glic_webui = glic_instance->host().webui_contents();
  ASSERT_TRUE(glic_webui);

  content::DropData drop_data;
  drop_data.file_contents = {1, 2, 3, 4};
  drop_data.url_infos.emplace_back(
      embedded_test_server()->GetURL("a.com",
                                     "/drag_and_drop/cors-allowed.jpg"),
      u"cors-allowed.jpg");

  content::SimulateDragStartForTesting(
      active_tab->GetContents()->GetPrimaryMainFrame(), &drop_data);
  glic::StartDragAndDropInvoke(glic_webui, drop_data);

  histogram_tester.ExpectUniqueSample("Glic.DragAndDrop.ValidationResult2",
                                      GlicDragAndDropValidationResult::kSuccess,
                                      1);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetBucketCount(
               "Glic.InvokeResult2.WebDragDrop",
               static_cast<int>(GlicInvokeResult::kSuccess)) == 1;
  }));
}

IN_PROC_BROWSER_TEST_F(GlicDragEligibilityBrowserTest,
                       BlockDragFromIneligibleTab) {
  base::HistogramTester histogram_tester;
  const GURL url = embedded_test_server()->GetURL(
      "ineligible.com", "/drag_and_drop/image_source.html");
  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(content::NavigateToURL(active_tab->GetContents(), url));

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * glic_instance,
                       OpenGlicForActiveTab());

  content::DropData drop_data;
  drop_data.file_contents = {1, 2, 3, 4};
  drop_data.url_infos.emplace_back(
      embedded_test_server()->GetURL("ineligible.com",
                                     "/drag_and_drop/cors-allowed.jpg"),
      u"cors-allowed.jpg");

  content::SimulateDragStartForTesting(
      active_tab->GetContents()->GetPrimaryMainFrame(), &drop_data);
  content::WebContents* glic_webui = glic_instance->host().webui_contents();
  ASSERT_TRUE(glic_webui);
  glic::StartDragAndDropInvoke(glic_webui, drop_data);

  histogram_tester.ExpectUniqueSample(
      "Glic.DragAndDrop.ValidationResult2",
      GlicDragAndDropValidationResult::kPageContextIneligible, 1);
  histogram_tester.ExpectTotalCount("Glic.InvokeResult2.WebDragDrop", 0);
}

IN_PROC_BROWSER_TEST_F(GlicDragEligibilityBrowserTest,
                       BlockDragAfterSourceTabNavigatesToEligible) {
  base::HistogramTester histogram_tester;
  const GURL ineligible_url = embedded_test_server()->GetURL(
      "ineligible.com", "/drag_and_drop/image_source.html");
  const GURL eligible_url = embedded_test_server()->GetURL(
      "a.com", "/drag_and_drop/image_source.html");

  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(
      content::NavigateToURL(active_tab->GetContents(), ineligible_url));

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * glic_instance,
                       OpenGlicForActiveTab());

  content::DropData drop_data;
  drop_data.file_contents = {1, 2, 3, 4};

  // Drag starts while the tab is on an ineligible URL.
  content::SimulateDragStartForTesting(
      active_tab->GetContents()->GetPrimaryMainFrame(), &drop_data);

  // Even if the source tab navigates to an eligible URL before the drop
  // finishes, the drag-start eligibility decision must be preserved.
  ASSERT_TRUE(content::NavigateToURL(active_tab->GetContents(), eligible_url));

  content::WebContents* glic_webui = glic_instance->host().webui_contents();
  ASSERT_TRUE(glic_webui);
  glic::StartDragAndDropInvoke(glic_webui, drop_data);

  histogram_tester.ExpectUniqueSample(
      "Glic.DragAndDrop.ValidationResult2",
      GlicDragAndDropValidationResult::kPageContextIneligible, 1);
  histogram_tester.ExpectTotalCount("Glic.InvokeResult2.WebDragDrop", 0);
}

IN_PROC_BROWSER_TEST_F(GlicDragEligibilityBrowserTest,
                       BlockDragWithUnregisteredDragId) {
  base::HistogramTester histogram_tester;

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * glic_instance,
                       OpenGlicForActiveTab());
  content::WebContents* glic_webui = glic_instance->host().webui_contents();
  ASSERT_TRUE(glic_webui);

  content::DropData drop_data;
  drop_data.drag_id = base::UnguessableToken::Create();
  drop_data.file_contents = {1, 2, 3, 4};

  // A drag ID not registered in GlicDragEligibilityMonitor (e.g. a drag from
  // outside a tab) must be blocked with kUnknownDragSource.
  glic::StartDragAndDropInvoke(glic_webui, drop_data);

  histogram_tester.ExpectUniqueSample(
      "Glic.DragAndDrop.ValidationResult2",
      GlicDragAndDropValidationResult::kUnknownDragSource, 1);
  histogram_tester.ExpectTotalCount("Glic.InvokeResult2.WebDragDrop", 0);
}

IN_PROC_BROWSER_TEST_F(GlicDragEligibilityBrowserTest, BlockDragCrossProfile) {
  base::HistogramTester histogram_tester;
  const GURL url = embedded_test_server()->GetURL(
      "a.com", "/drag_and_drop/image_source.html");

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * glic_instance,
                       OpenGlicForActiveTab());

  BrowserWindowInterface* incognito_browser =
      InProcessBrowserTest::CreateIncognitoBrowser();
  tabs::TabInterface* incognito_tab =
      incognito_browser->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(incognito_tab);
  content::WebContents* incognito_contents = incognito_tab->GetContents();
  ASSERT_TRUE(content::NavigateToURL(incognito_contents, url));

  content::DropData drop_data;
  drop_data.file_contents = {1, 2, 3, 4};

  // The drag starts in an eligible page inside the Incognito BrowserContext
  // and is dropped onto the regular profile's Glic panel.
  content::SimulateDragStartForTesting(
      incognito_contents->GetPrimaryMainFrame(), &drop_data);
  content::WebContents* glic_webui = glic_instance->host().webui_contents();
  ASSERT_TRUE(glic_webui);
  glic::StartDragAndDropInvoke(glic_webui, drop_data);

  histogram_tester.ExpectUniqueSample(
      "Glic.DragAndDrop.ValidationResult2",
      GlicDragAndDropValidationResult::kCrossProfileDragBlocked, 1);
  histogram_tester.ExpectTotalCount("Glic.InvokeResult2.WebDragDrop", 0);
}

IN_PROC_BROWSER_TEST_F(GlicDragEligibilityDisabledBrowserTest,
                       BlockDragWhenFeatureDisabled) {
  base::HistogramTester histogram_tester;
  const GURL url = embedded_test_server()->GetURL(
      "a.com", "/drag_and_drop/image_source.html");
  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(content::NavigateToURL(active_tab->GetContents(), url));

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * glic_instance,
                       OpenGlicForActiveTab());
  content::WebContents* glic_webui = glic_instance->host().webui_contents();
  ASSERT_TRUE(glic_webui);

  content::DropData drop_data;
  drop_data.file_contents = {1, 2, 3, 4};

  content::SimulateDragStartForTesting(
      active_tab->GetContents()->GetPrimaryMainFrame(), &drop_data);
  glic::StartDragAndDropInvoke(glic_webui, drop_data);

  histogram_tester.ExpectUniqueSample(
      "Glic.DragAndDrop.ValidationResult2",
      GlicDragAndDropValidationResult::kBlockedByFeatureFlag, 1);
  histogram_tester.ExpectTotalCount("Glic.InvokeResult2.WebDragDrop", 0);
}

IN_PROC_BROWSER_TEST_F(GlicDragEligibilityBrowserTest,
                       BlockDragAfterSourceFrameDestroyed) {
  base::HistogramTester histogram_tester;
  const GURL url = embedded_test_server()->GetURL("a.com", "/iframe.html");
  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(content::NavigateToURL(active_tab->GetContents(), url));

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * glic_instance,
                       OpenGlicForActiveTab());

  content::RenderFrameHost* main_frame =
      active_tab->GetContents()->GetPrimaryMainFrame();
  content::RenderFrameHost* source_frame = content::ChildFrameAt(main_frame, 0);
  ASSERT_TRUE(source_frame);

  content::DropData drop_data;
  drop_data.file_contents = {1, 2, 3, 4};

  // The drag starts in an eligible subframe, which is then removed from the
  // page before the drop, while its tab and the Glic panel stay open.
  content::SimulateDragStartForTesting(source_frame, &drop_data);
  content::RenderFrameDeletedObserver source_frame_deleted(source_frame);
  ASSERT_TRUE(
      content::ExecJs(main_frame, "document.getElementById('test').remove();"));
  source_frame_deleted.WaitUntilDeleted();

  content::WebContents* glic_webui = glic_instance->host().webui_contents();
  ASSERT_TRUE(glic_webui);
  glic::StartDragAndDropInvoke(glic_webui, drop_data);

  histogram_tester.ExpectUniqueSample(
      "Glic.DragAndDrop.ValidationResult2",
      GlicDragAndDropValidationResult::kSourceFrameDestroyed, 1);
  histogram_tester.ExpectTotalCount("Glic.InvokeResult2.WebDragDrop", 0);
}

}  // namespace
}  // namespace glic
