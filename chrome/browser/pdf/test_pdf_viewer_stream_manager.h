// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PDF_TEST_PDF_VIEWER_STREAM_MANAGER_H_
#define CHROME_BROWSER_PDF_TEST_PDF_VIEWER_STREAM_MANAGER_H_

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/pdf/pdf_viewer_stream_manager.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_user_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace pdf {

class TestPdfViewerStreamManager : public PdfViewerStreamManager {
 public:
  // Prefer using this over the constructor so that this instance is used for
  // PDF loads.
  static TestPdfViewerStreamManager* CreateForWebContents(
      content::WebContents* web_contents);

  explicit TestPdfViewerStreamManager(content::WebContents* contents);
  TestPdfViewerStreamManager(const TestPdfViewerStreamManager&) = delete;
  TestPdfViewerStreamManager& operator=(const TestPdfViewerStreamManager&) =
      delete;
  ~TestPdfViewerStreamManager() override;

  // WebContentsObserver overrides.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // PdfViewerStreamManager overrides.
  void NavigateToPdfExtensionUrl(
      content::FrameTreeNodeId extension_host_frame_tree_node_id,
      StreamInfo* stream_info,
      content::SiteInstance* site_instance,
      content::GlobalRenderFrameHostId global_id) override;

  // Delays the PDF extension from navigating to the PDF extension URL. There
  // are two navigations for the PDF extension frame: a navigation to
  // about:blank and a navigation to the PDF extension URL. This delays the
  // latter navigation.
  void DelayNextPdfExtensionNavigation();

  // Waits until the PDF extension frame finishes its first navigation to
  // about:blank, but not the navigation to the PDF extension URL. The test will
  // hang if `embedder_host` is not navigating to a PDF.
  void WaitUntilPdfExtensionNavigationStarted(
      content::RenderFrameHost* embedder_host);

  // Resumes the PDF extension frame navigation to the PDF extension URL. Must
  // be used in conjunction with `DelayNextPdfExtensionNavigation()` and
  // `WaitUntilPdfExtensionNavigationStarted()`.
  void ResumePdfExtensionNavigation(content::RenderFrameHost* embedder_host);

  // Waits until the PDF has finished loading. Returns true if the PDF loads
  // successfully, false otherwise. The test will hang if `embedder_host` is not
  // a PDF, or if the PDF frames never finish navigating.
  [[nodiscard]] testing::AssertionResult WaitUntilPdfLoaded(
      content::RenderFrameHost* embedder_host);

  // Same as `WaitUntilPdfLoaded()`, but allows additional subframes under the
  // PDF embedder host. There are some special cases where the PDF embedder may
  // have additional subframes. See crbug.com/40671023.
  [[nodiscard]] testing::AssertionResult WaitUntilPdfLoadedAllowMultipleFrames(
      content::RenderFrameHost* embedder_host);

  // Same as `WaitUntilPdfLoaded()`, but the first child of the primary main
  // frame should be the embedder. This is a common case where an HTML page only
  // embeds a single PDF.
  [[nodiscard]] testing::AssertionResult WaitUntilPdfLoadedInFirstChild();

 private:
  // Gathers all the necessary navigation params and navigates to the PDF
  // extension URL. `global_id` should be the ID for the intermediate
  // about:blank host.
  void GetParamsAndNavigateToPdfExtensionUrl(
      content::GlobalRenderFrameHostId global_id);

  // Waits for all PDF frames in a single PDF load to finish navigating.
  void WaitUntilPdfNavigationFinished(content::RenderFrameHost* embedder_host);

  // Indicates whether to delay the next PDF extension navigation to the PDF
  // extension URL.
  bool delay_next_pdf_extension_load_ = false;

  // Used only if `WaitUntilPdfExtensionNavigationStarted()` was used. Resumes
  // the PDF load.
  base::OnceClosure on_first_pdf_extension_navigation_finished_;

  // Used only if `ResumePdfExtensionNavigation()` was used. Resumes the PDF
  // extension load.
  base::OnceClosure on_resume_pdf_extension_navigation_;

  // Used once the PDF finished loading. Resumes the test.
  base::OnceClosure on_pdf_loaded_;

  // Needed to avoid use-after-free in callbacks.
  base::WeakPtrFactory<TestPdfViewerStreamManager> weak_factory_{this};
};

// While a `TestPdfViewerStreamManagerFactory` instance exists, it will
// automatically set itself as the global factory override. All PDF navigations
// will automatically use a `TestPdfViewerStreamManager` instance created from
// this factory.
class TestPdfViewerStreamManagerFactory
    : public PdfViewerStreamManager::Factory {
 public:
  TestPdfViewerStreamManagerFactory();

  TestPdfViewerStreamManagerFactory(const TestPdfViewerStreamManagerFactory&) =
      delete;
  TestPdfViewerStreamManagerFactory& operator=(
      const TestPdfViewerStreamManagerFactory&) = delete;

  ~TestPdfViewerStreamManagerFactory() override;

  // Return value is always non-nullptr. A `TestPdfViewerStreamManager` for
  // `contents` must have been created by `this`, or else a crash occurs.
  TestPdfViewerStreamManager* GetTestPdfViewerStreamManager(
      content::WebContents* contents);

  // PdfViewerStreamManager::Factory overrides.
  // Use `CreatePdfViewerStreamManager()` directly to create a test PDF stream
  // manager if the test does not block during navigation. If the test does
  // block during navigation, then the test PDF stream manager instance should
  // already be created automatically on navigation.
  void CreatePdfViewerStreamManager(content::WebContents* contents) override;

 private:
  // Tracks managers this factory has created. It's safe to track raw pointers,
  // since the pointers are only for comparison and aren't dereferenced.
  base::flat_set<raw_ptr<PdfViewerStreamManager, CtnExperimental>> managers_;
};

}  // namespace pdf

#endif  // CHROME_BROWSER_PDF_TEST_PDF_VIEWER_STREAM_MANAGER_H_
