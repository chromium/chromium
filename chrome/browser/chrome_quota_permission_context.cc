// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_quota_permission_context.h"

#include <stdint.h>

#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/permissions/permission_request.h"
#include "chrome/browser/permissions/permission_request_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/android_theme_resources.h"
#else
#include "components/vector_icons/vector_icons.h"
#endif

namespace {

// On Android, if the site requested larger quota than this threshold, show a
// different message to the user.
const int64_t kRequestLargeQuotaThreshold = 5 * 1024 * 1024;

// QuotaPermissionRequest ---------------------------------------------

class QuotaPermissionRequest : public PermissionRequest {
 public:
  QuotaPermissionRequest(
      ChromeQuotaPermissionContext* context,
      const GURL& origin_url,
      bool is_large_quota_request_,
      const content::QuotaPermissionContext::PermissionCallback& callback);

  ~QuotaPermissionRequest() override;

 private:
  // PermissionRequest:
  IconId GetIconId() const override;
#if defined(OS_ANDROID)
  base::string16 GetTitleText() const override;
  base::string16 GetMessageText() const override;
#endif
  base::string16 GetMessageTextFragment() const override;
  GURL GetOrigin() const override;
  void PermissionGranted() override;
  void PermissionDenied() override;
  void Cancelled() override;
  void RequestFinished() override;
  PermissionRequestType GetPermissionRequestType() const override;

  const scoped_refptr<ChromeQuotaPermissionContext> context_;
  const GURL origin_url_;
  const bool is_large_quota_request_;
  content::QuotaPermissionContext::PermissionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(QuotaPermissionRequest);
};

QuotaPermissionRequest::QuotaPermissionRequest(
    ChromeQuotaPermissionContext* context,
    const GURL& origin_url,
    bool is_large_quota_request,
    const content::QuotaPermissionContext::PermissionCallback& callback)
    : context_(context),
      origin_url_(origin_url),
      is_large_quota_request_(is_large_quota_request),
      callback_(callback) {
  // Suppress unused private field warning on desktop
  (void)is_large_quota_request_;
}

QuotaPermissionRequest::~QuotaPermissionRequest() {}

PermissionRequest::IconId QuotaPermissionRequest::GetIconId() const {
#if defined(OS_ANDROID)
  return IDR_ANDROID_INFOBAR_FOLDER;
#else
  return vector_icons::kFolderIcon;
#endif
}

#if defined(OS_ANDROID)
base::string16 QuotaPermissionRequest::GetTitleText() const {
  return l10n_util::GetStringUTF16(IDS_REQUEST_QUOTA_PERMISSION_TITLE);
}

base::string16 QuotaPermissionRequest::GetMessageText() const {
  // If the site requested larger quota than this threshold, show a different
  // message to the user.
  return l10n_util::GetStringFUTF16(
      (is_large_quota_request_ ? IDS_REQUEST_LARGE_QUOTA_INFOBAR_TEXT
                               : IDS_REQUEST_QUOTA_INFOBAR_TEXT),
      url_formatter::FormatUrlForSecurityDisplay(origin_url_));
}
#endif

base::string16 QuotaPermissionRequest::GetMessageTextFragment() const {
  return l10n_util::GetStringUTF16(IDS_REQUEST_QUOTA_PERMISSION_FRAGMENT);
}

GURL QuotaPermissionRequest::GetOrigin() const {
  return origin_url_;
}

void QuotaPermissionRequest::PermissionGranted() {
  context_->DispatchCallbackOnIOThread(
      callback_,
      content::QuotaPermissionContext::QUOTA_PERMISSION_RESPONSE_ALLOW);
  callback_ = content::QuotaPermissionContext::PermissionCallback();
}

void QuotaPermissionRequest::PermissionDenied() {
  context_->DispatchCallbackOnIOThread(
      callback_,
      content::QuotaPermissionContext::QUOTA_PERMISSION_RESPONSE_DISALLOW);
  callback_ = content::QuotaPermissionContext::PermissionCallback();
}

void QuotaPermissionRequest::Cancelled() {
}

void QuotaPermissionRequest::RequestFinished() {
  if (!callback_.is_null()) {
    context_->DispatchCallbackOnIOThread(
        callback_,
        content::QuotaPermissionContext::QUOTA_PERMISSION_RESPONSE_CANCELLED);
  }

  delete this;
}

PermissionRequestType QuotaPermissionRequest::GetPermissionRequestType() const {
  return PermissionRequestType::QUOTA;
}

}  // namespace


// ChromeQuotaPermissionContext -----------------------------------------------

ChromeQuotaPermissionContext::ChromeQuotaPermissionContext() {
}

void ChromeQuotaPermissionContext::RequestQuotaPermission(
    const content::StorageQuotaParams& params,
    int render_process_id,
    const PermissionCallback& callback) {
  if (params.storage_type != blink::mojom::StorageType::kPersistent) {
    // For now we only support requesting quota with this interface
    // for Persistent storage type.
    callback.Run(QUOTA_PERMISSION_RESPONSE_DISALLOW);
    return;
  }

  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&ChromeQuotaPermissionContext::RequestQuotaPermission,
                       this, params, render_process_id, callback));
    return;
  }

  content::WebContents* web_contents = tab_util::GetWebContentsByFrameID(
      render_process_id, params.render_frame_id);
  if (!web_contents) {
    // The tab may have gone away or the request may not be from a tab.
    LOG(WARNING) << "Attempt to request quota tabless renderer: "
                 << render_process_id << "," << params.render_frame_id;
    DispatchCallbackOnIOThread(callback, QUOTA_PERMISSION_RESPONSE_CANCELLED);
    return;
  }

  PermissionRequestManager* permission_request_manager =
      PermissionRequestManager::FromWebContents(web_contents);
  if (permission_request_manager) {
    bool is_large_quota_request =
        params.requested_size > kRequestLargeQuotaThreshold;
    permission_request_manager->AddRequest(new QuotaPermissionRequest(
        this, params.origin_url, is_large_quota_request, callback));
    return;
  }

  // The tab has no UI service for presenting the permissions request.
  LOG(WARNING) << "Attempt to request quota from a background page: "
               << render_process_id << "," << params.render_frame_id;
  DispatchCallbackOnIOThread(callback, QUOTA_PERMISSION_RESPONSE_CANCELLED);
}

void ChromeQuotaPermissionContext::DispatchCallbackOnIOThread(
    const PermissionCallback& callback,
    QuotaPermissionResponse response) {
  DCHECK_EQ(false, callback.is_null());

  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::IO)) {
    base::PostTask(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(
            &ChromeQuotaPermissionContext::DispatchCallbackOnIOThread, this,
            callback, response));
    return;
  }

  callback.Run(response);
}

ChromeQuotaPermissionContext::~ChromeQuotaPermissionContext() {}
