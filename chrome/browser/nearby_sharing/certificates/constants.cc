// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/certificates/constants.h"

const base::TimeDelta kNearbyShareCertificateValidityPeriod = base::Days(3);
const base::TimeDelta kNearbyShareMaxPrivateCertificateValidityBoundOffset =
    base::Hours(2);
const base::TimeDelta
    kNearbySharePublicCertificateValidityBoundOffsetTolerance =
        base::Minutes(30);
const size_t kNearbyShareNumPrivateCertificates = 3;
const size_t kNearbyShareNumBytesAuthenticationTokenHash = 6;
const size_t kNearbyShareNumBytesAesGcmKey = 32;
const size_t kNearbyShareNumBytesAesGcmIv = 12;
const size_t kNearbyShareNumBytesAesCtrIv = 16;
const size_t kNearbyShareNumBytesSecretKey = 32;
const size_t kNearbyShareNumBytesMetadataEncryptionKey = 14;
const size_t kNearbyShareNumBytesMetadataEncryptionKeySalt = 2;
const size_t kNearbyShareNumBytesMetadataEncryptionKeyTag = 32;
const size_t kNearbyShareNumBytesCertificateId = 32;
const size_t kNearbyShareMaxNumMetadataEncryptionKeySalts = 32768;
const size_t kNearbyShareMaxNumMetadataEncryptionKeySaltGenerationRetries = 128;
const char kNearbyShareSenderVerificationPrefix = 0x01;
const char kNearbyShareReceiverVerificationPrefix = 0x02;
const size_t kNearbyShareCertificateStorageMaxNumInitializeAttempts = 3;
const base::TimeDelta kNearbySharePublicCertificateDownloadPeriod =
    base::Hours(12);
