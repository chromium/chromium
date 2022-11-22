// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_permission_request.h"

#include "build/build_config.h"
#include "chrome/grit/generated_resources.h"
#include "components/permissions/request_type.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/android_theme_resources.h"
#include "components/url_formatter/elide_url.h"
#include "url/origin.h"
#else
#include "components/vector_icons/vector_icons.h"
#endif

DownloadPermissionRequest::DownloadPermissionRequest(
    base::WeakPtr<DownloadRequestLimiter::TabDownloadState> host,
    const url::Origin& requesting_origin)
    : PermissionRequest(
          requesting_origin.GetURL(),
          permissions::RequestType::kMultipleDownloads,
          /*has_gesture=*/false,
          base::BindRepeating(&DownloadPermissionRequest::PermissionDecided,
                              base::Unretained(this)),
          base::BindOnce(&DownloadPermissionRequest::DeleteRequest,
                         base::Unretained(this))),
      host_(host),
      requesting_origin_(requesting_origin) {}

DownloadPermissionRequest::~DownloadPermissionRequest() {}

void DownloadPermissionRequest::PermissionDecided(ContentSetting result,
                                                  bool is_one_time,
                                                  bool is_final_decision) {
  DCHECK(!is_one_time);
  DCHECK(is_final_decision);
  if (!host_)
    return;

  // This may invalidate |host_|.
  if (result == ContentSetting::CONTENT_SETTING_ALLOW) {
    host_->Accept(requesting_origin_);
  } else if (result == ContentSetting::CONTENT_SETTING_BLOCK) {
    host_->Cancel(requesting_origin_);
  } else {
    DCHECK_EQ(CONTENT_SETTING_DEFAULT, result);
    host_->CancelOnce(requesting_origin_);
  }
}

void DownloadPermissionRequest::DeleteRequest() {
  delete this;
}
