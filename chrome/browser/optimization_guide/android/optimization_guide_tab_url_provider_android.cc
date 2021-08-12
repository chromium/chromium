// Copyright 2021 The Chromium Authors. All rights reserved.
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
  std::vector<std::pair<GURL, absl::optional<base::TimeTicks>>>
      urls_and_active_time;
  for (const TabModel* tab_model : TabModelList::models()) {
    if (tab_model->GetProfile() != profile_)
      continue;

    int tab_count = tab_model->GetTabCount();
    for (int i = 0; i < tab_count; i++) {
      content::WebContents* web_contents = tab_model->GetWebContentsAt(i);
      if (web_contents) {
        if ((base::TimeTicks::Now() - web_contents->GetLastActiveTime()) <
            duration_since_last_shown) {
          urls_and_active_time.push_back(
              std::make_pair(web_contents->GetLastCommittedURL(),
                             web_contents->GetLastActiveTime()));
        }
        continue;
      }

      // Fall back to the tab if there isn't a WebContents created for the tab.
      TabAndroid* tab = tab_model->GetTabAt(i);
      if (tab) {
        // Just push back the URL even though we have no idea if it was shown
        // before. TabAndroid does not expose the last active time.
        urls_and_active_time.push_back(
            std::make_pair(tab->GetURL(), absl::nullopt));
      }
    }
  }
  // Sort by descending active time.
  std::sort(urls_and_active_time.begin(), urls_and_active_time.end(),
            [](const std::pair<GURL, absl::optional<base::TimeTicks>>& a,
               const std::pair<GURL, absl::optional<base::TimeTicks>>& b) {
              if (a.second && b.second)
                return *a.second > *b.second;
              // If b.second has a value, then put that in front. Otherwise,
              // leave the same order.
              return !b.second.has_value();
            });

  std::vector<GURL> urls;
  urls.reserve(urls_and_active_time.size());
  for (const auto& url_and_active_time : urls_and_active_time) {
    urls.emplace_back(url_and_active_time.first);
  }
  return urls;
}

}  // namespace android
}  // namespace optimization_guide
