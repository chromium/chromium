// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/test_mime_handler_stream_manager.h"

#include <memory>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/run_loop.h"
#include "chrome/browser/pdf/mime_handler_stream_manager.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace pdf {

TestMimeHandlerStreamManager::TestMimeHandlerStreamManager(
    content::WebContents* contents)
    : MimeHandlerStreamManager(contents) {}

TestMimeHandlerStreamManager::~TestMimeHandlerStreamManager() = default;

// static
TestMimeHandlerStreamManager*
TestMimeHandlerStreamManager::CreateForWebContents(
    content::WebContents* web_contents) {
  auto manager = std::make_unique<TestMimeHandlerStreamManager>(web_contents);
  auto* manager_ptr = manager.get();
  web_contents->SetUserData(MimeHandlerStreamManager::UserDataKey(),
                            std::move(manager));
  return manager_ptr;
}

void TestMimeHandlerStreamManager::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  MimeHandlerStreamManager::DidFinishNavigation(navigation_handle);

  if (!on_navigation_finished_) {
    return;
  }

  // Check if the handler has finished loading after the final content
  // navigation. A complete load should have a claimed `StreamInfo`.
  auto* claimed_stream_info =
      GetClaimedStreamInfoFromContentNavigation(navigation_handle);
  if (!claimed_stream_info ||
      !claimed_stream_info->DidContentFrameFinishNavigation()) {
    return;
  }

  std::move(on_navigation_finished_).Run();
}

void TestMimeHandlerStreamManager::NavigateToExtensionUrl(
    content::FrameTreeNodeId extension_host_frame_tree_node_id,
    extensions::StreamInfo* stream_info,
    content::SiteInstance* site_instance,
    content::GlobalRenderFrameHostId global_id) {
  if (on_first_extension_navigation_finished_) {
    // Stop blocking and immediately continue with the extension navigation.
    std::move(on_first_extension_navigation_finished_).Run();
  }

  if (delay_next_extension_load_) {
    // Delay the extension load until `ResumeExtensionNavigation()` is
    // called.
    delay_next_extension_load_ = false;
    on_resume_extension_navigation_ = base::BindOnce(
        &TestMimeHandlerStreamManager::GetParamsAndNavigateToExtensionUrl,
        weak_factory_.GetWeakPtr(), global_id);
    return;
  }

  MimeHandlerStreamManager::NavigateToExtensionUrl(
      extension_host_frame_tree_node_id, stream_info, site_instance, global_id);
}

void TestMimeHandlerStreamManager::DelayNextExtensionNavigation() {
  delay_next_extension_load_ = true;
}

void TestMimeHandlerStreamManager::WaitUntilExtensionNavigationStarted(
    content::RenderFrameHost* embedder_host) {
  // If `StreamInfo::extension_host_frame_tree_node_id()` has been set, then
  // the navigation to about:blank has already committed.
  auto* claimed_stream_info = GetClaimedStreamInfo(embedder_host);
  if (!claimed_stream_info ||
      !claimed_stream_info->extension_host_frame_tree_node_id()) {
    base::RunLoop run_loop;
    on_first_extension_navigation_finished_ = run_loop.QuitClosure();
    run_loop.Run();
  }
}

void TestMimeHandlerStreamManager::ResumeExtensionNavigation(
    content::RenderFrameHost* embedder_host) {
  CHECK(on_resume_extension_navigation_);
  std::move(on_resume_extension_navigation_).Run();
}

testing::AssertionResult TestMimeHandlerStreamManager::WaitUntilPdfLoaded(
    content::RenderFrameHost* embedder_host) {
  WaitUntilNavigationFinished(embedder_host);

  // Wait until the PDF extension and content are loaded.
  return pdf_extension_test_util::EnsurePDFHasLoaded(embedder_host);
}

testing::AssertionResult
TestMimeHandlerStreamManager::WaitUntilPdfLoadedAllowMultipleFrames(
    content::RenderFrameHost* embedder_host) {
  WaitUntilNavigationFinished(embedder_host);

  // Wait until the PDF extension and content are loaded.
  pdf_extension_test_util::EnsurePDFHasLoadedOptions options{
      .allow_multiple_frames = true};
  return pdf_extension_test_util::EnsurePDFHasLoadedWithOptions(embedder_host,
                                                                options);
}

testing::AssertionResult
TestMimeHandlerStreamManager::WaitUntilPdfLoadedInFirstChild() {
  content::RenderFrameHost* embedder_host =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  CHECK(embedder_host);
  return WaitUntilPdfLoaded(embedder_host);
}

void TestMimeHandlerStreamManager::GetParamsAndNavigateToExtensionUrl(
    content::GlobalRenderFrameHostId global_id) {
  auto* about_blank_host = content::RenderFrameHost::FromID(global_id);
  CHECK(about_blank_host);
  content::RenderFrameHost* embedder_host = about_blank_host->GetParent();
  CHECK(embedder_host);
  auto* stream_info = GetClaimedStreamInfo(embedder_host);
  CHECK(stream_info);

  MimeHandlerStreamManager::NavigateToExtensionUrl(
      about_blank_host->GetFrameTreeNodeId(), stream_info,
      embedder_host->GetSiteInstance(), global_id);
}

void TestMimeHandlerStreamManager::WaitUntilNavigationFinished(
    content::RenderFrameHost* embedder_host) {
  // If all of the MIME handler frames haven't navigated, wait.
  auto* claimed_stream_info = GetClaimedStreamInfo(embedder_host);
  if (!claimed_stream_info ||
      !claimed_stream_info->DidContentFrameFinishNavigation()) {
    base::RunLoop run_loop;
    on_navigation_finished_ = run_loop.QuitClosure();
    run_loop.Run();
  }
}

TestMimeHandlerStreamManagerFactory::TestMimeHandlerStreamManagerFactory() {
  MimeHandlerStreamManager::SetFactoryForTesting(this);
}

TestMimeHandlerStreamManagerFactory::~TestMimeHandlerStreamManagerFactory() {
  MimeHandlerStreamManager::SetFactoryForTesting(nullptr);
}

TestMimeHandlerStreamManager*
TestMimeHandlerStreamManagerFactory::GetTestMimeHandlerStreamManager(
    content::WebContents* contents) {
  MimeHandlerStreamManager* manager =
      MimeHandlerStreamManager::FromWebContents(contents);
  CHECK(manager);

  // Check if `manager` was created by `this`. If so, the `manager` is safe to
  // downcast into a `TestMimeHandlerStreamManager`.
  CHECK(managers_.contains(manager));

  return static_cast<TestMimeHandlerStreamManager*>(manager);
}

void TestMimeHandlerStreamManagerFactory::CreateMimeHandlerStreamManager(
    content::WebContents* contents) {
  MimeHandlerStreamManager* manager =
      TestMimeHandlerStreamManager::CreateForWebContents(contents);
  CHECK(managers_.insert(manager).second);
}

}  // namespace pdf
