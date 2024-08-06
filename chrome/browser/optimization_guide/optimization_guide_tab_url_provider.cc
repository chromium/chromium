// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_tab_url_provider.h"

#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif

namespace {

// Returns whether the web contents has been shown in the last 90 days.
bool IsWebContentsCandidateForRefresh(
    content::WebContents* web_contents,
    const base::TimeDelta& duration_since_last_shown) {
  return web_contents &&
         (base::TimeTicks::Now() - web_contents->GetLastActiveTimeTicks()) <
             duration_since_last_shown;
}

}  // namespace

OptimizationGuideTabUrlProvider::OptimizationGuideTabUrlProvider(
    Profile* profile)
    : profile_(profile) {}
OptimizationGuideTabUrlProvider::~OptimizationGuideTabUrlProvider() = default;

const std::vector<GURL> OptimizationGuideTabUrlProvider::GetUrlsOfActiveTabs(
    const base::TimeDelta& duration_since_last_shown) {
  const std::vector<content::WebContents*> web_contents_list =
      GetAllWebContentsForProfile(profile_);
  std::vector<std::pair<GURL, base::TimeTicks>> urls_and_active_time;
  urls_and_active_time.reserve(web_contents_list.size());
  for (content::WebContents* web_contents : web_contents_list) {
    if (IsWebContentsCandidateForRefresh(web_contents,
                                         duration_since_last_shown)) {
      urls_and_active_time.emplace_back(
          std::make_pair(web_contents->GetLastCommittedURL(),
                         web_contents->GetLastActiveTimeTicks()));
    }
  }
  std::sort(urls_and_active_time.begin(), urls_and_active_time.end(),
            [](const std::pair<GURL, base::TimeTicks>& a,
               const std::pair<GURL, base::TimeTicks>& b) {
              return a.second > b.second;
            });

  std::vector<GURL> urls;
  urls.reserve(urls_and_active_time.size());
  for (const auto& url_and_active_time : urls_and_active_time) {
    urls.emplace_back(url_and_active_time.first);
  }
  return urls;
}

const std::vector<content::WebContents*>
OptimizationGuideTabUrlProvider::GetAllWebContentsForProfile(Profile* profile) {
#if BUILDFLAG(IS_ANDROID)
  NOTREACHED_IN_MIGRATION();
  return {};
#else
  std::vector<content::WebContents*> web_contents_list;
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (!browser || browser->profile() != profile)
      continue;

    TabStripModel* tab_strip_model = browser->tab_strip_model();
    int tab_count = tab_strip_model->count();
    for (int i = 0; i < tab_count; i++) {
      content::WebContents* web_contents = tab_strip_model->GetWebContentsAt(i);
      if (web_contents)
        web_contents_list.push_back(web_contents);
    }
  }
  return web_contents_list;
#endif
}
