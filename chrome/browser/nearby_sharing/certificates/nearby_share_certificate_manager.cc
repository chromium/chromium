// Copyright 2020 The Chromium Authors
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
  if (is_running_)
    return;

  is_running_ = true;
  OnStart();
}

void NearbyShareCertificateManager::Stop() {
  if (!is_running_)
    return;

  is_running_ = false;
  OnStop();
}

std::optional<NearbyShareEncryptedMetadataKey>
NearbyShareCertificateManager::EncryptPrivateCertificateMetadataKey(
    nearby_share::mojom::Visibility visibility) {
  std::optional<NearbySharePrivateCertificate> cert =
      GetValidPrivateCertificate(visibility);
  if (!cert)
    return std::nullopt;

  std::optional<NearbyShareEncryptedMetadataKey> encrypted_key =
      cert->EncryptMetadataKey();

  // Every salt consumed to encrypt the metadata encryption key is tracked by
  // the NearbySharePrivateCertificate. Update the private certificate in
  // storage to reflect the new list of consumed salts.
  UpdatePrivateCertificateInStorage(*cert);

  return encrypted_key;
}

std::optional<std::vector<uint8_t>>
NearbyShareCertificateManager::SignWithPrivateCertificate(
    nearby_share::mojom::Visibility visibility,
    base::span<const uint8_t> payload) const {
  std::optional<NearbySharePrivateCertificate> cert =
      GetValidPrivateCertificate(visibility);
  if (!cert)
    return std::nullopt;

  return cert->Sign(payload);
}

std::optional<std::vector<uint8_t>>
NearbyShareCertificateManager::HashAuthenticationTokenWithPrivateCertificate(
    nearby_share::mojom::Visibility visibility,
    base::span<const uint8_t> authentication_token) const {
  std::optional<NearbySharePrivateCertificate> cert =
      GetValidPrivateCertificate(visibility);
  if (!cert)
    return std::nullopt;

  return cert->HashAuthenticationToken(authentication_token);
}

void NearbyShareCertificateManager::NotifyPublicCertificatesDownloaded() {
  for (auto& observer : observers_)
    observer.OnPublicCertificatesDownloaded();
}

void NearbyShareCertificateManager::NotifyPrivateCertificatesChanged() {
  for (auto& observer : observers_)
    observer.OnPrivateCertificatesChanged();
}
