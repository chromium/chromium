// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/android_info.h"
#include "base/android/device_info.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/escape.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/glic/host/context/glic_sharing_utils.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_api_test.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/page_content_annotations/content/page_context_fetcher_metrics.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/filename_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
#include "components/enterprise/data_controls/core/browser/test_utils.h"
#endif

namespace glic {

namespace {

class GlicAndroidMojoBrowserTest : public GlicApiBrowserTest {
 public:
  GlicAndroidMojoBrowserTest()
      : GlicApiBrowserTest(GlicTestJsPath("./glic_android_browsertest.js")) {}
};

// Tests that page context fetching can query the URL of the focused browser
// tab.
IN_PROC_BROWSER_TEST_F(GlicAndroidMojoBrowserTest, testPageContextFetching) {
  // 1. Setup mock page to query context from
  GURL page_url = GetTestUrl("page.html");
  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(), page_url));

  // 2. Open Glic and wait for client to bind
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_TRUE(WaitForGlicClient(instance).has_value());

  // Enable default tab context sharing
  GetProfile()->GetPrefs()->SetBoolean(prefs::kGlicDefaultTabContextEnabled,
                                       true);

  // 3. Trigger TypeScript test method via GlicApiBrowserTest helper
  ExecuteJsTest();
}

// Tests that page context fetching includes PDF document data when the focused
// tab is viewing a PDF document.
IN_PROC_BROWSER_TEST_F(GlicAndroidMojoBrowserTest,
                       testPageContextFetchingWithPdf) {
  if (base::android::android_info::sdk_int() <
      base::android::android_info::SDK_VERSION_V) {
    GTEST_SKIP() << "Android inline PDF requires Android V+.";
  }

  // 1. Setup a temporary PDF file and navigate to its native PDF page URL.
  static constexpr char kPdfContent[] =
      "%PDF-1.4\n1 0 obj <<>> endobj\ntrailer <<>>\n%%EOF\n";
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath pdf_path = temp_dir.GetPath().AppendASCII("test.pdf");
  ASSERT_TRUE(base::WriteFile(pdf_path, kPdfContent));

  GURL file_url = net::FilePathToFileURL(pdf_path);
  GURL pdf_url(
      "chrome-native://pdf/link?url=" +
      base::EscapeQueryParamValue(file_url.spec(), /*use_plus=*/false));
  TabAndroid* tab_android =
      TabAndroid::FromTabInterface(GetTabListInterface()->GetActiveTab());
  ASSERT_TRUE(tab_android);
  tab_android->SetPdfNativePageForTesting(pdf_url.spec(), pdf_path.value());
  ASSERT_TRUE(content::NavigateToURL(
      GetTabListInterface()->GetActiveTab()->GetContents(), pdf_url));
  ASSERT_TRUE(tab_android->IsPdf());

  // 2. Open Glic and wait for client to bind
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_TRUE(WaitForGlicClient(instance).has_value());

  // Enable default tab context sharing
  GetProfile()->GetPrefs()->SetBoolean(prefs::kGlicDefaultTabContextEnabled,
                                       true);

  // 3. Trigger TypeScript test method via GlicApiBrowserTest helper. The test
  // verifies that the fetched PDF bytes match the content of the PDF file.
  base::HistogramTester histogram_tester;
  ExecuteJsTest({.params = base::Value(kPdfContent)});
  tab_android->SetPdfNativePageForTesting("", "");

  // 4. Verify that the PDF fetch metrics are recorded once, for the single
  // fetch requested by the TypeScript test.
  histogram_tester.ExpectUniqueSample(
      page_content_annotations::kPdfBytesTopLevelSizeLimitExceededHistogram,
      false, 1);
  histogram_tester.ExpectTotalCount(
      page_content_annotations::kPdfBytesTopLevelLatencyHistogram, 1);
  histogram_tester.ExpectUniqueSample(
      page_content_annotations::kPdfBytesTopLevelSizeHistogram, 0, 1);
  histogram_tester.ExpectUniqueSample(
      page_content_annotations::kPdfContentsRequestedHistogram,
      page_content_annotations::PdfRequestStates::kPdfMainDoc_PdfFound, 1);
}

