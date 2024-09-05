// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/tablet_mode/chrome_content_browser_client_tablet_mode_part.h"

#include <string_view>

#include "base/feature_list.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
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
#include "content/public/common/url_constants.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/display/screen.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "extensions/common/constants.h"
#endif

namespace {

GURL GetURL(content::WebContents* contents) {
  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  return entry ? entry->GetURL() : GURL();
}

// Returns true if |contents| is of an internal pages (such as
// chrome://settings, chrome://extensions, ... etc).
bool IsInternalPage(content::WebContents* contents) {
  DCHECK(contents);

  GURL url = GetURL(contents);
  if (url.is_empty()) {
    // No entry has been committed. We don't know anything about this page, so
    // exclude it and let it have the default web prefs.
    return true;
  }

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  if (profile && search::IsNTPOrRelatedURL(url, profile))
    return false;

  return url.SchemeIs(content::kChromeUIScheme);
}

void OverrideWebkitPrefsForTabletMode(
    content::WebContents* contents,
    blink::web_pref::WebPreferences* web_prefs) {
  // Enable some mobile-like behaviors when in tablet mode on Chrome OS.
  if (!display::Screen::GetScreen()->HasScreen() ||
      !display::Screen::GetScreen()->InTabletMode()) {
    return;
  }

  // Do this only for webcontents displayed in browsers and are not of hosted
  // apps.
  auto* browser = chrome::FindBrowserWithTab(contents);
  if (!browser || browser->is_type_app() || browser->is_type_app_popup())
    return;

  if (IsInternalPage(contents))
    return;

  web_prefs->double_tap_to_zoom_enabled =
      base::FeatureList::IsEnabled(features::kDoubleTapToZoomInTabletMode);
  web_prefs->text_autosizing_enabled = true;
  web_prefs->shrinks_viewport_contents_to_fit = true;
  web_prefs->main_frame_resizes_are_orientation_changes = true;
  web_prefs->default_minimum_page_scale_factor = 0.25f;
  web_prefs->default_maximum_page_scale_factor = 5.0;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Returns true if the WebUI at |url| is considered "system UI" and should use
// the system font size (the default) instead of the browser font size.
// Takes a URL because the WebContents may not yet be associated with a window,
// SettingsWindowManager, etc.
bool UseDefaultFontSize(const GURL& url) {
  if (ash::SystemWebDialogDelegate::HasInstance(url))
    return true;

  if (url.SchemeIs(content::kChromeUIScheme))
    return chrome::IsSystemWebUIHost(url.host_piece());

  if (url.SchemeIs(extensions::kExtensionScheme)) {
    std::string_view extension_id = url.host_piece();
    return extension_misc::IsSystemUIApp(extension_id);
  }
  return false;
}

void OverrideFontSize(content::WebContents* contents,
                      blink::web_pref::WebPreferences* web_prefs) {
  DCHECK(contents);
  // Check the URL because |contents| may not yet be associated with a window,
  // SettingsWindowManager, etc.
  GURL url = GetURL(contents);
  if (!url.is_empty() && UseDefaultFontSize(url)) {
    // System dialogs are considered native UI, so they do not follow the
    // browser's web-page font sizes. Reset fonts to the base sizes.
    blink::web_pref::WebPreferences base_prefs;
    web_prefs->default_font_size = base_prefs.default_font_size;
    web_prefs->default_fixed_font_size = base_prefs.default_fixed_font_size;
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

ChromeContentBrowserClientTabletModePart::
    ChromeContentBrowserClientTabletModePart() = default;

ChromeContentBrowserClientTabletModePart::
    ~ChromeContentBrowserClientTabletModePart() = default;

void ChromeContentBrowserClientTabletModePart::OverrideWebkitPrefs(
    content::WebContents* web_contents,
    blink::web_pref::WebPreferences* web_prefs) {
  // A webcontents may not be the delegate of the render view host such as in
  // the case of interstitial pages.
  if (!web_contents)
    return;

  OverrideWebkitPrefsForTabletMode(web_contents, web_prefs);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  OverrideFontSize(web_contents, web_prefs);
#endif
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// static
bool ChromeContentBrowserClientTabletModePart::UseDefaultFontSizeForTest(
    const GURL& url) {
  return UseDefaultFontSize(url);
}
#endif
