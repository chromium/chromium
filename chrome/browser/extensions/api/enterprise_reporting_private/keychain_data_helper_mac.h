// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_KEYCHAIN_DATA_HELPER_MAC_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_KEYCHAIN_DATA_HELPER_MAC_H_

#include <Security/Security.h>

#include <string>

namespace extensions {

// Writes |password| into Keychain with |service_name| and |account_name|.
OSStatus WriteKeychainItem(const std::string& service_name,
                           const std::string& account_name,
                           const std::string& password);

// Verifies that the keychain for `item_ref` is unlocked. If all goes well, the
// value of `unlocked` will be set to the unlocked status of the keychain and
// noErr will be returned. If an error is encountered, its OSStatus will be
// returned and `unlocked` will remain untouched.
OSStatus VerifyKeychainForItemUnlocked(SecKeychainItemRef item_ref,
                                       bool* unlocked);

// Verifies that the default keychain is unlocked. If all goes well, the
// value of `unlocked` will be set to the unlocked status of the keychain and
// noErr will be returned. If an error is encountered, its OSStatus will be
// returned and `unlocked` will remain untouched.
OSStatus VerifyDefaultKeychainUnlocked(bool* unlocked);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_KEYCHAIN_DATA_HELPER_MAC_H_
