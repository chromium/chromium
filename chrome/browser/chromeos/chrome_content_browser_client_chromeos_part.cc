// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/chrome_content_browser_client_chromeos_part.h"

#include "base/feature_list.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/web_preferences.h"

namespace {

// Returns true if |contents| is of an internal pages (such as
// chrome://settings, chrome://extensions, ... etc) or the New Tab Page.
bool ShouldExcludePage(content::WebContents* contents) {
  DCHECK(contents);

  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  if (!entry) {
    // No entry has been committed. We don't know anything about this page, so
    // exclude it and let it have the default web prefs.
    return true;
  }

  const GURL& url = entry->GetURL();
  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  if (profile && search::IsNTPURL(url, profile))
    return true;

  return url.SchemeIs("chrome");
}

}  // namespace

ChromeContentBrowserClientChromeOsPart ::
    ChromeContentBrowserClientChromeOsPart() = default;

ChromeContentBrowserClientChromeOsPart ::
    ~ChromeContentBrowserClientChromeOsPart() = default;

void ChromeContentBrowserClientChromeOsPart::OverrideWebkitPrefs(
    content::RenderViewHost* rvh,
    content::WebPreferences* web_prefs) {
  // Enable some mobile-like behaviors when in tablet mode on Chrome OS.
  if (!TabletModeClient::Get() ||
      !TabletModeClient::Get()->tablet_mode_enabled()) {
    return;
  }

  // Do this only for webcontents displayed in browsers and are not of hosted
  // apps.
  content::WebContents* contents =
      content::WebContents::FromRenderViewHost(rvh);

  // A webcontents may not be the delegate of the render view host such as in
  // the case of interstitial pages.
  if (!contents)
    return;

  auto* browser = chrome::FindBrowserWithWebContents(contents);
  if (!browser || browser->is_app())
    return;

  // Also exclude internal pages and NTPs.
  if (ShouldExcludePage(contents))
    return;

  web_prefs->double_tap_to_zoom_enabled =
      base::FeatureList::IsEnabled(features::kDoubleTapToZoomInTabletMode);
  web_prefs->text_autosizing_enabled = true;
  web_prefs->shrinks_viewport_contents_to_fit = true;
  web_prefs->main_frame_resizes_are_orientation_changes = true;
  web_prefs->default_minimum_page_scale_factor = 0.25f;
  web_prefs->default_maximum_page_scale_factor = 5.0;
}
