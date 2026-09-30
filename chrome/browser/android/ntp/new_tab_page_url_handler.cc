// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/new_tab_page_url_handler.h"

#include <string>

#include "base/android/device_info.h"
#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/extension_url_overrides.h"
#endif

namespace {
const char kBookmarkFolderPath[] = "folder/";
}

namespace chrome {
namespace android {

bool HandleAndroidNativePageURL(GURL* url,
                                content::BrowserContext* browser_context) {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  if (base::FeatureList::IsEnabled(
          chrome::android::kChromeNativeUrlOverriding)) {
    // If an extension is overriding this URL, do not redirect it.
    if (ExtensionUrlOverrides::GetNumberOfExtensionsOverridingURL(
            *url, browser_context) > 0) {
      return false;
    }
  }
#endif

  if (url->SchemeIs(content::kChromeUIScheme)) {
    if (url->GetHost() == chrome::kChromeUINewTabHost) {
      // The url handler rewrites the url from chrome://newtab to the WebUI
      // NTP if the WebUI NTP is enabled. Depending on the default search
      // engine, it will either return a 1P or 3P NTP.
      if (search::IsWebUiNtpEnabledForDesktopAndroid()) {
        GURL new_tab_url = search::GetNewTabPageURL(
            Profile::FromBrowserContext(browser_context));
        if (new_tab_url.is_valid()) {
          *url = new_tab_url;
        } else {
          // TODO(b/559771711): Add fallback WebUI NTP.
          *url = GURL(chrome::kChromeUINativeNewTabURL);
        }
      } else {
        *url = GURL(chrome::kChromeUINativeNewTabURL);
      }
      return true;
    }
  }

  // LINT.IfChange(AndroidDesktopWebUiHistory)
  if (base::android::device_info::is_desktop() &&
      base::FeatureList::IsEnabled(
          chrome::android::kAndroidDesktopWebUiHistory)) {
    if (url->SchemeIs(chrome::kChromeNativeScheme) &&
        url->GetHost() == kChromeUIHistoryHost) {
      GURL::Replacements replacements;
      replacements.SetSchemeStr(content::kChromeUIScheme);
      *url = url->ReplaceComponents(replacements);
      return true;
    }
  }
  // LINT.ThenChange(//chrome/browser/history/java/src/org/chromium/chrome/browser/history/HistoryFeatures.java:AndroidDesktopWebUiHistory,
  // //chrome/browser/ui/webui/history/history_ui.cc:AndroidDesktopWebUiHistory)

  if (url->SchemeIs(chrome::kChromeNativeScheme) &&
      url->GetHost() == kChromeUIBookmarksHost) {
    std::string ref = url->GetRef();
    if (!ref.empty()) {
      *url = GURL(std::string(kChromeUINativeBookmarksURL)
                      .append(kBookmarkFolderPath)
                      .append(ref));
      return true;
    }
  }

  return false;
}

}  // namespace android
}  // namespace chrome