// Tests that page context fetching for a PDF downloaded from the web reports
// the title of the native PDF page, rather than the URL.
IN_PROC_BROWSER_TEST_F(GlicAndroidMojoBrowserTest,
                       testPageContextFetchingWithWebPdf) {
  if (base::android::android_info::sdk_int() <
      base::android::android_info::SDK_VERSION_V) {
    GTEST_SKIP() << "Android inline PDF requires Android V+.";
  }

  static constexpr char kPdfContent[] =
      "%PDF-1.4\n1 0 obj <<>> endobj\ntrailer <<>>\n%%EOF\n";
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath pdf_path = temp_dir.GetPath().AppendASCII("test.pdf");
  ASSERT_TRUE(base::WriteFile(pdf_path, kPdfContent));

  // Mimic `DownloadController#onPdfDownloadStarted()`, which loads the native
  // PDF page URL with the downloaded PDF URL as the virtual URL.
  GURL download_url("https://example.com/test.pdf");
  GURL pdf_url(
      "chrome-native://pdf/link?url=" +
      base::EscapeQueryParamValue(download_url.spec(), /*use_plus=*/false));
  content::WebContents* web_contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  // Start from a committed page, like a real PDF download does. On slow
  // devices, the startup about:blank navigation of the initial tab may still be
  // committing when the test body starts. That commit discards the pending
  // navigation entry of the PDF navigation below, so the PDF navigation then
  // commits a new entry without the virtual URL.
  ASSERT_TRUE(content::NavigateToURL(web_contents, GetTestUrl("page.html")));

  TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents);
  ASSERT_TRUE(tab_android);
  tab_android->SetPdfNativePageForTesting(pdf_url.spec(), pdf_path.value());
  content::NavigationController::LoadURLParams params(pdf_url);
  params.load_type = content::NavigationController::LOAD_TYPE_PDF_ANDROID;
  params.virtual_url_for_special_cases = download_url;
  params.is_pdf = true;
  content::TestNavigationObserver observer(web_contents);
  web_contents->GetController().LoadURLWithParams(params);
  observer.Wait();
  ASSERT_EQ(download_url, web_contents->GetLastCommittedURL());
  ASSERT_TRUE(tab_android->IsPdf());
  // The native PDF page title is pushed to the navigation entry, even though
  // the native page URL differs from the virtual URL.
  EXPECT_EQ(u"test.pdf", web_contents->GetTitle());

  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_TRUE(WaitForGlicClient(instance).has_value());
  GetProfile()->GetPrefs()->SetBoolean(prefs::kGlicDefaultTabContextEnabled,
                                       true);

  ExecuteJsTest({.params = base::Value(kPdfContent)});
  tab_android->SetPdfNativePageForTesting("", "");
}

