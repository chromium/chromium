// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/tab_group_collection_data.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "components/tab_groups/tab_group_color.h"

namespace tabs {

TabGroupCollectionData::TabGroupCollectionData(
    tabs_pb::TabGroupCollectionState state) {
  title_ = base::UTF8ToUTF16(state.title());
  tab_group_id_ = base::Token(state.group_id_high(), state.group_id_low());
  is_collapsed_ = state.is_collapsed();
  color_ = static_cast<tab_groups::TabGroupColorId>(state.color());
}

TabGroupCollectionData::~TabGroupCollectionData() = default;

}  // namespace tabs
