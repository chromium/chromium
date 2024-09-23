// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOCOMPLETE_TAB_MATCHER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_AUTOCOMPLETE_TAB_MATCHER_ANDROID_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/tab_matcher.h"
#include "components/search_engines/template_url_service.h"

class AutocompleteInput;
class TemplateURLService;

// Implementation of TabMatcher targeting Android platform.
class TabMatcherAndroid : public TabMatcher {
 public:
  TabMatcherAndroid(const TemplateURLService* template_url_service,
                    Profile* profile)
      : template_url_service_{template_url_service}, profile_{profile} {}

  // TabMatcher implementation.
  bool IsTabOpenWithURL(const GURL& gurl,
                        const AutocompleteInput* input) const override;
  void FindMatchingTabs(GURLToTabInfoMap* map,
                        const AutocompleteInput* input) const override;
  std::vector<TabMatcher::TabWrapper> GetOpenTabs() const override;

 private:
  std::vector<raw_ptr<TabAndroid, VectorExperimental>> GetOpenAndroidTabs()
      const;
  GURLToTabInfoMap GetAllHiddenAndNonCCTTabInfos(
      const bool keep_search_intent_params,
      const bool normalize_search_terms) const;

  raw_ptr<const TemplateURLService> template_url_service_;
  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_ANDROID_AUTOCOMPLETE_TAB_MATCHER_ANDROID_H_
