// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_USB_ANDROID_RSA_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_USB_ANDROID_RSA_H_

#include <memory>
#include <optional>
#include <string>

#include "crypto/keypair.h"

class Profile;

// Retrieves the stored ADB private key from the given profile, creating a new
// one if the key isn't stored or the stored key can't be decoded.
crypto::keypair::PrivateKey AndroidRSAPrivateKey(Profile* profile);

// Encodes `key`'s public key according to the custom format expected by ADB.
// The format can only encode 2048-bit RSA keys. If `key` cannot be encoded, it
// returns `std::nullopt`.
std::optional<std::string> AndroidRSAPublicKey(crypto::keypair::PrivateKey key);

std::string AndroidRSASign(crypto::keypair::PrivateKey key,
                           const std::string& body);

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_USB_ANDROID_RSA_H_
