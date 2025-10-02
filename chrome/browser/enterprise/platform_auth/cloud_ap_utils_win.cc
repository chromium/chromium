// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/cloud_ap_utils_win.h"

#include <winerror.h>

#include <string>
#include <string_view>
#include <utility>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/win/registry.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace enterprise_auth {

void AppendRegistryOrigins(HKEY root,
                           const wchar_t* key_name,
                           const wchar_t* value_name,
                           std::vector<url::Origin>& origins) {
  base::win::RegKey key;
  auto result = key.Open(root, key_name, KEY_WOW64_64KEY | KEY_QUERY_VALUE);
  if (result != ERROR_SUCCESS) {
    if (result != ERROR_FILE_NOT_FOUND && DLOG_IS_ON(ERROR)) {
      DPLOG(ERROR) << __func__ << " Failed to open " << key_name;
    }
    return;
  }

  std::vector<std::wstring> multi_string_values;
  result = key.ReadValues(value_name, &multi_string_values);
  if (result == ERROR_CANTREAD) {
    multi_string_values.resize(1);
    result = key.ReadValue(value_name, &multi_string_values[0]);
  }
  if (result != ERROR_SUCCESS) {
    if (result != ERROR_FILE_NOT_FOUND && DLOG_IS_ON(ERROR)) {
      DPLOG(ERROR) << __func__ << " Failed to read value " << value_name;
    }
    return;
  }

  for (const auto& value : multi_string_values) {
    GURL url(base::AsStringPiece16(value));
    if (url.is_valid() && url.SchemeIs(url::kHttpsScheme) && url.has_host() &&
        url.EffectiveIntPort() ==
            url::DefaultPortForScheme(url.scheme_piece())) {
      DVLOG(1) << __func__ << " Discovered MS Auth LoginUrl: \"" << url << "\"";
      origins.push_back(url::Origin::Create(url));
    } else {
      DLOG(ERROR) << __func__ << " Ignoring invalid LoginUrl value: \"" << value
                  << "\"";
    }
  }
}

}  // namespace enterprise_auth
