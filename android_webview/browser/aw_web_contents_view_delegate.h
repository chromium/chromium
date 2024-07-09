// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_WEB_CONTENTS_VIEW_DELEGATE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_WEB_CONTENTS_VIEW_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_view_delegate.h"

namespace content {
class WebContents;
}  // namespace content

namespace android_webview {

class AwWebContentsViewDelegate : public content::WebContentsViewDelegate {
 public:
  explicit AwWebContentsViewDelegate(content::WebContents* web_contents);

  AwWebContentsViewDelegate(const AwWebContentsViewDelegate&) = delete;
  AwWebContentsViewDelegate& operator=(const AwWebContentsViewDelegate&) =
      delete;

  ~AwWebContentsViewDelegate() override;

  void ShowContextMenu(content::RenderFrameHost& render_frame_host,
                       const content::ContextMenuParams& params) override;

  void DismissContextMenu() override;

  // content::WebContentsViewDelegate implementation.
  content::WebDragDestDelegate* GetDragDestDelegate() override;

 private:
  // The WebContents that owns the view and this delegate transitively.
  raw_ptr<content::WebContents> web_contents_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_WEB_CONTENTS_VIEW_DELEGATE_H_
