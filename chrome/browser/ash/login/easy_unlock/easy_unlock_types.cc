// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/easy_unlock/easy_unlock_types.h"

namespace ash {

const char kEasyUnlockKeyMetaNameBluetoothAddress[] = "eu.btaddr";
const char kEasyUnlockKeyMetaNamePsk[] = "eu.psk";
const char kEasyUnlockKeyMetaNamePubKey[] = "eu.pubkey";
const char kEasyUnlockKeyMetaNameChallenge[] = "eu.C";
const char kEasyUnlockKeyMetaNameWrappedSecret[] = "eu.WUK";
const char kEasyUnlockKeyMetaNameSerializedBeaconSeeds[] = "eu.BS";
const char kEasyUnlockKeyMetaNameUnlockKey[] = "eu.unlock_key";

EasyUnlockDeviceKeyData::EasyUnlockDeviceKeyData() = default;

EasyUnlockDeviceKeyData::EasyUnlockDeviceKeyData(
    const EasyUnlockDeviceKeyData&) = default;

EasyUnlockDeviceKeyData::~EasyUnlockDeviceKeyData() = default;

}  // namespace ash
