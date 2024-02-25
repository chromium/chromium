// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_storage.h"

#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/nearby_sharing/certificates/common.h"
#include "components/cross_device/logging/logging.h"

std::optional<base::Time>
NearbyShareCertificateStorage::NextPrivateCertificateExpirationTime() {
  std::optional<std::vector<NearbySharePrivateCertificate>> certs =
      GetPrivateCertificates();
  if (!certs || certs->empty())
    return std::nullopt;

  base::Time min_time = base::Time::Max();
  for (const NearbySharePrivateCertificate& cert : *certs)
    min_time = std::min(min_time, cert.not_after());

  return min_time;
}

void NearbyShareCertificateStorage::UpdatePrivateCertificate(
    const NearbySharePrivateCertificate& private_certificate) {
  std::optional<std::vector<NearbySharePrivateCertificate>> certs =
      GetPrivateCertificates();
  if (!certs) {
    CD_LOG(WARNING, Feature::NS)
        << __func__ << ": No private certificates to update.";
    return;
  }

  auto it = base::ranges::find(*certs, private_certificate.id(),
                               &NearbySharePrivateCertificate::id);
  if (it == certs->end()) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": No private certificate with id="
        << base::HexEncode(private_certificate.id());
    return;
  }

  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Updating private certificate id="
      << base::HexEncode(private_certificate.id());
  *it = private_certificate;
  ReplacePrivateCertificates(*certs);
}

void NearbyShareCertificateStorage::RemoveExpiredPrivateCertificates(
    base::Time now) {
  std::optional<std::vector<NearbySharePrivateCertificate>> certs =
      GetPrivateCertificates();
  if (!certs)
    return;

  std::vector<NearbySharePrivateCertificate> unexpired_certs;
  for (const NearbySharePrivateCertificate& cert : *certs) {
    if (!IsNearbyShareCertificateExpired(
            now, cert.not_after(),
            /*use_public_certificate_tolerance=*/false)) {
      unexpired_certs.push_back(cert);
    }
  }

  size_t num_removed = certs->size() - unexpired_certs.size();
  if (num_removed == 0)
    return;

  CD_LOG(VERBOSE, Feature::NS) << __func__ << ": Removing " << num_removed
                               << " expired private certificates.";
  ReplacePrivateCertificates(unexpired_certs);
}

void NearbyShareCertificateStorage::ClearPrivateCertificates() {
  CD_LOG(VERBOSE, Feature::NS)
      << __func__ << ": Removing all private certificates.";
  ReplacePrivateCertificates(std::vector<NearbySharePrivateCertificate>());
}

void NearbyShareCertificateStorage::ClearPrivateCertificatesOfVisibility(
    nearby_share::mojom::Visibility visibility) {
  std::optional<std::vector<NearbySharePrivateCertificate>> certs =
      GetPrivateCertificates();
  if (!certs)
    return;

  bool were_certs_removed = false;
  std::vector<NearbySharePrivateCertificate> new_certs;
  for (const NearbySharePrivateCertificate& cert : *certs) {
    if (cert.visibility() == visibility) {
      were_certs_removed = true;
    } else {
      new_certs.push_back(cert);
    }
  }

  if (were_certs_removed) {
    CD_LOG(VERBOSE, Feature::NS)
        << __func__ << ": Removing all private certificates of visibility "
        << visibility;
    ReplacePrivateCertificates(new_certs);
  }
}
