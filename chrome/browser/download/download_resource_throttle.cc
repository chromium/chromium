// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_resource_throttle.h"

#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_stats.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/download/download_controller_base.h"
#include "content/public/browser/render_view_host.h"
#endif

using content::BrowserThread;

namespace {

void OnCanDownloadDecided(base::WeakPtr<DownloadResourceThrottle> throttle,
                          bool storage_permission_granted, bool allow) {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::Bind(&DownloadResourceThrottle::ContinueDownload, throttle,
                 storage_permission_granted, allow));
}

void CanDownload(
    std::unique_ptr<DownloadResourceThrottle::DownloadRequestInfo> info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  info->limiter->CanDownload(info->web_contents_getter, info->url,
                             info->request_method,
                             base::Bind(info->continue_callback, true));
}

#if defined(OS_ANDROID)
void OnThrottleAcquireFileAccessPermissionDone(
    std::unique_ptr<DownloadResourceThrottle::DownloadRequestInfo> info,
    bool granted) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (granted)
    CanDownload(std::move(info));
  else
    info->continue_callback.Run(false, false);
}
#endif

void CanDownloadOnUIThread(
    std::unique_ptr<DownloadResourceThrottle::DownloadRequestInfo> info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
#if defined(OS_ANDROID)
  const content::ResourceRequestInfo::WebContentsGetter& web_contents_getter =
      info->web_contents_getter;
  DownloadControllerBase::Get()->AcquireFileAccessPermission(
      web_contents_getter,
      base::Bind(&OnThrottleAcquireFileAccessPermissionDone,
                 base::Passed(std::move(info))));
#else
  CanDownload(std::move(info));
#endif
}

}  // namespace

DownloadResourceThrottle::DownloadRequestInfo::DownloadRequestInfo(
    scoped_refptr<DownloadRequestLimiter> limiter,
    const content::ResourceRequestInfo::WebContentsGetter& web_contents_getter,
    const GURL& url,
    const std::string& request_method,
    const DownloadRequestInfo::Callback& continue_callback)
    : limiter(limiter),
      web_contents_getter(web_contents_getter),
      url(url),
      request_method(request_method),
      continue_callback(continue_callback) {}

DownloadResourceThrottle::DownloadRequestInfo::~DownloadRequestInfo() {}

DownloadResourceThrottle::DownloadResourceThrottle(
    scoped_refptr<DownloadRequestLimiter> limiter,
    const content::ResourceRequestInfo::WebContentsGetter& web_contents_getter,
    const GURL& url,
    const std::string& request_method)
    : querying_limiter_(true),
      request_allowed_(false),
      request_deferred_(false) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &CanDownloadOnUIThread,
          std::unique_ptr<DownloadRequestInfo>(new DownloadRequestInfo(
              limiter, web_contents_getter, url, request_method,
              base::Bind(&OnCanDownloadDecided, AsWeakPtr())))));
}

DownloadResourceThrottle::~DownloadResourceThrottle() {
}

void DownloadResourceThrottle::WillStartRequest(bool* defer) {
  WillDownload(defer);
}

void DownloadResourceThrottle::WillRedirectRequest(
    const net::RedirectInfo& redirect_info,
    bool* defer) {
  WillDownload(defer);
}

void DownloadResourceThrottle::WillProcessResponse(bool* defer) {
  WillDownload(defer);
}

const char* DownloadResourceThrottle::GetNameForLogging() const {
  return "DownloadResourceThrottle";
}

void DownloadResourceThrottle::WillDownload(bool* defer) {
  DCHECK(!request_deferred_);

  // Defer the download until we have the DownloadRequestLimiter result.
  if (querying_limiter_) {
    request_deferred_ = true;
    *defer = true;
    return;
  }

  if (!request_allowed_) {
    RecordDownloadCount(CHROME_DOWNLOAD_COUNT_BLOCKED_BY_THROTTLING);
    Cancel();
  }
}

void DownloadResourceThrottle::ContinueDownload(
    bool storage_permission_granted, bool allow) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  querying_limiter_ = false;
  request_allowed_ = allow;

  if (!storage_permission_granted) {
    // UMA for this will be recorded in MobileDownload.StoragePermission.
  } else if (allow) {
    // Presumes all downloads initiated by navigation use this throttle and
    // nothing else does.
    RecordDownloadSource(DOWNLOAD_INITIATED_BY_NAVIGATION);
  } else {
    RecordDownloadCount(CHROME_DOWNLOAD_COUNT_BLOCKED_BY_THROTTLING);
  }

  if (request_deferred_) {
    request_deferred_ = false;
    if (allow) {
      Resume();
    } else {
      Cancel();
    }
  }
}
