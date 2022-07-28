// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_DEVICE_LOCAL_ACCOUNT_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_DEVICE_LOCAL_ACCOUNT_UTIL_H_

#include <string>

// This file contains utilities used for device local accounts (public sessions
// / kiosks). Eg. check whether an extension is allowlisted for use in public
// session.

namespace extensions {

bool IsAllowlistedForPublicSession(const std::string& extension_id);

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_DEVICE_LOCAL_ACCOUNT_UTIL_H_