#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
// Tests that screenshot Data Controls rules are matched against the backing
// file URL of a local PDF tab (whose committed URL is the opaque
// `chrome-native://pdf` URL) and against the download URL of a web PDF tab.
IN_PROC_BROWSER_TEST_F(GlicAndroidMojoBrowserTest,
                       testScreenshotDataControlsMatchPdfTabUrl) {
  if (base::android::android_info::sdk_int() <
      base::android::android_info::SDK_VERSION_V) {
    GTEST_SKIP() << "Android inline PDF requires Android V+.";
  }

  static constexpr char kPdfContent[] =
      "%PDF-1.4\n1 0 obj <<>> endobj\ntrailer <<>>\n%%EOF\n";
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath pdf_path = temp_dir.GetPath().AppendASCII("test.pdf");
  ASSERT_TRUE(base::WriteFile(pdf_path, kPdfContent));

  content::WebContents* web_contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  TabAndroid* tab_android = TabAndroid::FromWebContents(web_contents);
  ASSERT_TRUE(tab_android);

  // A rule for file URLs blocks a local PDF tab, which commits the opaque
  // native page URL rather than the backing file URL.
  GURL file_url = net::FilePathToFileURL(pdf_path);
  GURL local_pdf_url(
      "chrome-native://pdf/link?url=" +
      base::EscapeQueryParamValue(file_url.spec(), /*use_plus=*/false));
  tab_android->SetPdfNativePageForTesting(local_pdf_url.spec(),
                                          pdf_path.value());
  ASSERT_TRUE(content::NavigateToURL(web_contents, local_pdf_url));
  ASSERT_TRUE(tab_android->IsPdf());
  ASSERT_FALSE(web_contents->GetLastCommittedURL().SchemeIsFile());
  EXPECT_FALSE(IsScreenshotBlockedByEnterprisePolicy(web_contents));
  data_controls::SetDataControls(GetProfile()->GetPrefs(), {R"({
    "sources": {
      "urls": ["file://*"]
    },
    "restrictions": [
      {"class": "SCREENSHOT", "level": "BLOCK"}
    ]
  })"});
  EXPECT_TRUE(IsScreenshotBlockedByEnterprisePolicy(web_contents));
  tab_android->SetPdfNativePageForTesting("", "");

  // A web PDF tab commits its download URL as the virtual URL, so the file
  // rule does not apply even though the PDF is backed by a cache file; a rule
  // for the download host does.
  ASSERT_TRUE(content::NavigateToURL(web_contents, GetTestUrl("page.html")));
  GURL download_url("https://example.com/test.pdf");
  GURL web_pdf_url(
      "chrome-native://pdf/link?url=" +
      base::EscapeQueryParamValue(download_url.spec(), /*use_plus=*/false));
  tab_android->SetPdfNativePageForTesting(web_pdf_url.spec(), pdf_path.value());
  content::NavigationController::LoadURLParams params(web_pdf_url);
  params.load_type = content::NavigationController::LOAD_TYPE_PDF_ANDROID;
  params.virtual_url_for_special_cases = download_url;
  params.is_pdf = true;
  content::TestNavigationObserver observer(web_contents);
  web_contents->GetController().LoadURLWithParams(params);
  observer.Wait();
  ASSERT_EQ(download_url, web_contents->GetLastCommittedURL());
  ASSERT_TRUE(tab_android->IsPdf());
  EXPECT_FALSE(IsScreenshotBlockedByEnterprisePolicy(web_contents));
  data_controls::SetDataControls(GetProfile()->GetPrefs(), {R"({
    "sources": {
      "urls": ["example.com"]
    },
    "restrictions": [
      {"class": "SCREENSHOT", "level": "BLOCK"}
    ]
  })"});
  EXPECT_TRUE(IsScreenshotBlockedByEnterprisePolicy(web_contents));
  tab_android->SetPdfNativePageForTesting("", "");
}
#endif  // BUILDFLAG(ENTERPRISE_DATA_CONTROLS)

// Tests that Mojo pipe stays resilient when device screen orientation changes.
IN_PROC_BROWSER_TEST_F(GlicAndroidMojoBrowserTest,
                       testDeviceRotationMojoResiliency) {
  // TODO (crbug.com/545025371): The test is consistenty failing on automotive
  // bot. Disabled by the gardener.
  if (base::android::device_info::is_desktop() ||
      base::android::device_info::is_automotive()) {
    GTEST_SKIP() << "Screen rotation is not applicable on desktop Android.";
  }

  content::WebContents* tab_contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  ASSERT_TRUE(tab_contents);
  ASSERT_TRUE(content::NavigateToURL(tab_contents, GetTestUrl("page.html")));

  // 1. Open Glic and wait for client connection
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_TRUE(WaitForGlicClient(instance).has_value());

  // Execute TypeScript test helper to confirm connection is ready.
  ExecuteJsTest();

  // 2. Simulate device rotation to landscape (SCREEN_ORIENTATION_LANDSCAPE = 0)
  SetActivityOrientationForTesting(tab_contents, 0);

  // 3. Ensure Glic client Mojo connection remains bound after rotation
  EXPECT_TRUE(RunUntil(
      [this]() { return GetOnlyGlicInstance()->host().IsWebClientConnected(); },
      "Mojo connection disconnected post-rotation"));

  // 4. Restore original portrait orientation (SCREEN_ORIENTATION_PORTRAIT = 1)
  SetActivityOrientationForTesting(tab_contents, 1);

  EXPECT_TRUE(RunUntil(
      [this]() { return GetOnlyGlicInstance()->host().IsWebClientConnected(); },
      "Mojo connection disconnected post-rotation restore"));
}

