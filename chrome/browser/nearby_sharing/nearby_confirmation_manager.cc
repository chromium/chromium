// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_confirmation_manager.h"

#include "base/functional/bind.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "content/public/browser/browser_thread.h"

namespace {

NearbySharingService::StatusCodesCallback ToStatusCodesCallback(
    base::OnceCallback<void(bool)> callback) {
  return base::BindOnce(
      [](base::OnceCallback<void(bool)> callback,
         NearbySharingService::StatusCodes status) {
        std::move(callback).Run(status ==
                                NearbySharingService::StatusCodes::kOk);
      },
      std::move(callback));
}

}  // namespace

NearbyConfirmationManager::NearbyConfirmationManager(
    NearbySharingService* nearby_service,
    ShareTarget share_target)
    : nearby_service_(nearby_service), share_target_(std::move(share_target)) {
  DCHECK(nearby_service_);
}

NearbyConfirmationManager::~NearbyConfirmationManager() = default;

void NearbyConfirmationManager::Accept(AcceptCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  nearby_service_->Accept(share_target_,
                          ToStatusCodesCallback(std::move(callback)));
}

void NearbyConfirmationManager::Reject(RejectCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  nearby_service_->Reject(share_target_,
                          ToStatusCodesCallback(std::move(callback)));
}

void NearbyConfirmationManager::Cancel(CancelCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  nearby_service_->Cancel(share_target_,
                          ToStatusCodesCallback(std::move(callback)));
}
