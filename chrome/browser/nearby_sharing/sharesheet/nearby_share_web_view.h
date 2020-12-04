// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_SHARESHEET_NEARBY_SHARE_WEB_VIEW_H_
#define CHROME_BROWSER_NEARBY_SHARING_SHARESHEET_NEARBY_SHARE_WEB_VIEW_H_

#include "ui/views/controls/webview/webview.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

// NearbyShareWebView is used in place of the general views::WebView when
// creating the UI for the sharesheet action so that we can handle navigation
// to open a new tab when a link is clicked.
class NearbyShareWebView : public views::WebView {
 public:
  explicit NearbyShareWebView(content::BrowserContext* browser_context);
  ~NearbyShareWebView() override = default;

  // content::WebContentsDelegate:
  void WebContentsCreated(content::WebContents* source_contents,
                          int opener_render_process_id,
                          int opener_render_frame_id,
                          const std::string& frame_name,
                          const GURL& target_url,
                          content::WebContents* new_contents) override;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_SHARESHEET_NEARBY_SHARE_WEB_VIEW_H_
