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
#include "build/android_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/glic/host/glic_drag_and_drop_util.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/service/metrics/glic_invoke_metrics.h"
#include "chrome/browser/glic/service/metrics/metrics_types.h"
#include "chrome/browser/glic/test_support/glic_api_test.h"
#include "chrome/browser/glic/test_support/glic_drag_and_drop_test_base.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/test_support/scoped_page_context_eligibility.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/test/base/drag_and_drop_test_utils.h"
#include "components/glic/glic_pref_names.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#endif

namespace glic {
namespace {

class GlicWebDragAndDropBrowserTest : public GlicDragAndDropTestBase,
                                      public testing::WithParamInterface<bool> {
 public:
  GlicWebDragAndDropBrowserTest()
      : GlicDragAndDropTestBase(
            GlicTestJsPath("./glic_web_drag_and_drop_browsertest.js"),
            GetParam()) {}

  // Drags a real image out of the active tab and drops it on Glic.
  // The drag payload is captured from Blink's production drag path via
  // `DragStartWaiter` and replayed onto the Glic panel while the drag is live.
  void DragWebImageToGlic(Host& glic_host) {
    content::WebContents* source_wc = SetupSourceTabWithDraggableImage();
    ASSERT_TRUE(source_wc);

    drag_and_drop_test_utils::DragAndDropSimulator simulator(
        glic_host.webui_contents());

    bool dropped = false;
    drag_and_drop_test_utils::DragStartWaiter waiter(
        source_wc, base::BindLambdaForTesting([&]() {
          base::ScopedClosureRunner release_runner(base::BindOnce(
              &drag_and_drop_test_utils::DragStartWaiter::ReleaseDrag,
              base::Unretained(&waiter)));
          gfx::Point host_relative_point = GetGuestCenterInHost(glic_host);
          ASSERT_TRUE(simulator.SimulateDragEnter(host_relative_point,
                                                  waiter.TakeCapturedData()));
          // Wait for the guest frame to receive `dragenter` and `dragover`
          // before dropping.
          ASSERT_NO_FATAL_FAILURE(ContinueJsTest());
          host_relative_point = GetGuestCenterInHost(glic_host);
          ASSERT_TRUE(simulator.SimulateDrop(host_relative_point));
          dropped = true;
        }));
    waiter.SuppressPassingStartDragFurther();

    ASSERT_NO_FATAL_FAILURE(SimulateMouseDragFromImage(source_wc));
    waiter.WaitUntilDragStart();
    ASSERT_TRUE(dropped);
  }

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

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * glic_instance,
                       OpenGlicForActiveTab());
  Host* glic_host = &glic_instance->host();
  ASSERT_NO_FATAL_FAILURE(PrepareGuestForDrag(*glic_host));

  ASSERT_NO_FATAL_FAILURE(DragWebImageToGlic(*glic_host));

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

#if !BUILDFLAG(IS_ANDROID)
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

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * glic_instance,
                       OpenGlicForActiveTabAndDetach());
  Host* glic_host = &glic_instance->host();
  ASSERT_NO_FATAL_FAILURE(PrepareGuestForDrag(*glic_host));

  ASSERT_NO_FATAL_FAILURE(DragWebImageToGlic(*glic_host));

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
  ASSERT_NO_FATAL_FAILURE(PrepareGuestForDrag(*glic_host));
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
  ASSERT_NO_FATAL_FAILURE(PrepareGuestForDrag(*glic_host));
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
  // `kUnsupportedContentType` and never records
  // `Glic.InvokeResult2.WebDragDrop` (`kClipboardMissingMetadata`).
  content::DropData hyperlink_drop_data;
  hyperlink_drop_data.text = u"Wikipedia hyperlink";
  hyperlink_drop_data.url_infos.emplace_back(
      GURL("https://en.wikipedia.org/wiki/Main_Page"), u"Wikipedia hyperlink");
  hyperlink_drop_data.drag_id = drag_id;
  StartDragAndDropInvoke(glic_host->webui_contents(), hyperlink_drop_data);

  ContinueJsTest();

  histogram_tester.ExpectTotalCount("Glic.InvokeResult2.WebDragDrop", 0);
  histogram_tester.ExpectUniqueSample("Glic.DragAndDrop.ContentType",
                                      GlicDragAndDropContentType::kUrl, 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.DragAndDrop.ValidationResult2",
      GlicDragAndDropValidationResult::kUnsupportedContentType, 1);
}
#endif  // !BUILDFLAG(IS_ANDROID)

