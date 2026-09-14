// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/tablet_mode/tablet_mode_page_behavior.h"

#include "base/check.h"
#include "chrome/browser/chromeos/arc/arc_web_contents_data.h"
#include "chromeos/ash/components/browser_delegate/browser_controller.h"
#include "chromeos/ash/components/browser_delegate/browser_delegate.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"

TabletModePageBehavior::TabletModePageBehavior() {
  display::Screen::Get()->AddObserver(this);
  OnTabletModeToggled(display::Screen::Get()->InTabletMode());
}

TabletModePageBehavior::~TabletModePageBehavior() {
  display::Screen::Get()->RemoveObserver(this);
}

void TabletModePageBehavior::OnTabletModeToggled(bool enabled) {
  SetMobileLikeBehaviorEnabled(enabled);
  ui::TouchUiController::Get()->OnTabletModeToggled(enabled);
}

void TabletModePageBehavior::OnDisplayTabletStateChanged(
    display::TabletState state) {
  switch (state) {
    case display::TabletState::kInTabletMode:
      OnTabletModeToggled(true);
      return;
    case display::TabletState::kInClamshellMode:
      OnTabletModeToggled(false);
      return;
    case display::TabletState::kEnteringTabletMode:
    case display::TabletState::kExitingTabletMode:
      break;
  }
}

void TabletModePageBehavior::OnTabInserted(ash::BrowserDelegate* browser,
                                           content::WebContents* contents) {
  // We limit the mobile-like behavior to webcontents in tabstrips since many
  // apps and extensions draw their own caption buttons and header frames. We
  // don't want those to shrink down and resize to fit the width of their
  // windows like webpages on mobile do. So this behavior is limited to webpages
  // in tabs and packaged apps.
  contents->NotifyPreferencesChanged();
}

void TabletModePageBehavior::SetMobileLikeBehaviorEnabled(bool enabled) {
  auto* browser_controller = ash::BrowserController::GetInstance();

  if (enabled) {
    CHECK(!tab_observation_.IsObserving());
    tab_observation_.Observe(browser_controller);
  } else {
    tab_observation_.Reset();
  }

  // Toggling tablet mode on/off should trigger refreshing the WebKit
  // preferences, since in tablet mode, we enable certain mobile-like features
  // such as "double tap to zoom", "shrink page contents to fit", ... etc.
  // Do this only for webpages that belong to existing browsers as well as
  // future browsers and webcontents.
  browser_controller->ForEachBrowser(
      ash::BrowserController::BrowserOrder::kAscendingActivationTime,
      [enabled](ash::BrowserDelegate& browser) {
        for (tabs::TabInterface* tab : browser.GetTabIterator()) {
          content::WebContents* web_contents = tab->GetContents();
          web_contents->NotifyPreferencesChanged();

          if (!enabled) {
            // For a tab that is requesting its mobile version site (via
            // chrome::ToggleRequestTabletSite()), and is not originated from
            // ARC context, return to its normal version site when exiting
            // tablet mode.
            content::NavigationController& controller =
                web_contents->GetController();
            content::NavigationEntry* entry =
                controller.GetLastCommittedEntry();
            if (entry && entry->GetIsOverridingUserAgent() &&
                !web_contents->GetUserData(
                    arc::ArcWebContentsData::kArcTransitionFlag)) {
              entry->SetIsOverridingUserAgent(false);
              controller.LoadOriginalRequestURL();
            }
          }
        }
        return ash::BrowserController::kContinueIteration;
      });
}
