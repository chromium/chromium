// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/chrome_content_settings_utils.h"

#include "base/metrics/histogram_macros.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#endif

namespace content_settings {

void RecordPluginsAction(PluginsAction action) {
  UMA_HISTOGRAM_ENUMERATION("ContentSettings.Plugins", action,
                            PLUGINS_ACTION_COUNT);
}

void RecordPopupsAction(PopupsAction action) {
  UMA_HISTOGRAM_ENUMERATION("ContentSettings.Popups", action,
                            POPUPS_ACTION_COUNT);
}

void UpdateLocationBarUiForWebContents(content::WebContents* web_contents) {
#if !defined(OS_ANDROID)
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;

  if (browser->tab_strip_model()->GetActiveWebContents() != web_contents)
    return;

  LocationBar* location_bar = browser->window()->GetLocationBar();
  if (location_bar)
    location_bar->UpdateContentSettingsIcons();
#endif
}

#if !defined(OS_ANDROID)
std::u16string GetPermissionDetailString(Profile* profile,
                                         ContentSettingsType content_type,
                                         const GURL& url) {
  if (!url.is_valid())
    return {};

  switch (content_type) {
    case ContentSettingsType::FILE_HANDLING:
      return web_app::GetFileTypeAssociationsHandledByWebAppsForDisplay(profile,
                                                                        url);
    default:
      return {};
  }
}
#endif

}  // namespace content_settings
