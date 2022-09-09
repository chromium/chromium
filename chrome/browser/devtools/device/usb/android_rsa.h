// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_USB_ANDROID_RSA_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_USB_ANDROID_RSA_H_

#include <memory>
#include <string>

namespace crypto {
class RSAPrivateKey;
}

class Profile;

std::unique_ptr<crypto::RSAPrivateKey> AndroidRSAPrivateKey(Profile* profile);

std::string AndroidRSAPublicKey(crypto::RSAPrivateKey* key);

std::string AndroidRSASign(crypto::RSAPrivateKey* key,
                           const std::string& body);

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_USB_ANDROID_RSA_H_
