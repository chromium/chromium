// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_CONSTANTS_H_
#define CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_CONSTANTS_H_

#include "base/time/time.h"

// The number of days a certificate is valid.
extern const base::TimeDelta kNearbyShareCertificateValidityPeriod;

// The maximum offset for obfuscating a private certificate's not before/after
// timestamps when converting to a public certificate.
extern const base::TimeDelta
    kNearbyShareMaxPrivateCertificateValidityBoundOffset;

// To account for clock skew between the local device and remote devices, public
// certificates will be considered valid if the current time is within the
// bounds [not-before - tolerance, not-after + tolerance).
extern const base::TimeDelta
    kNearbySharePublicCertificateValidityBoundOffsetTolerance;

// The number of private certificates for a given visibility to be stored and
// rotated on the local device.
extern const size_t kNearbyShareNumPrivateCertificates;

// The number of bytes comprising the hash of the authentication token using the
// secret key.
extern const size_t kNearbyShareNumBytesAuthenticationTokenHash;

// Length of key in bytes required by AES-GCM encryption.
extern const size_t kNearbyShareNumBytesAesGcmKey;

// Length of salt in bytes required by AES-GCM encryption.
extern const size_t kNearbyShareNumBytesAesGcmIv;

// Length of salt in bytes required by AES-CTR encryption.
extern const size_t kNearbyShareNumBytesAesCtrIv;

// The number of bytes of the AES secret key used to encrypt/decrypt the
// metadata encryption key.
extern const size_t kNearbyShareNumBytesSecretKey;

// The number of the bytes of the AES key used to encryption personal info
// metadata, for example, name and picture data. These bytes are broadcast in an
// advertisement to other devices, thus the smaller byte size.
extern const size_t kNearbyShareNumBytesMetadataEncryptionKey;

// The number of bytes for the salt used for encryption of the metadata
// encryption key. These bytes are broadcast in the advertisement to other
// devices.
extern const size_t kNearbyShareNumBytesMetadataEncryptionKeySalt;

// The number of bytes used for the hash of the metadata encryption key.
extern const size_t kNearbyShareNumBytesMetadataEncryptionKeyTag;

// The number of bytes in a certificate's identifier.
extern const size_t kNearbyShareNumBytesCertificateId;

// Half of the possible 2-byte salt values.
//
// Note: Static identifiers can be tracked over time by setting up persistent
// scanners at known locations (eg. at different isles within a supermarket). As
// the scanners’ location is already known, anyone who walks past the scanner
// has their location recorded too. This can be used for heuristics (eg. number
// of customers in a store, customers who prefer product X also prefer product
// Y, dwell time), or can be attached to an identity (eg. rewards card when
// checking out at the cashier). By rotating our identifiers, we prevent
// inadvertently leaking location. However, even rotations can be tracked as we
// get closer to running out of salts. If tracked over a long enough time, the
// device that avoids salts that you’ve seen in the past is statistically likely
// to be the device you’re tracking. Therefore, we only use half of the
// available 2-byte salts.
extern const size_t kNearbyShareMaxNumMetadataEncryptionKeySalts;

// The max number of retries allowed to generate a salt. This is a sanity check
// that will never be hit.
extern const size_t
    kNearbyShareMaxNumMetadataEncryptionKeySaltGenerationRetries;

// The prefix prepended to the UKEY2 authentication token by the sender before
// signing.
extern const char kNearbyShareSenderVerificationPrefix;

// The prefix prepended to the UKEY2 authentication token by the receiver before
// signing.
extern const char kNearbyShareReceiverVerificationPrefix;

// The maximum number of attempts to initialize LevelDB in Certificate Storage.
extern const size_t kNearbyShareCertificateStorageMaxNumInitializeAttempts;

// The frequency with which to download public certificates.
extern const base::TimeDelta kNearbySharePublicCertificateDownloadPeriod;

#endif  // CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_CONSTANTS_H_
