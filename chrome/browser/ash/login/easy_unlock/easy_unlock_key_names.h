// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_KEY_NAMES_H_
#define CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_KEY_NAMES_H_

namespace ash {
namespace key_names {

// These are the names of the fields which populate the keys which are persisted
// in the TPM.

extern const char kKeyBluetoothAddress[];
extern const char kKeyBluetoothType[];
extern const char kKeyPermitRecord[];
extern const char kKeyPermitId[];
extern const char kKeyPermitPermitId[];
extern const char kKeyPermitData[];
extern const char kKeyPermitType[];
extern const char kKeyPsk[];
extern const char kKeySerializedBeaconSeeds[];
extern const char kKeyUnlockKey[];
extern const char kKeyLabelPrefix[];
extern const char kPermitPermitIdFormat[];
extern const char kPermitTypeLicence[];

}  // namespace key_names
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_EASY_UNLOCK_EASY_UNLOCK_KEY_NAMES_H_
