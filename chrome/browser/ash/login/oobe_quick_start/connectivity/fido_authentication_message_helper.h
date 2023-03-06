// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_FIDO_AUTHENTICATION_MESSAGE_HELPER_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_FIDO_AUTHENTICATION_MESSAGE_HELPER_H_

#include <vector>
namespace ash::quick_start::message_helper {

static constexpr char kCredentialIdKey[] = "id";
static constexpr char kEntitiyIdMapKey[] = "id";

// CBOR Values from https://www.rfc-editor.org/rfc/rfc7049
static constexpr int kCborTypeInt = 1;
static constexpr int kCborTypeByteString = 2;
static constexpr int kCborTypeString = 3;
static constexpr int kCborTypeArray = 4;

std::vector<uint8_t> BuildEncodedResponseData(
    std::vector<uint8_t> credential_id,
    std::vector<uint8_t> auth_data,
    std::vector<uint8_t> signature,
    std::vector<uint8_t> user_id,
    uint8_t status);

}  // namespace ash::quick_start::message_helper

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_FIDO_AUTHENTICATION_MESSAGE_HELPER_H_
