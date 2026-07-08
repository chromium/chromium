// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MULTISTEP_FILTER_CHROME_FILTER_NAVIGATION_OBSERVER_TEST_API_H_
#define CHROME_BROWSER_MULTISTEP_FILTER_CHROME_FILTER_NAVIGATION_OBSERVER_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/multistep_filter/chrome_filter_navigation_observer.h"

namespace multistep_filter {

class ContentFilterNavigationObserver;

class ChromeFilterNavigationObserverTestApi {
 public:
  explicit ChromeFilterNavigationObserverTestApi(
      ChromeFilterNavigationObserver& observer)
      : observer_(observer) {}

  ChromeFilterNavigationObserverTestApi(
      const ChromeFilterNavigationObserverTestApi&) = delete;
  ChromeFilterNavigationObserverTestApi& operator=(
      const ChromeFilterNavigationObserverTestApi&) = delete;

  ~ChromeFilterNavigationObserverTestApi() = default;

  ContentFilterNavigationObserver* GetObserver() {
    return observer_->observer_.get();
  }

 private:
  const raw_ref<ChromeFilterNavigationObserver> observer_;
};

inline ChromeFilterNavigationObserverTestApi test_api(
    ChromeFilterNavigationObserver& observer) {
  return ChromeFilterNavigationObserverTestApi(observer);
}

}  // namespace multistep_filter

#endif  // CHROME_BROWSER_MULTISTEP_FILTER_CHROME_FILTER_NAVIGATION_OBSERVER_TEST_API_H_
