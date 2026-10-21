// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OMNIBOX_CHROME_OMNIBOX_NAVIGATION_OBSERVER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_OMNIBOX_CHROME_OMNIBOX_NAVIGATION_OBSERVER_ANDROID_H_

#include <string>

#include "chrome/browser/omnibox/chrome_omnibox_navigation_observer_base.h"
#include "components/omnibox/browser/autocomplete_match.h"

class Profile;

namespace content {
class NavigationHandle;
}  // namespace content

// Android implementation of ChromeOmniboxNavigationObserverBase. When the
// alternative navigation probe succeeds, shows an Android message prompt once
// the search result has also loaded.
//
// Please see the class comment on the base class for important information
// about the memory management of this object.
class ChromeOmniboxNavigationObserverAndroid
    : public ChromeOmniboxNavigationObserverBase {
 public:
  // Create ChromeOmniboxNavigationObserverAndroid.
  static void Create(
      content::NavigationHandle* navigation_handle,
      Profile* profile,
      const std::u16string& omnibox_user_input,
      const AutocompleteMatch& selected_match,
      const AutocompleteMatch& alternative_nav_match = AutocompleteMatch());

 private:
  friend class ChromeOmniboxNavigationObserverAndroidTest;

  ChromeOmniboxNavigationObserverAndroid(
      content::NavigationHandle& navigation_handle,
      Profile* profile,
      const std::u16string& omnibox_user_input,
      const AutocompleteMatch& selected_match,
      const AutocompleteMatch& alternative_nav_match,
      network::mojom::URLLoaderFactory* loader_factory = nullptr);

  ~ChromeOmniboxNavigationObserverAndroid() override;

  void ShowAlternativeNavMessage();
};

#endif  // CHROME_BROWSER_ANDROID_OMNIBOX_CHROME_OMNIBOX_NAVIGATION_OBSERVER_ANDROID_H_
