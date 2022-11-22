// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PERMISSION_REQUEST_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PERMISSION_REQUEST_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "components/permissions/permission_request.h"
#include "url/origin.h"

// A permission request that presents the user with a choice to allow or deny
// multiple downloads from the same site. This confirmation step protects
// against "carpet-bombing", where a malicious site forces multiple downloads on
// an unsuspecting user.
class DownloadPermissionRequest : public permissions::PermissionRequest {
 public:
  DownloadPermissionRequest(
      base::WeakPtr<DownloadRequestLimiter::TabDownloadState> host,
      const url::Origin& requesting_origin);

  DownloadPermissionRequest(const DownloadPermissionRequest&) = delete;
  DownloadPermissionRequest& operator=(const DownloadPermissionRequest&) =
      delete;

  ~DownloadPermissionRequest() override;

 private:
  void PermissionDecided(ContentSetting result,
                         bool is_one_time,
                         bool is_final_decision);
  void DeleteRequest();

  base::WeakPtr<DownloadRequestLimiter::TabDownloadState> host_;
  url::Origin requesting_origin_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PERMISSION_REQUEST_H_
