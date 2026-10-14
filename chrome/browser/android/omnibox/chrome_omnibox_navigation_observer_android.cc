// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/omnibox/chrome_omnibox_navigation_observer_android.h"

#include "content/public/browser/navigation_handle.h"

// static
void ChromeOmniboxNavigationObserverAndroid::Create(
    content::NavigationHandle* navigation_handle,
    Profile* profile,
    const std::u16string& omnibox_user_input,
    const AutocompleteMatch& selected_match,
    const AutocompleteMatch& alternative_nav_match) {
  if (!navigation_handle) {
    return;
  }

  // The observer will be kept alive until the navigation and the alternative
  // fetch (if any) finish.
  new ChromeOmniboxNavigationObserverAndroid(*navigation_handle, profile,
                                             omnibox_user_input, selected_match,
                                             alternative_nav_match);
}

ChromeOmniboxNavigationObserverAndroid::ChromeOmniboxNavigationObserverAndroid(
    content::NavigationHandle& navigation_handle,
    Profile* profile,
    const std::u16string& omnibox_user_input,
    const AutocompleteMatch& selected_match,
    const AutocompleteMatch& alternative_nav_match,
    network::mojom::URLLoaderFactory* loader_factory)
    : ChromeOmniboxNavigationObserverBase(navigation_handle,
                                          profile,
                                          omnibox_user_input,
                                          selected_match,
                                          alternative_nav_match,
                                          loader_factory) {}

ChromeOmniboxNavigationObserverAndroid::
    ~ChromeOmniboxNavigationObserverAndroid() = default;
