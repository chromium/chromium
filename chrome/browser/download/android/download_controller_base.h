// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_CONTROLLER_BASE_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_CONTROLLER_BASE_H_

#include <string>

#include "base/functional/callback.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_start_observer.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_content_disposition.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

extern const char kOMADrmMessageMimeType[];
extern const char kOMADrmContentMimeType[];
extern const char kOMADrmRightsMimeType1[];
extern const char kOMADrmRightsMimeType2[];

content::WebContents* GetWebContents(int render_process_id, int render_view_id);

// Used to store all the information about an Android download.
struct DownloadInfo {
  DownloadInfo(const DownloadInfo& other);
  DownloadInfo(const GURL& url,
               const GURL& original_url,
               const std::string& content_disposition,
               const std::string& original_mime_type,
               const std::string& user_agent,
               const std::string& cookie,
               const GURL& referer);
  ~DownloadInfo();

  // The URL from which we are downloading. This is the final URL after any
  // redirection by the server for |original_url_|.
  GURL url;
  // The original URL before any redirection by the server for this URL.
  GURL original_url;
  std::string content_disposition;
  std::string original_mime_type;
  std::string user_agent;
  std::string cookie;
  GURL referer;
};

// Interface to request GET downloads and send notifications for POST
// downloads.
class DownloadControllerBase : public download::DownloadItem::Observer,
                               public download::DownloadStartObserver {
 public:
  // Returns the singleton instance of the DownloadControllerBase.
  static DownloadControllerBase* Get();

  // Called to set the DownloadControllerBase instance.
  static void SetDownloadControllerBase(
      DownloadControllerBase* download_controller);

  // Called when a download is initiated by context menu.
  virtual void StartContextMenuDownload(
      const content::ContextMenuParams& params,
      content::WebContents* web_contents,
      bool is_link) = 0;

  // Callback when user permission prompt finishes. Args: whether file access
  // permission is acquired.
  using AcquireFileAccessPermissionCallback = base::OnceCallback<void(bool)>;

  // Called to prompt the user for file access permission. When finished,
  // |callback| will be executed.
  virtual void AcquireFileAccessPermission(
      const content::WebContents::Getter& wc_getter,
      AcquireFileAccessPermissionCallback callback) = 0;

  // Called by unit test to approve or disapprove file access request.
  virtual void SetApproveFileAccessRequestForTesting(bool approve) {}

  // Starts a new download request with Android DownloadManager. Can be called
  // on any thread.
  virtual void CreateAndroidDownload(
      const content::WebContents::Getter& wc_getter,
      const DownloadInfo& info) = 0;

 protected:
  ~DownloadControllerBase() override {}
  static DownloadControllerBase* download_controller_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_CONTROLLER_BASE_H_
