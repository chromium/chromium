// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/downloads/resource_throttle.h"

#include "base/logging.h"
#include "base/task/post_task.h"
#include "chrome/browser/offline_pages/offline_page_utils.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"

namespace {
void WillStartOfflineRequestOnUIThread(
    const GURL& url,
    const std::string& request_origin,
    const content::ResourceRequestInfo::WebContentsGetter& contents_getter) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::WebContents* web_contents = contents_getter.Run();
  if (!web_contents)
    return;
  offline_pages::OfflinePageUtils::ScheduleDownload(
      web_contents, offline_pages::kDownloadNamespace, url,
      offline_pages::OfflinePageUtils::DownloadUIActionFlags::ALL,
      request_origin);
}
}  // namespace

namespace offline_pages {
namespace downloads {

ResourceThrottle::ResourceThrottle(const net::URLRequest* request)
    : request_(request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

ResourceThrottle::~ResourceThrottle() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void ResourceThrottle::WillProcessResponse(bool* defer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::string mime_type;
  request_->GetMimeType(&mime_type);
  if (offline_pages::OfflinePageUtils::CanDownloadAsOfflinePage(request_->url(),
                                                                mime_type)) {
    const content::ResourceRequestInfo* info =
        content::ResourceRequestInfo::ForRequest(request_);
    if (!info)
      return;

    std::string request_origin =
        content::DownloadRequestUtils::GetRequestOriginFromRequest(request_);

    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::Bind(&WillStartOfflineRequestOnUIThread, request_->url(),
                   request_origin, info->GetWebContentsGetterForRequest()));
    Cancel();
  }
}

const char* ResourceThrottle::GetNameForLogging() const {
  return "offline_pages::downloads::ResourceThrottle";
}

}  // namespace downloads
}  // namespace offline_pages
