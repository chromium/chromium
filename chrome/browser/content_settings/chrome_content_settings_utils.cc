// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/chrome_content_settings_utils.h"

#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view.h"
#include "content/public/browser/web_contents.h"
#endif

namespace content_settings {

void RecordPopupsAction(PopupsAction action) {
  UMA_HISTOGRAM_ENUMERATION("ContentSettings.Popups", action,
                            POPUPS_ACTION_COUNT);
}

void UpdateLocationBarUiForWebContents(content::WebContents* web_contents) {
#if !BUILDFLAG(IS_ANDROID)
  Browser* browser = chrome::FindBrowserWithTab(web_contents);
  if (!browser)
    return;

  if (browser->tab_strip_model()->GetActiveWebContents() != web_contents)
    return;

  LocationBar* location_bar = browser->window()->GetLocationBar();
  if (location_bar)
    location_bar->UpdateContentSettingsIcons();

  // The document PiP window does not have a location bar, but has some content
  // setting views that need to be updated too.
  if (browser->is_type_picture_in_picture()) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
    auto* frame_view = static_cast<PictureInPictureBrowserFrameView*>(
        browser_view->frame()->GetFrameView());
    frame_view->UpdateContentSettingsIcons();
  }
#endif
}

}  // namespace content_settings
