// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/test_pdf_viewer_stream_manager.h"

#include <memory>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/run_loop.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/pdf/pdf_viewer_stream_manager.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace pdf {

TestPdfViewerStreamManager::TestPdfViewerStreamManager(
    content::WebContents* contents)
    : PdfViewerStreamManager(contents) {}

TestPdfViewerStreamManager::~TestPdfViewerStreamManager() = default;

// static
TestPdfViewerStreamManager* TestPdfViewerStreamManager::CreateForWebContents(
    content::WebContents* web_contents) {
  auto manager = std::make_unique<TestPdfViewerStreamManager>(web_contents);
  auto* manager_ptr = manager.get();
  web_contents->SetUserData(PdfViewerStreamManager::UserDataKey(),
                            std::move(manager));
  return manager_ptr;
}

void TestPdfViewerStreamManager::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  PdfViewerStreamManager::DidFinishNavigation(navigation_handle);

  if (!on_pdf_loaded_) {
    return;
  }

  // Check if the PDF has finished loading after the final PDF navigation. A
  // complete PDF navigation should have a claimed `StreamInfo`.
  auto* claimed_stream_info =
      GetClaimedStreamInfoFromPdfContentNavigation(navigation_handle);
  if (!claimed_stream_info || !claimed_stream_info->DidPdfContentNavigate()) {
    return;
  }

  std::move(on_pdf_loaded_).Run();
}

void TestPdfViewerStreamManager::NavigateToPdfExtensionUrl(
    content::FrameTreeNodeId extension_host_frame_tree_node_id,
    StreamInfo* stream_info,
    content::SiteInstance* site_instance,
    content::GlobalRenderFrameHostId global_id) {
  if (on_first_pdf_extension_navigation_finished_) {
    // Stop blocking and immediately continue with the PDF extension navigation.
    std::move(on_first_pdf_extension_navigation_finished_).Run();
  }

  if (delay_next_pdf_extension_load_) {
    // Delay the PDF extension load until `ResumePdfExtensionNavigation()` is
    // called.
    delay_next_pdf_extension_load_ = false;
    on_resume_pdf_extension_navigation_ = base::BindOnce(
        &TestPdfViewerStreamManager::GetParamsAndNavigateToPdfExtensionUrl,
        weak_factory_.GetWeakPtr(), global_id);
    return;
  }

  PdfViewerStreamManager::NavigateToPdfExtensionUrl(
      extension_host_frame_tree_node_id, stream_info, site_instance, global_id);
}

void TestPdfViewerStreamManager::DelayNextPdfExtensionNavigation() {
  delay_next_pdf_extension_load_ = true;
}

void TestPdfViewerStreamManager::WaitUntilPdfExtensionNavigationStarted(
    content::RenderFrameHost* embedder_host) {
  // If `StreamInfo::extension_host_frame_tree_node_id()` has been set, then
  // the navigation to about:blank has already committed.
  auto* claimed_stream_info = GetClaimedStreamInfo(embedder_host);
  if (!claimed_stream_info ||
      !claimed_stream_info->extension_host_frame_tree_node_id()) {
    base::RunLoop run_loop;
    on_first_pdf_extension_navigation_finished_ = run_loop.QuitClosure();
    run_loop.Run();
  }
}

void TestPdfViewerStreamManager::ResumePdfExtensionNavigation(
    content::RenderFrameHost* embedder_host) {
  CHECK(on_resume_pdf_extension_navigation_);
  std::move(on_resume_pdf_extension_navigation_).Run();
}

testing::AssertionResult TestPdfViewerStreamManager::WaitUntilPdfLoaded(
    content::RenderFrameHost* embedder_host) {
  WaitUntilPdfNavigationFinished(embedder_host);

  // Wait until the PDF extension and content are loaded.
  return pdf_extension_test_util::EnsurePDFHasLoaded(embedder_host);
}

testing::AssertionResult
TestPdfViewerStreamManager::WaitUntilPdfLoadedAllowMultipleFrames(
    content::RenderFrameHost* embedder_host) {
  WaitUntilPdfNavigationFinished(embedder_host);

  // Wait until the PDF extension and content are loaded.
  pdf_extension_test_util::EnsurePDFHasLoadedOptions options{
      .allow_multiple_frames = true};
  return pdf_extension_test_util::EnsurePDFHasLoadedWithOptions(embedder_host,
                                                                options);
}

testing::AssertionResult
TestPdfViewerStreamManager::WaitUntilPdfLoadedInFirstChild() {
  content::RenderFrameHost* embedder_host =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  CHECK(embedder_host);
  return WaitUntilPdfLoaded(embedder_host);
}

void TestPdfViewerStreamManager::GetParamsAndNavigateToPdfExtensionUrl(
    content::GlobalRenderFrameHostId global_id) {
  auto* about_blank_host = content::RenderFrameHost::FromID(global_id);
  CHECK(about_blank_host);
  content::RenderFrameHost* embedder_host = about_blank_host->GetParent();
  CHECK(embedder_host);
  auto* stream_info = GetClaimedStreamInfo(embedder_host);
  CHECK(stream_info);

  PdfViewerStreamManager::NavigateToPdfExtensionUrl(
      about_blank_host->GetFrameTreeNodeId(), stream_info,
      embedder_host->GetSiteInstance(), global_id);
}

void TestPdfViewerStreamManager::WaitUntilPdfNavigationFinished(
    content::RenderFrameHost* embedder_host) {
  // If all of the PDF frames haven't navigated, wait.
  auto* claimed_stream_info = GetClaimedStreamInfo(embedder_host);
  if (!claimed_stream_info || !claimed_stream_info->DidPdfContentNavigate()) {
    base::RunLoop run_loop;
    on_pdf_loaded_ = run_loop.QuitClosure();
    run_loop.Run();
  }
}

TestPdfViewerStreamManagerFactory::TestPdfViewerStreamManagerFactory() {
  PdfViewerStreamManager::SetFactoryForTesting(this);
}

TestPdfViewerStreamManagerFactory::~TestPdfViewerStreamManagerFactory() {
  PdfViewerStreamManager::SetFactoryForTesting(nullptr);
}

TestPdfViewerStreamManager*
TestPdfViewerStreamManagerFactory::GetTestPdfViewerStreamManager(
    content::WebContents* contents) {
  PdfViewerStreamManager* manager =
      PdfViewerStreamManager::FromWebContents(contents);
  CHECK(manager);

  // Check if `manager` was created by `this`. If so, the `manager` is safe to
  // downcast into a `TestPdfViewerStreamManager`.
  CHECK(managers_.contains(manager));

  return static_cast<TestPdfViewerStreamManager*>(manager);
}

void TestPdfViewerStreamManagerFactory::CreatePdfViewerStreamManager(
    content::WebContents* contents) {
  PdfViewerStreamManager* manager =
      TestPdfViewerStreamManager::CreateForWebContents(contents);
  CHECK(managers_.insert(manager).second);
}

}  // namespace pdf
