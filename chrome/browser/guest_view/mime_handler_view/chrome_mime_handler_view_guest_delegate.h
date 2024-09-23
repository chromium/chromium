// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_CHROME_MIME_HANDLER_VIEW_GUEST_DELEGATE_H_
#define CHROME_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_CHROME_MIME_HANDLER_VIEW_GUEST_DELEGATE_H_

#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest_delegate.h"

namespace extensions {

class ChromeMimeHandlerViewGuestDelegate : public MimeHandlerViewGuestDelegate {
 public:
  ChromeMimeHandlerViewGuestDelegate();

  ChromeMimeHandlerViewGuestDelegate(
      const ChromeMimeHandlerViewGuestDelegate&) = delete;
  ChromeMimeHandlerViewGuestDelegate& operator=(
      const ChromeMimeHandlerViewGuestDelegate&) = delete;

  ~ChromeMimeHandlerViewGuestDelegate() override;

  // MimeHandlerViewGuestDelegate.
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;
  void RecordLoadMetric(bool is_full_page,
                        const std::string& mime_type,
                        content::BrowserContext* browser_context) override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_CHROME_MIME_HANDLER_VIEW_GUEST_DELEGATE_H_
