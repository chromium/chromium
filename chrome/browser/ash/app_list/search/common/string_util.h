// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_COMMON_STRING_UTIL_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_COMMON_STRING_UTIL_H_

#include <optional>
#include <string>
#include <string_view>

#include "url/gurl.h"

namespace app_list {

// Normalizes training targets by removing any scheme prefix and trailing slash:
// "arc://[id]/" to "[id]". This is necessary because apps launched from
// different parts of the launcher have differently formatted IDs.
std::string NormalizeId(std::string_view id);

// Remove the Arc app shortcut label from an app ID, if it exists, so that
// "[app]/[label]" becomes "[app]".
std::string RemoveAppShortcutLabel(std::string_view id);

// Extracts the Drive ID from the given URL.
// Only works with "docs.google.com/.../d/[id]/" links.
std::optional<std::string> GetDriveId(const GURL& url);

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_COMMON_STRING_UTIL_H_
