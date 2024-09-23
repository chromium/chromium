// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/android/optimization_guide_tab_url_provider_android.h"

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "content/public/browser/web_contents.h"

namespace optimization_guide {
namespace android {

OptimizationGuideTabUrlProviderAndroid::OptimizationGuideTabUrlProviderAndroid(
    Profile* profile)
    : profile_(profile) {}
OptimizationGuideTabUrlProviderAndroid::
    ~OptimizationGuideTabUrlProviderAndroid() = default;

const std::vector<GURL>
OptimizationGuideTabUrlProviderAndroid::GetUrlsOfActiveTabs(
    const base::TimeDelta& duration_since_last_shown) {
  std::vector<TabRepresentation> tabs;

  const TabModelList::TabModelVector& tab_models = TabModelList::models();
  for (size_t tab_model_idx = 0; tab_model_idx < tab_models.size();
       tab_model_idx++) {
    const TabModel* tab_model = tab_models[tab_model_idx];
    if (tab_model->GetProfile() != profile_)
      continue;

    size_t tab_count = static_cast<size_t>(tab_model->GetTabCount());
    for (size_t tab_idx = 0; tab_idx < tab_count; tab_idx++) {
      TabRepresentation tab;
      tab.tab_model_index = tab_model_idx;
      tab.tab_index = tab_idx;

      content::WebContents* web_contents = tab_model->GetWebContentsAt(tab_idx);
      if (web_contents) {
        if ((base::TimeTicks::Now() - web_contents->GetLastActiveTimeTicks()) <
            duration_since_last_shown) {
          tab.url = web_contents->GetLastCommittedURL();
          tab.last_active_time = web_contents->GetLastActiveTimeTicks();
          tabs.push_back(tab);
        }
        continue;
      }

      // Fall back to the tab if there isn't a WebContents created for the tab.
      TabAndroid* tab_android = tab_model->GetTabAt(tab_idx);
      if (tab_android) {
        // Just push back the URL even though we have no idea if it was shown
        // before. TabAndroid does not expose the last active time.
        tab.url = tab_android->GetURL();
        tabs.push_back(tab);
      }
    }
  }
  SortTabs(&tabs);

  std::vector<GURL> urls;
  urls.reserve(tabs.size());
  for (const auto& tab : tabs) {
    urls.emplace_back(tab.url);
  }
  return urls;
}

void OptimizationGuideTabUrlProviderAndroid::SortTabs(
    std::vector<TabRepresentation>* tabs) {
  std::sort(tabs->begin(), tabs->end(),
            [](const TabRepresentation& a, const TabRepresentation& b) {
              // Attempt to sort by last active time if both are present.
              if (a.last_active_time && b.last_active_time)
                return *a.last_active_time > *b.last_active_time;

              // If both are not present, sort by its position in the tab model
              // list, assuming that the earlier it appears, the more likely it
              // will get revisited.
              if (!a.last_active_time && !b.last_active_time) {
                return (a.tab_model_index != b.tab_model_index)
                           ? (a.tab_model_index < b.tab_model_index)
                           : (a.tab_index < b.tab_index);
              }

              // Otherwise, if one of the tabs has an active time, put that
              // first.
              return a.last_active_time.value_or(base::TimeTicks::Min()) >
                     b.last_active_time.value_or(base::TimeTicks::Min());
            });
}

OptimizationGuideTabUrlProviderAndroid::TabRepresentation::TabRepresentation() =
    default;
OptimizationGuideTabUrlProviderAndroid::TabRepresentation::
    ~TabRepresentation() = default;
OptimizationGuideTabUrlProviderAndroid::TabRepresentation::TabRepresentation(
    const TabRepresentation&) = default;

}  // namespace android
}  // namespace optimization_guide
