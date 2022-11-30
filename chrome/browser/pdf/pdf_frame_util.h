// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PDF_PDF_FRAME_UTIL_H_
#define CHROME_BROWSER_PDF_PDF_FRAME_UTIL_H_

namespace content {
class RenderFrameHost;
}  // namespace content

namespace pdf_frame_util {

// Searches the children of the given `rfh` to find a `RenderFrameHost` that
// hosts PDF content.
content::RenderFrameHost* FindPdfChildFrame(content::RenderFrameHost* rfh);

}  // namespace pdf_frame_util

#endif  // CHROME_BROWSER_PDF_PDF_FRAME_UTIL_H_
