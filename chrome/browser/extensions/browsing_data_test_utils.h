// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BROWSING_DATA_TEST_UTILS_H_
#define CHROME_BROWSER_EXTENSIONS_BROWSING_DATA_TEST_UTILS_H_

#include <vector>

#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"

namespace blink {
class StorageKey;
}

class Profile;

namespace extensions::browsing_data_test_utils {

// Initialize a minimal localStorage area for `key` in `profile`'s default
// storage partition.
void CreateLocalStorageForKey(Profile* profile, const blink::StorageKey& key);

// Returns all localStorage StorageUsageInfoPtrs from `profile`'s default
// storage partition.
std::vector<storage::mojom::StorageUsageInfoPtr> GetLocalStorageInfo(
    Profile* profile);

// Returns true if `key` appears in `usage_infos`.
bool UsageInfosHasStorageKey(
    const std::vector<storage::mojom::StorageUsageInfoPtr>& usage_infos,
    const blink::StorageKey& key);

}  // namespace extensions::browsing_data_test_utils

#endif  // CHROME_BROWSER_EXTENSIONS_BROWSING_DATA_TEST_UTILS_H_
