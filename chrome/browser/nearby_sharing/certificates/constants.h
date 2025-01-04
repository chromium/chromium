// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_CONSTANTS_H_
#define CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_CONSTANTS_H_

#include "base/time/time.h"

// The number of days a certificate is valid.
inline constexpr base::TimeDelta kNearbyShareCertificateValidityPeriod =
    base::Days(3);

// The maximum offset for obfuscating a private certificate's not before/after
// timestamps when converting to a public certificate.
inline constexpr base::TimeDelta
    kNearbyShareMaxPrivateCertificateValidityBoundOffset = base::Hours(2);

// To account for clock skew between the local device and remote devices, public
// certificates will be considered valid if the current time is within the
// bounds [not-before - tolerance, not-after + tolerance).
inline constexpr base::TimeDelta
    kNearbySharePublicCertificateValidityBoundOffsetTolerance =
        base::Minutes(30);

// The number of private certificates for a given visibility to be stored and
// rotated on the local device.
inline constexpr size_t kNearbyShareNumPrivateCertificates = 3;

// The number of bytes comprising the hash of the authentication token using the
// secret key.
inline constexpr size_t kNearbyShareNumBytesAuthenticationTokenHash = 6;

// Length of key in bytes required by AES-GCM encryption.
inline constexpr size_t kNearbyShareNumBytesAesGcmKey = 32;

// Length of salt in bytes required by AES-GCM encryption.
inline constexpr size_t kNearbyShareNumBytesAesGcmIv = 12;

// Length of salt in bytes required by AES-CTR encryption.
inline constexpr size_t kNearbyShareNumBytesAesCtrIv = 16;

// The number of bytes of the AES secret key used to encrypt/decrypt the
// metadata encryption key.
inline constexpr size_t kNearbyShareNumBytesSecretKey = 32;

// The number of the bytes of the AES key used to encryption personal info
// metadata, for example, name and picture data. These bytes are broadcast in an
// advertisement to other devices, thus the smaller byte size.
inline constexpr size_t kNearbyShareNumBytesMetadataEncryptionKey = 14;

// The number of bytes for the salt used for encryption of the metadata
// encryption key. These bytes are broadcast in the advertisement to other
// devices.
inline constexpr size_t kNearbyShareNumBytesMetadataEncryptionKeySalt = 2;

// The number of bytes used for the hash of the metadata encryption key.
inline constexpr size_t kNearbyShareNumBytesMetadataEncryptionKeyTag = 32;

// The number of bytes in a certificate's identifier.
inline constexpr size_t kNearbyShareNumBytesCertificateId = 32;

// The size of the random byte array used for the encryption frame's signed data
// if a valid signature cannot be generated. This size is consistent with the
// GmsCore implementation.
inline constexpr size_t kNearbyShareNumBytesRandomSignature = 72;

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
inline constexpr size_t kNearbyShareMaxNumMetadataEncryptionKeySalts = 32768;

// The max number of retries allowed to generate a salt. This is a sanity check
// that will never be hit.
inline constexpr size_t
    kNearbyShareMaxNumMetadataEncryptionKeySaltGenerationRetries = 128;

// The prefix prepended to the UKEY2 authentication token by the sender before
// signing.
inline constexpr char kNearbyShareSenderVerificationPrefix = 0x01;

// The prefix prepended to the UKEY2 authentication token by the receiver before
// signing.
inline constexpr char kNearbyShareReceiverVerificationPrefix = 0x02;

// The maximum number of attempts to initialize LevelDB in Certificate Storage.
inline constexpr size_t kNearbyShareCertificateStorageMaxNumInitializeAttempts =
    3;

// The frequency with which to download public certificates.
inline constexpr base::TimeDelta kNearbySharePublicCertificateDownloadPeriod =
    base::Hours(12);

#endif  // CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_CONSTANTS_H_
