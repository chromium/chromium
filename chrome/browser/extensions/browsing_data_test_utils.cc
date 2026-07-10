// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browsing_data_test_utils.h"

#include <algorithm>
#include <optional>

#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"

namespace extensions::browsing_data_test_utils {

void CreateLocalStorageForKey(Profile* profile, const blink::StorageKey& key) {
  auto* local_storage_control =
      profile->GetDefaultStoragePartition()->GetLocalStorageControl();
  mojo::Remote<blink::mojom::StorageArea> area;
  local_storage_control->BindStorageArea(key,
                                         area.BindNewPipeAndPassReceiver());
  {
    base::test::TestFuture<bool> put_future;
    area->Put({'k', 'e', 'y'}, {'v', 'a', 'l', 'u', 'e'},
              /*client_old_value=*/std::nullopt, /*source=*/nullptr,
              put_future.GetCallback());
    ASSERT_TRUE(put_future.Get());
  }
}

std::vector<storage::mojom::StorageUsageInfoPtr> GetLocalStorageInfo(
    Profile* profile) {
  auto* local_storage_control =
      profile->GetDefaultStoragePartition()->GetLocalStorageControl();
  base::test::TestFuture<std::vector<storage::mojom::StorageUsageInfoPtr>>
      get_usage_future;
  local_storage_control->GetUsage(get_usage_future.GetCallback());
  return get_usage_future.Take();
}

bool UsageInfosHasStorageKey(
    const std::vector<storage::mojom::StorageUsageInfoPtr>& usage_infos,
    const blink::StorageKey& key) {
  bool found = std::ranges::any_of(
      usage_infos, [&key](const storage::mojom::StorageUsageInfoPtr& info) {
        return info->storage_key == key;
      });
  return found;
}

}  // namespace extensions::browsing_data_test_utils
