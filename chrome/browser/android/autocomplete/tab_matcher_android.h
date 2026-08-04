// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOCOMPLETE_TAB_MATCHER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_AUTOCOMPLETE_TAB_MATCHER_ANDROID_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/tab_matcher.h"
#include "components/search_engines/template_url_service.h"

namespace content {
class WebContents;
}

class AutocompleteInput;
class TabAndroid;
class TemplateURLService;

// Implementation of TabMatcher targeting Android platform.
class TabMatcherAndroid : public TabMatcher {
 public:
  using WebContentsGetter = base::RepeatingCallback<content::WebContents*()>;
  TabMatcherAndroid(const TemplateURLService* template_url_service,
                    Profile* profile,
                    WebContentsGetter web_contents_getter);
  ~TabMatcherAndroid() override;

  // TabMatcher implementation.
  bool IsTabOpenWithURL(const GURL& gurl,
                        const AutocompleteInput* input) const override;
  void FindMatchingTabs(GURLToTabInfoMap* map,
                        const AutocompleteInput* input) const override;
  std::vector<TabMatcher::TabWrapper> GetOpenTabs(
      const AutocompleteInput* input,
      bool unused_exclude_active_tab = true) const override;

 private:
  std::vector<int64_t> GetOpenAndroidTabs(const AutocompleteInput* input) const;
  GURLToTabInfoMap GetAllHiddenAndNonCCTTabInfos(
      const AutocompleteInput* input) const;

  raw_ptr<const TemplateURLService> template_url_service_;
  raw_ptr<Profile> profile_;
  WebContentsGetter web_contents_getter_;
};

#endif  // CHROME_BROWSER_ANDROID_AUTOCOMPLETE_TAB_MATCHER_ANDROID_H_
