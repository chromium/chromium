// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PDF_PDF_HANDLER_STREAM_DELEGATE_H_
#define CHROME_BROWSER_PDF_PDF_HANDLER_STREAM_DELEGATE_H_

#include "extensions/browser/mime_handler/mime_handler_stream_delegate.h"

namespace pdf {

class PdfHandlerStreamDelegate : public extensions::MimeHandlerStreamDelegate {
 public:
  PdfHandlerStreamDelegate();
  PdfHandlerStreamDelegate(const PdfHandlerStreamDelegate&) = delete;
  PdfHandlerStreamDelegate& operator=(const PdfHandlerStreamDelegate&) = delete;
  ~PdfHandlerStreamDelegate() override;

  // extensions::MimeHandlerStreamDelegate:
  void OnExtensionFrameFinished(content::NavigationHandle* navigation_handle,
                                extensions::StreamInfo* stream_info) override;
  void OnStreamClaimed(content::RenderFrameHost* embedder_host,
                       extensions::StreamInfo* stream_info) override;
  bool PluginCanSave() const override;
  void SetPluginCanSave(bool plugin_can_save) override;

 private:
  bool plugin_can_save_ = false;
};

}  // namespace pdf

#endif  // CHROME_BROWSER_PDF_PDF_HANDLER_STREAM_DELEGATE_H_
