// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_TAB_GROUP_COLLECTION_DATA_H_
#define CHROME_BROWSER_TAB_TAB_GROUP_COLLECTION_DATA_H_

#include <string>

#include "base/token.h"
#include "chrome/browser/tab/protocol/tab_group_collection_state.pb.h"
#include "components/tab_groups/tab_group_color.h"

namespace tabs {

// Holds the deserialized data for a TabGroupTabCollection.
struct TabGroupCollectionData {
 public:
  explicit TabGroupCollectionData(tabs_pb::TabGroupCollectionState state);
  ~TabGroupCollectionData();

  base::Token tab_group_id_;
  bool is_collapsed_;
  tab_groups::TabGroupColorId color_;
  std::u16string title_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_TAB_GROUP_COLLECTION_DATA_H_
