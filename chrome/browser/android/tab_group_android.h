// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_GROUP_ANDROID_H_
#define CHROME_BROWSER_ANDROID_TAB_GROUP_ANDROID_H_

#include <memory>

#include "chrome/browser/android/tab_group_features.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"

namespace tabs {
class TabGroupTabCollection;
class TabGroupFeatures;
}  // namespace tabs

// Android implementation of TabGroup. This subclass is used to construct
// platform-specific TabGroupFeatures.
class TabGroupAndroid : public TabGroup {
 public:
  TabGroupAndroid(Profile* profile,
                  tabs::TabGroupTabCollection* collection,
                  const tab_groups::TabGroupId& id,
                  const tab_groups::TabGroupVisualData& visual_data);
  ~TabGroupAndroid() override;

  class Factory : public TabGroup::Factory {
   public:
    explicit Factory(Profile* profile) : TabGroup::Factory(profile) {}
    ~Factory() override = default;
    std::unique_ptr<TabGroup> Create(
        tabs::TabGroupTabCollection* collection,
        const tab_groups::TabGroupId& id,
        const tab_groups::TabGroupVisualData& visual_data) override;
  };

  // Returns the feature controllers scoped to this tab group.
  TabGroupFeatures* GetTabGroupFeatures() override;
  const TabGroupFeatures* GetTabGroupFeatures() const override;

 private:
  std::unique_ptr<TabGroupFeatures> tab_group_features_;
};

#endif  // CHROME_BROWSER_ANDROID_TAB_GROUP_ANDROID_H_