class GlicAndroidReloadFocusBrowserTest
    : public GlicAndroidMojoBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  GlicAndroidReloadFocusBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kGlicNativeLoadingPlaceholder, GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Regression test for b/583963962. Reloading an open Glic panel on Android
// preserves focus when Glic held focus, and does not steal focus if the tab
// takes focus either mid-reload or prior to reload.
IN_PROC_BROWSER_TEST_P(GlicAndroidReloadFocusBrowserTest,
                       testFocusPreservedOnReload) {
  content::WebContents* tab_contents =
      GetTabListInterface()->GetActiveTab()->GetContents();
  ASSERT_TRUE(tab_contents);
  ASSERT_TRUE(content::NavigateToURL(tab_contents, GetTestUrl("page.html")));

  ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitUntilGuestIsShowing(instance));

  // Wait for `#inputBox` autofocus without synthesizing sticky user activation
  // (`ExecuteJsTest()` uses default `EvalJs` options which set user activation
  // and cause a concurrent `FlushAutofocusCandidates()` frame to request the
  // Android soft keyboard, collapsing `TabBottomSheetCoordinator` if focus
  // leaves the bottom sheet while the IME animation is in flight).
  auto prepare_guest_input_without_ime = [&]() {
    content::RenderFrameHost* guest_frame =
        instance->host().GetGuestMainFrame();
    ASSERT_TRUE(guest_frame);
    ASSERT_EQ(true, content::EvalJs(guest_frame,
                                    R"js(new Promise(resolve => {
                          const check = () => {
                            const input = document.getElementById('inputBox');
                            if (input) {
                              input.inputMode = 'none';
                            }
                            if (document.activeElement?.id === 'inputBox') {
                              resolve(true);
                            } else {
                              requestAnimationFrame(check);
                            }
                          };
                          check();
                        }))js",
                                    content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  };

  // Reloading while the tab holds focus must not steal focus into Glic.
  tab_contents->Focus();
  ASSERT_OK(RunUntilEqual<bool>([&]() { return instance->HasFocus(); }, false,
                                "Expected Glic not to hold focus"));
  instance->host().Reload();
  ASSERT_OK(WaitUntilGuestIsShowing(instance));
  EXPECT_FALSE(instance->HasFocus());

  // Reloading while Glic holds focus must preserve focus in the replacement
  // guest.
  ASSERT_NO_FATAL_FAILURE(prepare_guest_input_without_ime());
  ASSERT_OK(FocusGlic(instance));
  ExecuteJsTest();

  instance->host().Reload();
  ASSERT_OK(WaitUntilGuestIsShowing(instance));
  ASSERT_NO_FATAL_FAILURE(prepare_guest_input_without_ime());
  ExecuteJsTest();

  // Focusing the tab mid-reload (while loading overlay or native placeholder
  // is active) must not be clobbered when the replacement guest finishes
  // loading.
  instance->host().Reload();
  EXPECT_TRUE(instance->HasFocus());
  tab_contents->Focus();
  EXPECT_FALSE(instance->HasFocus());
  ASSERT_OK(WaitUntilGuestIsShowing(instance));
  EXPECT_FALSE(instance->HasFocus());

  // Re-focus Glic, then reload and move focus onto a native Android View
  // outside the Glic container mid-reload (modeling browser toolbar / UI
  // controls that do not fire Tab WebContents focus notifications). The slow
  // reload completion must not steal focus back into Glic.
  ASSERT_NO_FATAL_FAILURE(prepare_guest_input_without_ime());
  ASSERT_OK(FocusGlic(instance));
  ExecuteJsTest();

  instance->host().Reload();
  ASSERT_TRUE(FocusExternalAndroidViewForTesting(tab_contents));
  EXPECT_FALSE(instance->HasFocus());
  ASSERT_OK(WaitUntilGuestIsShowing(instance));
  EXPECT_FALSE(instance->HasFocus());
}

INSTANTIATE_TEST_SUITE_P(All,
                         GlicAndroidReloadFocusBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "NativePlaceholder"
                                             : "WebUiLoader";
                         });

}  // namespace

}  // namespace glic
