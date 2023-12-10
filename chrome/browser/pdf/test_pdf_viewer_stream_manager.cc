// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/test_pdf_viewer_stream_manager.h"

#include <memory>

#include "base/run_loop.h"
#include "chrome/browser/pdf/pdf_viewer_stream_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

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

  // Check if the PDF has finished loading after the final PDF navigation. A
  // complete PDF navigation should have a claimed `StreamInfo`.
  auto* claimed_stream_info =
      GetClaimedStreamInfoFromPdfContentNavigation(navigation_handle);
  if (!claimed_stream_info || !claimed_stream_info->DidPdfContentNavigate()) {
    return;
  }

  std::move(on_pdf_loaded_).Run();
}

void TestPdfViewerStreamManager::WaitUntilPdfLoaded() {
  base::RunLoop run_loop;
  on_pdf_loaded_ = run_loop.QuitClosure();
  run_loop.Run();
}

}  // namespace pdf
