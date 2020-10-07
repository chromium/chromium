// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/interstitials/chrome_settings_page_helper.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_ANDROID)
#include "chrome/browser/safe_browsing/android/safe_browsing_settings_launcher_android.h"
#else
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#endif

namespace security_interstitials {

// static
std::unique_ptr<security_interstitials::SettingsPageHelper>
ChromeSettingsPageHelper::CreateChromeSettingsPageHelper() {
  return std::make_unique<security_interstitials::ChromeSettingsPageHelper>();
}

void ChromeSettingsPageHelper::OpenEnhancedProtectionSettings(
    content::WebContents* web_contents) const {
#if defined(OS_ANDROID)
  safe_browsing::ShowSafeBrowsingSettings(web_contents);
#else
  chrome::ShowSafeBrowsingEnhancedProtection(
      chrome::FindBrowserWithWebContents(web_contents));
#endif
}

}  // namespace security_interstitials
