// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FONT_PREWARMER_TAB_HELPER_H_
#define CHROME_BROWSER_FONT_PREWARMER_TAB_HELPER_H_

#include <string>
#include <vector>

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

// FontPrewarmerTabHelper is responsible for tracking navigations to the search
// results page of the default search engine and prewarming the fonts that were
// previously used the last time a search results page of the default search
// engine was visited.
class FontPrewarmerTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<FontPrewarmerTabHelper> {
 public:
  FontPrewarmerTabHelper(const FontPrewarmerTabHelper&) = delete;
  FontPrewarmerTabHelper& operator=(const FontPrewarmerTabHelper&) = delete;
  ~FontPrewarmerTabHelper() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  friend class content::WebContentsUserData<FontPrewarmerTabHelper>;
  friend class FontPrewarmerTabHelperTest;

  explicit FontPrewarmerTabHelper(content::WebContents* web_contents);

  // Testing helpers:
  static std::string GetSearchResultsPageFontsPref();
  static std::vector<std::string> GetFontNames(Profile* profile);

  Profile* GetProfile();

  // Returns true if the url of `navigation_handle` is a search results page of
  // the default search provider.
  bool IsSearchResultsPageNavigation(
      content::NavigationHandle* navigation_handle);

  // content::WebContentsObserver implementation.
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;

  std::optional<int> expected_render_process_host_id_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_FONT_PREWARMER_TAB_HELPER_H_
