// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_WEB_FILE_HANDLERS_INTENT_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_WEB_FILE_HANDLERS_INTENT_UTIL_H_

#include <vector>

#include "components/services/app_service/public/cpp/intent.h"

namespace base {
class SafeBaseName;
}  // namespace base

namespace extensions {

class Extension;

// Get all names that were selected when the intent to open was initiated.
std::vector<base::SafeBaseName> GetBaseNamesForIntent(
    const apps::Intent& intent);

// Legacy versions of the QuickOffice extension are not compatible with web file
// handlers.
bool IsLegacyQuickOfficeExtension(const Extension& extension);

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_WEB_FILE_HANDLERS_INTENT_UTIL_H_
