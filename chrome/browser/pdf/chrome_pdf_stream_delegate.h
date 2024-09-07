// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PDF_CHROME_PDF_STREAM_DELEGATE_H_
#define CHROME_BROWSER_PDF_CHROME_PDF_STREAM_DELEGATE_H_

#include "components/pdf/browser/pdf_stream_delegate.h"
#include "content/public/browser/frame_tree_node_id.h"

class ChromePdfStreamDelegate : public pdf::PdfStreamDelegate {
 public:
  ChromePdfStreamDelegate();
  ChromePdfStreamDelegate(const ChromePdfStreamDelegate&) = delete;
  ChromePdfStreamDelegate operator=(const ChromePdfStreamDelegate&) = delete;
  ~ChromePdfStreamDelegate() override;

  // `pdf::PdfStreamDelegate`:
  std::optional<GURL> MapToOriginalUrl(
      content::NavigationHandle& navigation_handle) override;
  std::optional<StreamInfo> GetStreamInfo(
      content::RenderFrameHost* embedder_frame) override;
  void OnPdfEmbedderSandboxed(
      content::FrameTreeNodeId frame_tree_node_id) override;
  bool ShouldAllowPdfFrameNavigation(
      content::NavigationHandle* navigation_handle) override;
};

#endif  // CHROME_BROWSER_PDF_CHROME_PDF_STREAM_DELEGATE_H_
