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

// Android WebView does not use Chromium downloads, so implement methods here to
// unconditionally cancel the download.
class AwDownloadManagerDelegate : public content::DownloadManagerDelegate,
                                  public base::SupportsUserData::Data {
 public:
  AwDownloadManagerDelegate();

  AwDownloadManagerDelegate(const AwDownloadManagerDelegate&) = delete;
  AwDownloadManagerDelegate& operator=(const AwDownloadManagerDelegate&) =
      delete;

  ~AwDownloadManagerDelegate() override;

  // content::DownloadManagerDelegate implementation.
  bool InterceptDownloadIfApplicable(
      const GURL& url,
      const std::string& user_agent,
      const std::string& content_disposition,
      const std::string& mime_type,
      const std::string& request_origin,
      int64_t content_length,
      bool is_transient,
      content::WebContents* web_contents) override;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_DOWNLOAD_MANAGER_DELEGATE_H_
