// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SERVICES_DEVICE_SYNC_PUBLIC_CPP_GCM_CONSTANTS_H_
#define ASH_SERVICES_DEVICE_SYNC_PUBLIC_CPP_GCM_CONSTANTS_H_

namespace chromeos {

namespace device_sync {

// ID constants used in GCM for CryptAuth-related calls.
extern const char kCryptAuthGcmAppId[];
extern const char kCryptAuthGcmSenderId[];
extern const char kCryptAuthV2EnrollmentAuthorizedEntity[];

}  // namespace device_sync

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the migration is finished.
namespace ash {
namespace device_sync {
using ::chromeos::device_sync::kCryptAuthGcmAppId;
using ::chromeos::device_sync::kCryptAuthV2EnrollmentAuthorizedEntity;
}  // namespace device_sync
}  // namespace ash

#endif  // ASH_SERVICES_DEVICE_SYNC_PUBLIC_CPP_GCM_CONSTANTS_H_
