// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/download_app_verification_request.h"

#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "components/download/public/common/download_item.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_bridge.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_util.h"

DownloadAppVerificationRequest::DownloadAppVerificationRequest(
    download::DownloadItem* item,
    AppVerificationCallback callback)
    : item_(item), callback_(std::move(callback)) {
  item_->AddObserver(this);
}

DownloadAppVerificationRequest::~DownloadAppVerificationRequest() {
  item_->RemoveObserver(this);
}

void DownloadAppVerificationRequest::Start() {
  safe_browsing::SafeBrowsingApiHandlerBridge::GetInstance()
      .StartIsVerifyAppsEnabled(
          base::BindOnce(&DownloadAppVerificationRequest::IsVerifyAppsEnabled,
                         weak_factory_.GetWeakPtr()));
}
// DownloadItem::Observer
void DownloadAppVerificationRequest::OnDownloadDestroyed(
    download::DownloadItem* item) {
  // It's safe to pass `item` in the callback because the callback
  // immediately deletes `this`.
  std::move(callback_).Run(false, item);
  // Do not add code after this. Callback may delete `this`.
}

void DownloadAppVerificationRequest::IsVerifyAppsEnabled(
    safe_browsing::VerifyAppsEnabledResult result) {
  base::UmaHistogramEnumeration("SBClientDownload.AndroidAppVerificationResult",
                                result);

  if (result != safe_browsing::VerifyAppsEnabledResult::SUCCESS_NOT_ENABLED) {
    std::move(callback_).Run(false, item_);
    // Do not add code after this. Callback may delete `this`.
    return;
  }

  safe_browsing::SafeBrowsingApiHandlerBridge::GetInstance()
      .StartEnableVerifyApps(
          base::BindOnce(&DownloadAppVerificationRequest::EnableVerifyAppsDone,
                         weak_factory_.GetWeakPtr()));
}

void DownloadAppVerificationRequest::EnableVerifyAppsDone(
    safe_browsing::VerifyAppsEnabledResult result) {
  base::UmaHistogramEnumeration(
      "SBClientDownload.AndroidAppVerificationPromptResult", result);

  std::move(callback_).Run(true, item_);
  // Do not add code after this. Callback may delete `this`.
}
