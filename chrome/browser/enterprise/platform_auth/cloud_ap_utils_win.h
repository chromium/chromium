// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_CLOUD_AP_UTILS_WIN_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_CLOUD_AP_UTILS_WIN_H_

#include <vector>

#include "base/win/windows_types.h"

namespace url {
class Origin;
}

namespace enterprise_auth {

// Reads URLs from the value `value_name` in the key `key_name` in the Windows
// registry and appends their origins to `origins`. The named value must be of
// type either REG_SZ or REG_MULTI_SZ. Only https URLs with a host part and no
// or the default port are retained.
void AppendRegistryOrigins(HKEY root,
                           const wchar_t* key_name,
                           const wchar_t* value_name,
                           std::vector<url::Origin>& origins);

}  // namespace enterprise_auth

#endif  // CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_CLOUD_AP_UTILS_WIN_H_
