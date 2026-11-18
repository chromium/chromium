// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_DOWNLOAD_MANAGER_DELEGATE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_DOWNLOAD_MANAGER_DELEGATE_H_

#include <string>

#include "base/supports_user_data.h"
#include "content/public/browser/download_manager_delegate.h"

namespace content {

class WebContents;

}  // namespace content

namespace android_webview {

// Decides how downloads started in a WebView are handled. By default, WebView
// does not run Chromium downloads: InterceptDownloadIfApplicable() cancels the
// request and forwards it to the embedder's legacy DownloadListener. WebViews
// with a download handler (see AwContents#setDownloadHandler) instead let the
// Chromium download engine run, and DetermineDownloadTarget() hands each
// download to the handler, which chooses where to save it.
class AwDownloadManagerDelegate : public content::DownloadManagerDelegate,
                                  public base::SupportsUserData::Data {
 public:
  AwDownloadManagerDelegate();

  AwDownloadManagerDelegate(const AwDownloadManagerDelegate&) = delete;
  AwDownloadManagerDelegate& operator=(const AwDownloadManagerDelegate&) =
      delete;

  ~AwDownloadManagerDelegate() override;

  // content::DownloadManagerDelegate implementation.
  bool DetermineDownloadTarget(
      download::DownloadItem* item,
      download::DownloadTargetCallback* callback) override;
  bool InterceptDownloadIfApplicable(
      const GURL& url,
      const std::string& user_agent,
      const std::string& content_disposition,
      const std::string& mime_type,
      const std::string& request_origin,
      int64_t content_length,
      bool is_transient,
      bool is_content_initiated,
      content::WebContents* web_contents) override;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_DOWNLOAD_MANAGER_DELEGATE_H_
