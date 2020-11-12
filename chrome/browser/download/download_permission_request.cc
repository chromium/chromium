// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_permission_request.h"

#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/android_theme_resources.h"
#include "components/url_formatter/elide_url.h"
#include "url/origin.h"
#else
#include "components/vector_icons/vector_icons.h"
#endif

DownloadPermissionRequest::DownloadPermissionRequest(
    base::WeakPtr<DownloadRequestLimiter::TabDownloadState> host,
    const url::Origin& request_origin)
    : host_(host), request_origin_(request_origin) {}

DownloadPermissionRequest::~DownloadPermissionRequest() {}

permissions::PermissionRequest::IconId DownloadPermissionRequest::GetIconId()
    const {
#if defined(OS_ANDROID)
  return IDR_ANDROID_INFOBAR_MULTIPLE_DOWNLOADS;
#else
  return vector_icons::kFileDownloadIcon;
#endif
}

#if defined(OS_ANDROID)
base::string16 DownloadPermissionRequest::GetMessageText() const {
  return l10n_util::GetStringFUTF16(
      IDS_MULTI_DOWNLOAD_WARNING, url_formatter::FormatOriginForSecurityDisplay(
                                      request_origin_,
                                      /*scheme_display = */ url_formatter::
                                          SchemeDisplay::OMIT_CRYPTOGRAPHIC));
}
#endif

base::string16 DownloadPermissionRequest::GetMessageTextFragment() const {
  return l10n_util::GetStringUTF16(IDS_MULTI_DOWNLOAD_PERMISSION_FRAGMENT);
}

GURL DownloadPermissionRequest::GetOrigin() const {
  return request_origin_.GetURL();
}

void DownloadPermissionRequest::PermissionGranted() {
  if (host_) {
    // This may invalidate |host_|.
    host_->Accept(request_origin_);
  }
}

void DownloadPermissionRequest::PermissionDenied() {
  if (host_) {
    // This may invalidate |host_|.
    host_->Cancel(request_origin_);
  }
}

void DownloadPermissionRequest::Cancelled() {
  if (host_) {
    // This may invalidate |host_|.
    host_->CancelOnce(request_origin_);
  }
}

void DownloadPermissionRequest::RequestFinished() {
  delete this;
}

permissions::PermissionRequestType
DownloadPermissionRequest::GetPermissionRequestType() const {
  return permissions::PermissionRequestType::DOWNLOAD;
}
