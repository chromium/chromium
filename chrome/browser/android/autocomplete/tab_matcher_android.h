// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOCOMPLETE_TAB_MATCHER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_AUTOCOMPLETE_TAB_MATCHER_ANDROID_H_

#include <vector>

#include "chrome/browser/autocomplete/tab_matcher_desktop.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "content/public/browser/web_contents.h"

class TabAndroid;
class TabModel;

// Implementation of TabMatcher targeting Android platform.
// TODO(crbug.com/1176768): The code depends on Desktop's implementation for a
// certain hack that does not help Android code much. Remove the dependency.
class TabMatcherAndroid : public TabMatcherDesktop {
 public:
  TabMatcherAndroid(const AutocompleteProviderClient& client, Profile* profile)
      : TabMatcherDesktop(client, profile) {}

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
};

#endif  // CHROME_BROWSER_ANDROID_AUTOCOMPLETE_TAB_MATCHER_ANDROID_H_