INSTANTIATE_TEST_SUITE_P(All,
                         GlicWebDragAndDropBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "NoWebview" : "Webview";
                         });

class GlicDragEligibilityBrowserTest : public GlicBrowserTest {
 public:
  explicit GlicDragEligibilityBrowserTest(bool enable_web_drag = true) {
    SetGlicPagePath("/glic/test_client/index.html");
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

 protected:
  content::WebContents* InstallBlockedNoticesRecorder(
      GlicInstanceImpl* glic_instance) {
    EXPECT_OK(WaitForGlicClient());
    content::WebContents* glic_guest =
        glic_instance->host().web_client_contents();
    EXPECT_TRUE(glic_guest);
    EXPECT_TRUE(content::ExecJs(glic_guest, R"(
      (async () => {
        window.__capturedBlockedNotices = [];
        await window.client.getInitialized();
        window.client.browser.getDataTransferActionsBlockedNotices().subscribe(
            notice => window.__capturedBlockedNotices.push(notice));
      })()
    )"));
    return glic_guest;
  }

  base::ListValue GetCapturedBlockedNotices(content::WebContents* glic_guest) {
    return content::EvalJs(glic_guest, "window.__capturedBlockedNotices")
        .TakeValue()
        .TakeList();
  }

  void ExpectSingleBlockedDropNotice(
      content::WebContents* glic_guest,
      mojom::DataTransferActionsBlockedReason expected_reason) {
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return GetCapturedBlockedNotices(glic_guest).size() == 1u; }));
    base::ListValue notices = GetCapturedBlockedNotices(glic_guest);
    ASSERT_EQ(notices.size(), 1u);
    EXPECT_EQ(notices[0].GetDict().FindInt("action"),
              static_cast<int>(mojom::DataTransferAction::kDrop));
    EXPECT_EQ(notices[0].GetDict().FindInt("reason"),
              static_cast<int>(expected_reason));
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
  content::WebContents* glic_guest =
      InstallBlockedNoticesRecorder(glic_instance);
  ASSERT_TRUE(glic_guest);
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
  EXPECT_TRUE(GetCapturedBlockedNotices(glic_guest).empty());

  // `GlicDragEligibilityMonitor::TakeDragSourceInfo` consumes the entry on the
  // first lookup, so replaying the same `drag_id` must fail closed. Re-query
  // `webui_contents()` in case `GlicNoWebviewContentsManager` swapped out the
  // loading overlay during `RunUntil`.
  glic_webui = glic_instance->host().webui_contents();
  ASSERT_TRUE(glic_webui);
  glic::StartDragAndDropInvoke(glic_webui, drop_data);
  histogram_tester.ExpectBucketCount(
      "Glic.DragAndDrop.ValidationResult2",
      GlicDragAndDropValidationResult::kUnknownDragSource, 1);
  histogram_tester.ExpectTotalCount("Glic.DragAndDrop.ValidationResult2", 2);
  histogram_tester.ExpectTotalCount("Glic.InvokeResult2.WebDragDrop", 1);
  ExpectSingleBlockedDropNotice(
      glic_guest,
      mojom::DataTransferActionsBlockedReason::kUnsupportedOrExpiredSource);
}

