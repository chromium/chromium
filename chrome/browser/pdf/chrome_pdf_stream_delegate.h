// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PDF_CHROME_PDF_STREAM_DELEGATE_H_
#define CHROME_BROWSER_PDF_CHROME_PDF_STREAM_DELEGATE_H_

#include "components/pdf/browser/pdf_stream_delegate.h"

class ChromePdfStreamDelegate : public pdf::PdfStreamDelegate {
 public:
  ChromePdfStreamDelegate();
  ChromePdfStreamDelegate(const ChromePdfStreamDelegate&) = delete;
  ChromePdfStreamDelegate operator=(const ChromePdfStreamDelegate&) = delete;
  ~ChromePdfStreamDelegate() override;

  // `pdf::PdfStreamDelegate`:
  absl::optional<GURL> MapToOriginalUrl(content::WebContents* contents,
                                        const GURL& stream_url) override;
  absl::optional<StreamInfo> GetStreamInfo(
      content::WebContents* contents) override;
};

#endif  // CHROME_BROWSER_PDF_CHROME_PDF_STREAM_DELEGATE_H_
