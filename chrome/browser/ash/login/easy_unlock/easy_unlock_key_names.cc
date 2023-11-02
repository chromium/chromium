// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/easy_unlock_key_names.h"

namespace ash {
namespace key_names {

const char kKeyBluetoothAddress[] = "bluetoothAddress";
const char kKeyBluetoothType[] = "bluetoothType";
const char kKeyPermitRecord[] = "permitRecord";
const char kKeyPermitId[] = "permitRecord.id";
const char kKeyPermitPermitId[] = "permitRecord.permitId";
const char kKeyPermitData[] = "permitRecord.data";
const char kKeyPermitType[] = "permitRecord.type";
const char kKeyPsk[] = "psk";
const char kKeySerializedBeaconSeeds[] = "serializedBeaconSeeds";
const char kKeyUnlockKey[] = "unlockKey";
const char kKeyLabelPrefix[] = "easy-unlock-";
const char kPermitPermitIdFormat[] = "permit://google.com/easyunlock/v1/%s";
const char kPermitTypeLicence[] = "licence";

}  // namespace key_names
}  // namespace ash
