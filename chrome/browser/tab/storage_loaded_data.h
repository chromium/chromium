// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_STORAGE_LOADED_DATA_H_
#define CHROME_BROWSER_TAB_STORAGE_LOADED_DATA_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "chrome/browser/tab/payload.h"
#include "chrome/browser/tab/protocol/tab_state.pb.h"
#include "chrome/browser/tab/tab_group_collection_data.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs {

using OnTabInterfaceCreation = base::OnceCallback<void(const TabInterface*)>;
using LoadedTabState = std::pair<tabs_pb::TabState, OnTabInterfaceCreation>;

// Represents data loaded from the database.
struct StorageLoadedData {
  StorageLoadedData();
  ~StorageLoadedData();

  StorageLoadedData(const StorageLoadedData&) = delete;
  StorageLoadedData& operator=(const StorageLoadedData&) = delete;

  StorageLoadedData(StorageLoadedData&&);
  StorageLoadedData& operator=(StorageLoadedData&&);

  std::vector<LoadedTabState> loaded_tabs;
  std::vector<std::unique_ptr<TabGroupCollectionData>> loaded_groups;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_STORAGE_LOADED_DATA_H_
