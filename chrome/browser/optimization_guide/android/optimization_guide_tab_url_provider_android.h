// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_ANDROID_OPTIMIZATION_GUIDE_TAB_URL_PROVIDER_ANDROID_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_ANDROID_OPTIMIZATION_GUIDE_TAB_URL_PROVIDER_ANDROID_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/tab_url_provider.h"

class Profile;

namespace optimization_guide {
namespace android {

// Implementation of OptimizationGuideTabUrlProvider that gets URLs from Android
// browser windows.
class OptimizationGuideTabUrlProviderAndroid
    : public optimization_guide::TabUrlProvider {
 public:
  explicit OptimizationGuideTabUrlProviderAndroid(Profile* profile);
  ~OptimizationGuideTabUrlProviderAndroid() override;

  // optimization_guide::TabUrlProvider:
  const std::vector<GURL> GetUrlsOfActiveTabs(
      const base::TimeDelta& duration_since_last_shown) override;

 private:
  friend class OptimizationGuideTabUrlProviderAndroidTest;

  struct TabRepresentation {
    TabRepresentation();
    ~TabRepresentation();
    TabRepresentation(const TabRepresentation&);

    // The URL displayed on the tab.
    GURL url;

    // The index of the tab model the tab is in.
    size_t tab_model_index;
    // The index of the tab within the tab model.
    size_t tab_index;

    // The time the tab was last active.
    std::optional<base::TimeTicks> last_active_time;
  };

  // Sorts |tabs|. Sorts by descending last active time (if present) and then by
  // its position in the tab model.
  //
  // Exposed for testing purposes.
  void SortTabs(std::vector<TabRepresentation>* tabs);

  // The profile associated with this tab URL provider.
  raw_ptr<Profile> profile_;
};

}  // namespace android
}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_ANDROID_OPTIMIZATION_GUIDE_TAB_URL_PROVIDER_ANDROID_H_
