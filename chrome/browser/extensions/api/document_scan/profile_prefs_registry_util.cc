// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/document_scan/profile_prefs_registry_util.h"

#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace extensions {

void DocumentScanRegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kDocumentScanAPITrustedExtensions);
}

}  // namespace extensions
