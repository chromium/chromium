// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/content/web_contents_factory_data_deleter.h"

#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"

void WebContentsFactoryDataDeleter::WebContentsDestroyed() {
  // Clear all data for the storage partition.
  web_contents()
      ->GetBrowserContext()
      ->GetStoragePartition(storage_partition_config_, false)
      ->ClearData(content::StoragePartition::REMOVE_DATA_MASK_ALL,
                  content::StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
                  blink::StorageKey(), base::Time(), base::Time::Max(),
                  base::DoNothing());

  delete this;
}
