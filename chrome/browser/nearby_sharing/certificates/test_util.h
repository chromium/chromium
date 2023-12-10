// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_TEST_UTIL_H_
#define CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_TEST_UTIL_H_

#include <memory>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_encrypted_metadata_key.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_private_certificate.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"
#include "crypto/ec_private_key.h"
#include "crypto/symmetric_key.h"
#include "third_party/nearby/sharing/proto/encrypted_metadata.pb.h"
#include "third_party/nearby/sharing/proto/rpc_resources.pb.h"

extern const char kTestMetadataFullName[];
extern const char kTestMetadataIconUrl[];

// Test Bluetooth MAC address in the format "XX:XX:XX:XX:XX:XX".
extern const char kTestUnparsedBluetoothMacAddress[];

std::unique_ptr<crypto::ECPrivateKey> GetNearbyShareTestP256KeyPair();
const std::vector<uint8_t>& GetNearbyShareTestP256PublicKey();

std::unique_ptr<crypto::SymmetricKey> GetNearbyShareTestSecretKey();
const std::vector<uint8_t>& GetNearbyShareTestCertificateId();

const std::vector<uint8_t>& GetNearbyShareTestMetadataEncryptionKey();
const std::vector<uint8_t>& GetNearbyShareTestMetadataEncryptionKeyTag();
const std::vector<uint8_t>& GetNearbyShareTestSalt();
const NearbyShareEncryptedMetadataKey& GetNearbyShareTestEncryptedMetadataKey();

base::Time GetNearbyShareTestNotBefore();
base::TimeDelta GetNearbyShareTestValidityOffset();

const nearby::sharing::proto::EncryptedMetadata& GetNearbyShareTestMetadata();
const std::vector<uint8_t>& GetNearbyShareTestEncryptedMetadata();

const std::vector<uint8_t>& GetNearbyShareTestPayloadToSign();
const std::vector<uint8_t>& GetNearbyShareTestSampleSignature();
const std::vector<uint8_t>& GetNearbyShareTestPayloadHashUsingSecretKey();

NearbySharePrivateCertificate GetNearbyShareTestPrivateCertificate(
    nearby_share::mojom::Visibility visibility,
    base::Time not_before = GetNearbyShareTestNotBefore());
nearby::sharing::proto::PublicCertificate GetNearbyShareTestPublicCertificate(
    nearby_share::mojom::Visibility visibility,
    base::Time not_before = GetNearbyShareTestNotBefore());

// Returns a list of |kNearbyShareNumPrivateCertificates| private/public
// certificates, spanning contiguous validity periods.
std::vector<NearbySharePrivateCertificate>
GetNearbyShareTestPrivateCertificateList(
    nearby_share::mojom::Visibility visibility);
std::vector<nearby::sharing::proto::PublicCertificate>
GetNearbyShareTestPublicCertificateList(
    nearby_share::mojom::Visibility visibility);

const NearbyShareDecryptedPublicCertificate&
GetNearbyShareTestDecryptedPublicCertificate();

#endif  // CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_TEST_UTIL_H_