IN_PROC_BROWSER_TEST_F(GlicDragEligibilityBrowserTest,
                       BlockDragBeforeFreCompleted) {
  base::HistogramTester histogram_tester;
  const GURL url = embedded_test_server()->GetURL(
      "a.com", "/drag_and_drop/image_source.html");
  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(content::NavigateToURL(active_tab->GetContents(), url));

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * glic_instance,
                       OpenGlicForActiveTab());
  content::WebContents* glic_webui = glic_instance->host().webui_contents();
  ASSERT_TRUE(glic_webui);

  // Reset the FRE after the panel is open, as a pre-consent (e.g. action-first
  // FRE) user would have it.
  GlicKeyedService::Get(GetProfile())
      ->enabling()
      .SetCompletedFre(prefs::FreStatus::kNotStarted);
  ASSERT_FALSE(GlicEnabling::HasConsentedForProfile(GetProfile()));

  content::DropData drop_data;
  drop_data.file_contents = {1, 2, 3, 4};
  drop_data.url_infos.emplace_back(
      embedded_test_server()->GetURL("a.com",
                                     "/drag_and_drop/cors-allowed.jpg"),
      u"cors-allowed.jpg");
  content::SimulateDragStartForTesting(
      active_tab->GetContents()->GetPrimaryMainFrame(), &drop_data);
  glic::StartDragAndDropInvoke(glic_webui, drop_data);

  histogram_tester.ExpectUniqueSample(
      "Glic.DragAndDrop.ValidationResult2",
      GlicDragAndDropValidationResult::kFreNotCompleted, 1);
  histogram_tester.ExpectTotalCount("Glic.InvokeResult2.WebDragDrop", 0);
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
  content::WebContents* glic_guest =
      InstallBlockedNoticesRecorder(glic_instance);
  ASSERT_TRUE(glic_guest);

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
  ExpectSingleBlockedDropNotice(
      glic_guest,
      mojom::DataTransferActionsBlockedReason::kIneligiblePageContext);
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
  content::WebContents* glic_guest =
      InstallBlockedNoticesRecorder(glic_instance);
  ASSERT_TRUE(glic_guest);

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
  ExpectSingleBlockedDropNotice(
      glic_guest,
      mojom::DataTransferActionsBlockedReason::kIneligiblePageContext);
}

IN_PROC_BROWSER_TEST_F(GlicDragEligibilityBrowserTest,
                       BlockDragWithUnregisteredDragId) {
  base::HistogramTester histogram_tester;

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * glic_instance,
                       OpenGlicForActiveTab());
  content::WebContents* glic_guest =
      InstallBlockedNoticesRecorder(glic_instance);
  ASSERT_TRUE(glic_guest);
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
  ExpectSingleBlockedDropNotice(
      glic_guest,
      mojom::DataTransferActionsBlockedReason::kUnsupportedOrExpiredSource);
}

// Mobile Android does not support programmatically creating separate browser
// window tasks with an OTR profile via `CreateBrowserWindow`.
#if !BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_DESKTOP_ANDROID)
IN_PROC_BROWSER_TEST_F(GlicDragEligibilityBrowserTest, AllowDragFromIncognito) {
  base::HistogramTester histogram_tester;
  const GURL url = embedded_test_server()->GetURL(
      "a.com", "/drag_and_drop/image_source.html");

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * glic_instance,
                       OpenGlicForActiveTab());
  content::WebContents* glic_guest =
      InstallBlockedNoticesRecorder(glic_instance);
  ASSERT_TRUE(glic_guest);

  Profile* incognito_profile =
      GetProfile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  BrowserWindowInterface* incognito_browser =
      glic::CreateBrowserWindow(incognito_profile);
  ASSERT_TRUE(incognito_browser);
  ASSERT_TRUE(incognito_browser->GetProfile()->IsOffTheRecord());
  tabs::TabInterface* incognito_tab =
      CreateAndActivateTab(incognito_browser, url);
  ASSERT_TRUE(incognito_tab);
  content::WebContents* incognito_contents = incognito_tab->GetContents();

  content::DropData drop_data;
  drop_data.file_contents = {1, 2, 3, 4};
  drop_data.url_infos.emplace_back(
      embedded_test_server()->GetURL("a.com",
                                     "/drag_and_drop/cors-allowed.jpg"),
      u"cors-allowed.jpg");

  // With kGlicCrossProfileTransfer enabled (the default), a drag that starts in
  // an eligible Incognito page is accepted by the regular profile's Glic panel.
  content::SimulateDragStartForTesting(
      incognito_contents->GetPrimaryMainFrame(), &drop_data);
  content::WebContents* glic_webui = glic_instance->host().webui_contents();
  ASSERT_TRUE(glic_webui);
  glic::StartDragAndDropInvoke(glic_webui, drop_data);

  histogram_tester.ExpectUniqueSample("Glic.DragAndDrop.ValidationResult2",
                                      GlicDragAndDropValidationResult::kSuccess,
                                      1);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetBucketCount(
               "Glic.InvokeResult2.WebDragDrop",
               static_cast<int>(GlicInvokeResult::kSuccess)) == 1;
  }));
  EXPECT_TRUE(GetCapturedBlockedNotices(glic_guest).empty());
}
#endif  // !BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_DESKTOP_ANDROID)

