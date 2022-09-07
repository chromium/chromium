// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_download_manager_delegate.h"

#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/aw_contents_client_bridge.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace android_webview {

AwDownloadManagerDelegate::AwDownloadManagerDelegate() = default;
AwDownloadManagerDelegate::~AwDownloadManagerDelegate() = default;

bool AwDownloadManagerDelegate::InterceptDownloadIfApplicable(
    const GURL& url,
    const std::string& user_agent,
    const std::string& content_disposition,
    const std::string& mime_type,
    const std::string& request_origin,
    int64_t content_length,
    bool is_transient,
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!web_contents)
    return true;

  AwContentsClientBridge* client =
      AwContentsClientBridge::FromWebContents(web_contents);
  if (!client)
    return true;

  std::string aw_user_agent =
      web_contents->GetUserAgentOverride().ua_string_override;
  if (aw_user_agent.empty()) {
    // use default user agent if nothing is provided
    aw_user_agent = user_agent.empty() ? GetUserAgent() : user_agent;
  }

  client->NewDownload(url, aw_user_agent, content_disposition, mime_type,
                      content_length);
  return true;
}

}  // namespace android_webview
