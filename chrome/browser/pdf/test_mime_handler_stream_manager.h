// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PDF_TEST_MIME_HANDLER_STREAM_MANAGER_H_
#define CHROME_BROWSER_PDF_TEST_MIME_HANDLER_STREAM_MANAGER_H_

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/pdf/mime_handler_stream_manager.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents_user_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
class NavigationHandle;
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace pdf {

class TestMimeHandlerStreamManager : public MimeHandlerStreamManager {
 public:
  // Prefer using this over the constructor so that this instance is used for
  // MIME handler loads.
  static TestMimeHandlerStreamManager* CreateForWebContents(
      content::WebContents* web_contents);

  explicit TestMimeHandlerStreamManager(content::WebContents* contents);
  TestMimeHandlerStreamManager(const TestMimeHandlerStreamManager&) = delete;
  TestMimeHandlerStreamManager& operator=(const TestMimeHandlerStreamManager&) =
      delete;
  ~TestMimeHandlerStreamManager() override;

  // WebContentsObserver overrides.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // MimeHandlerStreamManager overrides.
  void NavigateToExtensionUrl(
      content::FrameTreeNodeId extension_host_frame_tree_node_id,
      extensions::StreamInfo* stream_info,
      content::SiteInstance* site_instance,
      content::GlobalRenderFrameHostId global_id) override;

  // Delays the extension from navigating to the extension URL. There are two
  // navigations for the extension frame: a navigation to about:blank and a
  // navigation to the extension URL. This delays the latter navigation.
  void DelayNextExtensionNavigation();

  // Waits until the extension frame finishes its first navigation to
  // about:blank, but not the navigation to the extension URL. The test will
  // hang if `embedder_host` is not navigating to a handled MIME type.
  void WaitUntilExtensionNavigationStarted(
      content::RenderFrameHost* embedder_host);

  // Resumes the extension frame navigation to the extension URL. Must be used
  // in conjunction with `DelayNextExtensionNavigation()` and
  // `WaitUntilExtensionNavigationStarted()`.
  void ResumeExtensionNavigation(content::RenderFrameHost* embedder_host);

  // Waits until the handler has finished loading. Returns true if it loads
  // successfully, false otherwise. The test will hang if `embedder_host` is not
  // a handled MIME type, or if the frames never finish navigating.
  [[nodiscard]] testing::AssertionResult WaitUntilPdfLoaded(
      content::RenderFrameHost* embedder_host);

  // Same as `WaitUntilPdfLoaded()`, but allows additional subframes under the
  // embedder host. There are some special cases where the embedder may have
  // additional subframes. See crbug.com/40671023.
  [[nodiscard]] testing::AssertionResult WaitUntilPdfLoadedAllowMultipleFrames(
      content::RenderFrameHost* embedder_host);

  // Same as `WaitUntilPdfLoaded()`, but the first child of the primary main
  // frame should be the embedder. This is a common case where an HTML page
  // embeds a single handled MIME type.
  [[nodiscard]] testing::AssertionResult WaitUntilPdfLoadedInFirstChild();

 private:
  // Gathers all the necessary navigation params and navigates to the extension
  // URL. `global_id` should be the ID for the intermediate about:blank host.
  void GetParamsAndNavigateToExtensionUrl(
      content::GlobalRenderFrameHostId global_id);

  // Waits for all MIME handler frames in a single load to finish navigating.
  void WaitUntilNavigationFinished(content::RenderFrameHost* embedder_host);

  // Indicates whether to delay the next extension navigation to the extension
  // URL.
  bool delay_next_extension_load_ = false;

  // Used only if `WaitUntilExtensionNavigationStarted()` was used. Resumes
  // the load.
  base::OnceClosure on_first_extension_navigation_finished_;

  // Used only if `ResumeExtensionNavigation()` was used. Resumes the
  // extension load.
  base::OnceClosure on_resume_extension_navigation_;

  // Used once the handler finished loading. Resumes the test.
  base::OnceClosure on_navigation_finished_;

  // Needed to avoid use-after-free in callbacks.
  base::WeakPtrFactory<TestMimeHandlerStreamManager> weak_factory_{this};
};

// While a `TestMimeHandlerStreamManagerFactory` instance exists, it will
// automatically set itself as the global factory override. All MIME handler
// navigations will automatically use a `TestMimeHandlerStreamManager` instance
// created from this factory.
class TestMimeHandlerStreamManagerFactory
    : public MimeHandlerStreamManager::Factory {
 public:
  TestMimeHandlerStreamManagerFactory();

  TestMimeHandlerStreamManagerFactory(
      const TestMimeHandlerStreamManagerFactory&) = delete;
  TestMimeHandlerStreamManagerFactory& operator=(
      const TestMimeHandlerStreamManagerFactory&) = delete;

  ~TestMimeHandlerStreamManagerFactory() override;

  // Return value is always non-nullptr. A `TestMimeHandlerStreamManager` for
  // `contents` must have been created by `this`, or else a crash occurs.
  TestMimeHandlerStreamManager* GetTestMimeHandlerStreamManager(
      content::WebContents* contents);

  // MimeHandlerStreamManager::Factory overrides.
  // Use `CreateMimeHandlerStreamManager()` directly to create a test stream
  // manager if the test does not block during navigation. If the test does
  // block during navigation, then the test stream manager instance should
  // already be created automatically on navigation.
  void CreateMimeHandlerStreamManager(content::WebContents* contents) override;

 private:
  // Tracks managers this factory has created. It's safe to track raw pointers,
  // since the pointers are only for comparison and aren't dereferenced.
  base::flat_set<raw_ptr<MimeHandlerStreamManager, CtnExperimental>> managers_;
};

}  // namespace pdf

#endif  // CHROME_BROWSER_PDF_TEST_MIME_HANDLER_STREAM_MANAGER_H_
