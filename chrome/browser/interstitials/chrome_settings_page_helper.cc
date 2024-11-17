// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/interstitials/chrome_settings_page_helper.h"
#include "build/build_config.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/safe_browsing/android/safe_browsing_settings_navigation_android.h"
#include "components/safe_browsing/core/common/safe_browsing_settings_metrics.h"
#else
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "components/safe_browsing/core/common/safebrowsing_referral_methods.h"
#endif

namespace security_interstitials {

// static
std::unique_ptr<security_interstitials::SettingsPageHelper>
ChromeSettingsPageHelper::CreateChromeSettingsPageHelper() {
  return std::make_unique<security_interstitials::ChromeSettingsPageHelper>();
}

void ChromeSettingsPageHelper::OpenEnhancedProtectionSettings(
    content::WebContents* web_contents) const {
#if BUILDFLAG(IS_ANDROID)
  safe_browsing::ShowSafeBrowsingSettings(
      web_contents->GetTopLevelNativeWindow(),
      safe_browsing::SettingsAccessPoint::kSecurityInterstitial);
#else
  // In rare circumstances, this happens outside of a Browser, better ignore
  // than crash.
  // TODO(crbug.com/40772284): Remove and find a better way, e.g. not showing
  // the enhanced protection promo at all.
  if (!chrome::FindBrowserWithTab(web_contents)) {
    return;
  }
  chrome::ShowSafeBrowsingEnhancedProtection(
      chrome::FindBrowserWithTab(web_contents));
#endif
}

void ChromeSettingsPageHelper::OpenEnhancedProtectionSettingsWithIph(
    content::WebContents* web_contents,
    safe_browsing::SafeBrowsingSettingReferralMethod referral_method) const {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  // In rare circumstances, this happens outside of a Browser, better ignore
  // than crash.
  // TODO(crbug.com/40772284): Remove and find a better way, e.g. not showing
  // the enhanced protection promo at all.
  if (!chrome::FindBrowserWithTab(web_contents)) {
    return;
  }
  chrome::ShowSafeBrowsingEnhancedProtectionWithIph(
      chrome::FindBrowserWithTab(web_contents), referral_method);
#endif
}

}  // namespace security_interstitials
