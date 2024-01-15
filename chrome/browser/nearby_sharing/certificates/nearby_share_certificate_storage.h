// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_STORAGE_H_
#define CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_STORAGE_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_private_certificate.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"
#include "third_party/nearby/sharing/proto/rpc_resources.pb.h"

// Stores local-device private certificates and remote-device public
// certificates. Provides methods to help manage certificate expiration. Due to
// the potentially large number of public certificates, some methods are
// asynchronous.
class NearbyShareCertificateStorage {
 public:
  using ResultCallback = base::OnceCallback<void(bool)>;
  using PublicCertificateCallback = base::OnceCallback<void(
      bool,
      std::unique_ptr<std::vector<nearby::sharing::proto::PublicCertificate>>)>;

  NearbyShareCertificateStorage() = default;
  virtual ~NearbyShareCertificateStorage() = default;

  // Returns all public certificates currently in storage. No RPC call is made.
  virtual void GetPublicCertificates(PublicCertificateCallback callback) = 0;

  // Returns all private certificates currently in storage. Will return
  // std::nullopt if deserialization from prefs fails -- not expected to happen
  // under normal circumstances.
  virtual std::optional<std::vector<NearbySharePrivateCertificate>>
  GetPrivateCertificates() const = 0;

  // Returns the next time a certificate expires or std::nullopt if no
  // certificates are present.
  std::optional<base::Time> NextPrivateCertificateExpirationTime();
  virtual std::optional<base::Time> NextPublicCertificateExpirationTime()
      const = 0;

  // Deletes existing private certificates and replaces them with
  // |private_certificates|.
  virtual void ReplacePrivateCertificates(
      const std::vector<NearbySharePrivateCertificate>&
          private_certificates) = 0;

  // Overwrites an existing record with |private_certificate| if that records
  // has the same ID . If no such record exists in storage, no action is taken.
  // This method is necessary for updating the private certificate's list of
  // consumed salts.
  void UpdatePrivateCertificate(
      const NearbySharePrivateCertificate& private_certificate);

  // Adds public certificates, or replaces existing certificates
  // by secret_id
  virtual void AddPublicCertificates(
      const std::vector<nearby::sharing::proto::PublicCertificate>&
          public_certificates,
      ResultCallback callback) = 0;

  // Removes all private certificates from storage with expiration date after
  // |now|.
  void RemoveExpiredPrivateCertificates(base::Time now);

  // Removes all public certificates from storage with expiration date after
  // |now|.
  virtual void RemoveExpiredPublicCertificates(base::Time now,
                                               ResultCallback callback) = 0;

  // Delete all private certificates from memory and persistent storage.
  void ClearPrivateCertificates();

  // Delete private certificates with |visibility| from memory and persistent
  // storage.
  void ClearPrivateCertificatesOfVisibility(
      nearby_share::mojom::Visibility visibility);
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_CERTIFICATE_STORAGE_H_
