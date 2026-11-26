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
#include "base/files/file_path.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_source.h"
#include "components/download/public/common/download_target_info.h"
#include "components/safe_browsing/content/common/file_type_policies/file_type_policies.h"
#include "components/safe_browsing/content/common/file_type_policies/proto/download_file_types.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/filename_util.h"
#include "net/base/mime_util.h"
#include "net/http/http_content_disposition.h"
#include "url/gurl.h"

namespace android_webview {

namespace {

// The server asks for a download with Content-Disposition: attachment. The page
// asks with the download attribute, which makes Blink download the URL instead
// of navigating to it.
bool IsExplicitDownload(const download::DownloadItem& item) {
  return item.GetDownloadSource() != download::DownloadSource::NAVIGATION ||
         net::HttpContentDisposition(item.GetContentDisposition(),
                                     /*referrer_charset=*/std::string())
             .is_attachment();
}

bool IsForMainFrame(const download::DownloadItem& item) {
  // A download is attached to the frame that navigated to it, or to the frame
  // whose page asked for it, e.g. with the download attribute.
  content::RenderFrameHost* render_frame_host =
      content::DownloadItemUtils::GetRenderFrameHost(&item);
  return render_frame_host && render_frame_host->IsInPrimaryMainFrame();
}

bool IsDangerousFileName(const base::FilePath& file_name, const GURL& url) {
  return safe_browsing::FileTypePolicies::GetInstance()->GetFileDangerLevel(
             file_name, url, /*prefs=*/nullptr) !=
         safe_browsing::DownloadFileType::NOT_DANGEROUS;
}

}  // namespace

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
  if (!aw_contents) {
    return true;
  }

  base::FilePath file_name = net::GenerateFileName(
      item->GetURL(), item->GetContentDisposition(),
      /*referrer_charset=*/std::string(), item->GetSuggestedFilename(),
      item->GetMimeType(), "download");
  base::FilePath::StringType mime_extension;
  if (IsDangerousFileName(file_name, item->GetURL()) ||
      (net::GetPreferredExtensionForMimeType(item->GetMimeType(),
                                             &mime_extension) &&
       IsDangerousFileName(file_name.AddExtension(mime_extension),
                           item->GetURL()))) {
    // Dropping `download` cancels the item without notifying the embedder.
    return true;
  }

  // DownloadItem reports 0 both for an unknown size and for
  // "Content-Length: 0". Report -1 for both.
  int64_t total_bytes = item->GetTotalBytes();
  int64_t content_length = total_bytes > 0 ? total_bytes : -1;
  aw_contents->OnDownloadRequested(
      std::move(download), item->GetURL(), item->GetMimeType(), content_length,
      file_name.value(), IsExplicitDownload(*item), IsForMainFrame(*item),
      item->HasUserGesture());
  return true;
}

}  // namespace android_webview
