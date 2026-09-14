// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_TABLET_MODE_TABLET_MODE_PAGE_BEHAVIOR_H_
#define CHROME_BROWSER_CHROMEOS_TABLET_MODE_TABLET_MODE_PAGE_BEHAVIOR_H_

#include "base/scoped_observation.h"
#include "chromeos/ash/components/browser_delegate/browser_controller.h"
#include "ui/display/display_observer.h"

namespace content {
class WebContents;
}  // namespace content

// Updates WebContents Blink preferences on tablet mode state change.
class TabletModePageBehavior : public display::DisplayObserver,
                               public ash::BrowserController::TabObserver {
 public:
  TabletModePageBehavior();

  TabletModePageBehavior(const TabletModePageBehavior&) = delete;
  TabletModePageBehavior& operator=(const TabletModePageBehavior&) = delete;

  ~TabletModePageBehavior() override;

  // Notify the tablet mode change.
  void OnTabletModeToggled(bool enabled);

 private:
  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // ash::BrowserController::TabObserver:
  void OnTabInserted(ash::BrowserDelegate* browser,
                     content::WebContents* contents) override;

  // Enables/disables mobile-like behavior for webpages in existing browsers, as
  // well as starts observing new browser pages if |enabled| is true.
  void SetMobileLikeBehaviorEnabled(bool enabled);

  // We only override the WebKit preferences of webcontents that belong to
  // tabstrips in browsers. When a webcontents is newly created, its WebKit
  // preferences are refreshed *before* it's added to any tabstrip, hence
  // `ChromeContentBrowserClientTabletModePart::OverrideWebPreferences()`
  // wouldn't be able to override the mobile-like behavior prefs we want.
  // Therefore, we need to observe webcontents being added to the tabstrips in
  // order to trigger a refresh of its WebKit prefs.
  base::ScopedObservation<ash::BrowserController,
                          ash::BrowserController::TabObserver>
      tab_observation_{this};
};

#endif  // CHROME_BROWSER_CHROMEOS_TABLET_MODE_TABLET_MODE_PAGE_BEHAVIOR_H_
