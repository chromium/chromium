// Copyright 2020 The Chromium Authors. All rights reserved.
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

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_KEYCHAIN_DATA_HELPER_MAC_H_
