// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_manager.h"

NearbyShareCertificateManager::NearbyShareCertificateManager() = default;

NearbyShareCertificateManager::~NearbyShareCertificateManager() = default;

void NearbyShareCertificateManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NearbyShareCertificateManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void NearbyShareCertificateManager::Start() {
  DCHECK(!is_running_);
  is_running_ = true;
  OnStart();
}

void NearbyShareCertificateManager::Stop() {
  DCHECK(is_running_);
  is_running_ = false;
  OnStop();
}

void NearbyShareCertificateManager::NotifyPublicCertificatesDownloaded() {
  for (auto& observer : observers_)
    observer.OnPublicCertificatesDownloaded();
}

void NearbyShareCertificateManager::NotifyPrivateCertificatesChanged() {
  for (auto& observer : observers_)
    observer.OnPrivateCertificatesChanged();
}