// Android browser tests do not support `PlatformBrowserTest::CreateBrowser` or
// `GenerateNextProfileDirectoryPath()` without `kAndroidMultiprofile`.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(GlicDragEligibilityBrowserTest, AllowDragCrossProfile) {
#if BUILDFLAG(IS_CHROMEOS)
  // ChromeOS profiles are tied to OS users; CreateProfileSync() CHECK-fails in
  // CreateUserCloudPolicyManagerAsh() in browser tests.
  GTEST_SKIP() << "Can't create a second regular profile on ChromeOS";
#else
  base::HistogramTester histogram_tester;
  const GURL url = embedded_test_server()->GetURL(
      "a.com", "/drag_and_drop/image_source.html");

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * glic_instance,
                       OpenGlicForActiveTab());
  content::WebContents* glic_guest =
      InstallBlockedNoticesRecorder(glic_instance);
  ASSERT_TRUE(glic_guest);

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile& other_profile = profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GenerateNextProfileDirectoryPath());
  BrowserWindowInterface* other_browser =
      PlatformBrowserTest::CreateBrowser(&other_profile);
  tabs::TabInterface* other_tab =
      TabListInterface::From(other_browser)->GetActiveTab();
  ASSERT_TRUE(other_tab);
  content::WebContents* other_contents = other_tab->GetContents();
  ASSERT_TRUE(content::NavigateToURL(other_contents, url));

  content::DropData drop_data;
  drop_data.file_contents = {1, 2, 3, 4};
  drop_data.url_infos.emplace_back(
      embedded_test_server()->GetURL("a.com",
                                     "/drag_and_drop/cors-allowed.jpg"),
      u"cors-allowed.jpg");

  // With kGlicCrossProfileTransfer enabled (the default), a drag that starts in
  // an eligible page in another profile is accepted by this profile's Glic
  // panel.
  content::SimulateDragStartForTesting(other_contents->GetPrimaryMainFrame(),
                                       &drop_data);
  content::WebContents* glic_webui = glic_instance->host().webui_contents();
  ASSERT_TRUE(glic_webui);
  glic::StartDragAndDropInvoke(glic_webui, drop_data);

  histogram_tester.ExpectUniqueSample("Glic.DragAndDrop.ValidationResult2",
                                      GlicDragAndDropValidationResult::kSuccess,
                                      1);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetBucketCount(
               "Glic.InvokeResult2.WebDragDrop",
               static_cast<int>(GlicInvokeResult::kSuccess)) == 1;
  }));
  EXPECT_TRUE(GetCapturedBlockedNotices(glic_guest).empty());
#endif
}
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(GlicDragEligibilityDisabledBrowserTest,
                       BlockDragWhenFeatureDisabled) {
  base::HistogramTester histogram_tester;
  const GURL url = embedded_test_server()->GetURL(
      "a.com", "/drag_and_drop/image_source.html");
  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(content::NavigateToURL(active_tab->GetContents(), url));

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * glic_instance,
                       OpenGlicForActiveTab());
  content::WebContents* glic_guest =
      InstallBlockedNoticesRecorder(glic_instance);
  ASSERT_TRUE(glic_guest);
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
  EXPECT_TRUE(GetCapturedBlockedNotices(glic_guest).empty());
}

IN_PROC_BROWSER_TEST_F(GlicDragEligibilityBrowserTest,
                       BlockDragAfterSourceFrameDestroyed) {
  base::HistogramTester histogram_tester;
  const GURL url = embedded_test_server()->GetURL("a.com", "/iframe.html");
  tabs::TabInterface* active_tab = GetTabListInterface()->GetActiveTab();
  ASSERT_TRUE(content::NavigateToURL(active_tab->GetContents(), url));

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * glic_instance,
                       OpenGlicForActiveTab());
  content::WebContents* glic_guest =
      InstallBlockedNoticesRecorder(glic_instance);
  ASSERT_TRUE(glic_guest);

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
  ExpectSingleBlockedDropNotice(
      glic_guest,
      mojom::DataTransferActionsBlockedReason::kUnsupportedOrExpiredSource);
}

}  // namespace
}  // namespace glic
