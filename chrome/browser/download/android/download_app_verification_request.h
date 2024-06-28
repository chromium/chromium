// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_APP_VERIFICATION_REQUEST_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_APP_VERIFICATION_REQUEST_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/download/public/common/download_item.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_util.h"

// Encapsulates the process of checking whether app verification is
// enabled and possibly prompting the user to enable app verification.
class DownloadAppVerificationRequest : public download::DownloadItem::Observer {
 public:
  // On request completion, this class calls back with:
  // - Whether a prompt was shown to the user
  // - The associated `DownloadItem`
  using AppVerificationCallback =
      base::OnceCallback<void(bool, download::DownloadItem*)>;
  DownloadAppVerificationRequest(download::DownloadItem* item,
                                 AppVerificationCallback callback);
  ~DownloadAppVerificationRequest() override;

  void Start();

 private:
  // DownloadItem::Observer
  void OnDownloadDestroyed(download::DownloadItem* item) override;

  void IsVerifyAppsEnabled(safe_browsing::VerifyAppsEnabledResult result);

  void EnableVerifyAppsDone(safe_browsing::VerifyAppsEnabledResult result);

  raw_ptr<download::DownloadItem> item_;
  AppVerificationCallback callback_;
  base::WeakPtrFactory<DownloadAppVerificationRequest> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_DOWNLOAD_APP_VERIFICATION_REQUEST_H_
