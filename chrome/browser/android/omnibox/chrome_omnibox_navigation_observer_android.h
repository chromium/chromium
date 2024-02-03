// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OMNIBOX_CHROME_OMNIBOX_NAVIGATION_OBSERVER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_OMNIBOX_CHROME_OMNIBOX_NAVIGATION_OBSERVER_ANDROID_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"

class Profile;

// Monitors omnibox navigations in order to trigger behaviors on Android.
class ChromeOmniboxNavigationObserverAndroid
    : public base::RefCounted<ChromeOmniboxNavigationObserverAndroid>,
      public content::WebContentsObserver {
 public:
  // Create ChromeOmniboxNavigationObserverAndroid.
  static void Create(content::NavigationHandle* navigation_handle,
                     Profile* profile,
                     const std::u16string& omnibox_user_input,
                     const AutocompleteMatch& selected_match);

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  friend class base::RefCounted<ChromeOmniboxNavigationObserverAndroid>;
  friend class ChromeOmniboxNavigationObserverAndroidTest;
  FRIEND_TEST_ALL_PREFIXES(ChromeOmniboxNavigationObserverAndroidTest,
                           NotCommitted);
  FRIEND_TEST_ALL_PREFIXES(ChromeOmniboxNavigationObserverAndroidTest,
                           NoHeaders);
  FRIEND_TEST_ALL_PREFIXES(ChromeOmniboxNavigationObserverAndroidTest,
                           AllHttp200AreFine);
  FRIEND_TEST_ALL_PREFIXES(ChromeOmniboxNavigationObserverAndroidTest,
                           SelectNonHttp200CodesAreFine);

  ChromeOmniboxNavigationObserverAndroid(
      content::NavigationHandle* navigation_handle,
      Profile* profile,
      const std::u16string& omnibox_user_input,
      const AutocompleteMatch& selected_match);
  ~ChromeOmniboxNavigationObserverAndroid() override;
  bool NavigationEligible(content::NavigationHandle* navigation_handle);

  const raw_ptr<Profile> profile_;
  const std::u16string omnibox_user_input_;
  const AutocompleteMatch selected_match_;
};

#endif  // CHROME_BROWSER_ANDROID_OMNIBOX_CHROME_OMNIBOX_NAVIGATION_OBSERVER_ANDROID_H_
