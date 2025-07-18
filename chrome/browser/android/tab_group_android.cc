// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_group_android.h"

#include <memory>
#include <optional>

#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group_tab_collection.h"
#include "components/tabs/public/tab_interface.h"

TabGroupAndroid::TabGroupAndroid(
    Profile* profile,
    tabs::TabGroupTabCollection* collection,
    const tab_groups::TabGroupId& id,
    const tab_groups::TabGroupVisualData& visual_data)
    : TabGroup(collection, id, visual_data) {
  tab_group_features_ = TabGroupFeatures::CreateTabGroupFeatures();
  tab_group_features_->Init(*this, profile);
}

TabGroupAndroid::~TabGroupAndroid() = default;

void TabGroupAndroid::set_last_shown_tab(
    std::optional<tabs::TabHandle> tab_handle) {
  last_shown_tab_ = tab_handle;
}

std::optional<tabs::TabHandle> TabGroupAndroid::last_shown_tab() const {
  return last_shown_tab_;
}

TabGroupFeatures* TabGroupAndroid::GetTabGroupFeatures() {
  return tab_group_features_.get();
}

const TabGroupFeatures* TabGroupAndroid::GetTabGroupFeatures() const {
  return tab_group_features_.get();
}

std::unique_ptr<TabGroup> TabGroupAndroid::Factory::Create(
    tabs::TabGroupTabCollection* collection,
    const tab_groups::TabGroupId& id,
    const tab_groups::TabGroupVisualData& visual_data) {
  return std::make_unique<TabGroupAndroid>(profile(), collection, id,
                                           visual_data);
}
