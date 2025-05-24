// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_PROFILE_PREFS_REGISTRY_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_PROFILE_PREFS_REGISTRY_UTIL_H_

class PrefRegistrySimple;

namespace extensions {

// Registers the documentScan API preference with the |registry|.
void DocumentScanRegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_PROFILE_PREFS_REGISTRY_UTIL_H_
