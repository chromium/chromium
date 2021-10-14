// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOCOMPLETE_TAB_MATCHER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_AUTOCOMPLETE_TAB_MATCHER_ANDROID_H_

#include <vector>

#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/omnibox/browser/tab_matcher.h"
#include "content/public/browser/web_contents.h"

class TabAndroid;
class TabModel;

// Implementation of TabMatcher targeting Android platform.
class TabMatcherAndroid : public TabMatcher {
 public:
  TabMatcherAndroid(const AutocompleteProviderClient& client, Profile* profile)
      : client_{client}, profile_{profile} {}

  bool IsTabOpenWithURL(const GURL& gurl,
                        const AutocompleteInput* input) const override;

  // Returns a TabAndroid has opened same URL as |url|.
  TabAndroid* GetTabOpenWithURL(const GURL& url,
                                const AutocompleteInput* input) const;

 private:
  // Make a JNI call to get all the hidden tabs and non Custom tabs in
  // |tab_model|.
  std::vector<TabAndroid*> GetAllHiddenAndNonCCTTabs(
      const std::vector<TabModel*>& tab_models) const;

  const AutocompleteProviderClient& client_;
  Profile* profile_;
};

#endif  // CHROME_BROWSER_ANDROID_AUTOCOMPLETE_TAB_MATCHER_ANDROID_H_
