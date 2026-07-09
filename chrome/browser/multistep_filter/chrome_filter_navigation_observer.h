// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MULTISTEP_FILTER_CHROME_FILTER_NAVIGATION_OBSERVER_H_
#define CHROME_BROWSER_MULTISTEP_FILTER_CHROME_FILTER_NAVIGATION_OBSERVER_H_

#include <memory>

#include "chrome/browser/ui/tabs/contents_observing_tab_feature.h"
#include "components/multistep_filter/content/content_filter_navigation_observer.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace content {
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace multistep_filter {

// Observes navigations to trigger Multistep Filter feature logic.
// This chrome-specific observer manages the lifetime of the component
// ContentFilterNavigationObserver and bridges it with the TabInterface.
//
// Owned by `tabs::TabFeatures`, one instance per tab.
class ChromeFilterNavigationObserver
    : public tabs::ContentsObservingTabFeature {
 public:
  DECLARE_USER_DATA(ChromeFilterNavigationObserver);

  static ChromeFilterNavigationObserver* From(tabs::TabInterface* tab);

  explicit ChromeFilterNavigationObserver(tabs::TabInterface& tab);
  ChromeFilterNavigationObserver(const ChromeFilterNavigationObserver&) =
      delete;
  ChromeFilterNavigationObserver& operator=(
      const ChromeFilterNavigationObserver&) = delete;

  ~ChromeFilterNavigationObserver() override;

  // tabs::ContentsObservingTabFeature:
  void OnDiscardContents(tabs::TabInterface* tab,
                         content::WebContents* old_contents,
                         content::WebContents* new_contents) override;

 protected:
  virtual void UpdateObserver(content::WebContents* web_contents);

  // The component-level observer that monitors navigations.
  std::unique_ptr<ContentFilterNavigationObserver> observer_;

 private:
  friend class ChromeFilterNavigationObserverTestApi;
  ui::ScopedUnownedUserData<ChromeFilterNavigationObserver>
      scoped_unowned_user_data_;
};

}  // namespace multistep_filter

#endif  // CHROME_BROWSER_MULTISTEP_FILTER_CHROME_FILTER_NAVIGATION_OBSERVER_H_
