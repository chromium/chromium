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
#include "content/public/browser/navigation_handle.h"
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
  return pdf_extension_test_util::EnsurePDFHasLoaded(
      embedder_host, /*wait_for_hit_test_data=*/true, /*pdf_element=*/"embed",
      /*allow_multiple_frames=*/true);
}

testing::AssertionResult
TestPdfViewerStreamManager::WaitUntilPdfLoadedInFirstChild() {
  content::RenderFrameHost* embedder_host =
      ChildFrameAt(web_contents()->GetPrimaryMainFrame(), 0);
  CHECK(embedder_host);
  return WaitUntilPdfLoaded(embedder_host);
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
