// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_sync/background_sync_delegate_impl.h"

#include "chrome/browser/metrics/ukm_background_recorder_service.h"
#include "chrome/browser/profiles/profile.h"
#include "url/origin.h"

BackgroundSyncDelegateImpl::BackgroundSyncDelegateImpl(Profile* profile)
    : ukm_background_service_(
          ukm::UkmBackgroundRecorderFactory::GetForProfile(profile)) {
  DCHECK(ukm_background_service_);
}

BackgroundSyncDelegateImpl::~BackgroundSyncDelegateImpl() = default;

void BackgroundSyncDelegateImpl::GetUkmSourceId(
    const url::Origin& origin,
    base::OnceCallback<void(base::Optional<ukm::SourceId>)> callback) {
  ukm_background_service_->GetBackgroundSourceIdIfAllowed(origin,
                                                          std::move(callback));
}
