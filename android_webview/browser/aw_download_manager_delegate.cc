// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_download_manager_delegate.h"

#include <memory>
#include <utility>

#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/aw_contents.h"
#include "android_webview/browser/aw_contents_client_bridge.h"
#include "android_webview/browser/aw_web_download.h"
#include "base/strings/utf_string_conversions.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_target_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/web_contents.h"
#include "net/base/filename_util.h"

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
    bool is_content_initiated,
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!web_contents)
    return true;

  // WebViews with a download handler let the Chromium download engine run, and
  // decide where to save each download in DetermineDownloadTarget().
  AwContents* aw_contents = AwContents::FromWebContents(web_contents);
  if (aw_contents && aw_contents->HasDownloadHandler()) {
    return false;
  }

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

bool AwDownloadManagerDelegate::DetermineDownloadTarget(
    download::DownloadItem* item,
    download::DownloadTargetCallback* callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // DownloadItemImpl asks again when it automatically resumes or restarts an
  // interrupted download. Keep the destination the embedder already chose
  // instead of handing the download to the embedder a second time.
  if (!item->GetTargetFilePath().empty()) {
    download::DownloadTargetInfo target_info;
    target_info.target_path = item->GetTargetFilePath();
    target_info.intermediate_path = target_info.target_path;
    std::move(*callback).Run(std::move(target_info));
    return true;
  }

  // Downloads that fail at the start skip InterceptDownloadIfApplicable(), so
  // the WebView may have no handler. Without one, the download is cancelled.
  auto download = std::make_unique<AwWebDownload>(item, std::move(*callback));
  content::WebContents* web_contents =
      content::DownloadItemUtils::GetWebContents(item);
  AwContents* aw_contents =
      web_contents ? AwContents::FromWebContents(web_contents) : nullptr;
  if (aw_contents) {
    // DownloadItem reports 0 both for an unknown size and for
    // "Content-Length: 0". Report -1 for both.
    int64_t total_bytes = item->GetTotalBytes();
    int64_t content_length = total_bytes > 0 ? total_bytes : -1;
    std::string suggested_filename =
        base::UTF16ToUTF8(net::GetSuggestedFilename(
            item->GetURL(), item->GetContentDisposition(),
            /*referrer_charset=*/std::string(), item->GetSuggestedFilename(),
            item->GetMimeType(), "download"));
    aw_contents->OnDownloadRequested(std::move(download), item->GetURL(),
                                     item->GetMimeType(), content_length,
                                     suggested_filename);
  }
  return true;
}

}  // namespace android_webview
