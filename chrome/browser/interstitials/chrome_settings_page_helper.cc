// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/interstitials/chrome_settings_page_helper.h"
#include "build/build_config.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/safe_browsing/android/safe_browsing_settings_launcher_android.h"
#else
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/user_education/open_page_and_show_help_bubble.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/safe_browsing/core/common/safebrowsing_referral_methods.h"
#include "ui/base/l10n/l10n_util.h"
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
      web_contents, safe_browsing::SettingsAccessPoint::kSecurityInterstitial);
#else
  // In rare circumstances, this happens outside of a Browser, better ignore
  // than crash.
  // TODO(crbug.com/1219535): Remove and find a better way, e.g. not showing the
  // enhanced protection promo at all.
  if (!chrome::FindBrowserWithWebContents(web_contents))
    return;
  chrome::ShowSafeBrowsingEnhancedProtection(
      chrome::FindBrowserWithWebContents(web_contents));
#endif
}

void ChromeSettingsPageHelper::OpenEnhancedProtectionSettingsWithIph(
    content::WebContents* web_contents) const {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  OpenPageAndShowHelpBubble::Params params;
  params.target_url =
      chrome::GetSettingsUrl(chrome::kSafeBrowsingEnhancedProtectionSubPage);
  params.bubble_anchor_id = kEnhancedProtectionSettingElementId;
  params.bubble_arrow = user_education::HelpBubbleArrow::kBottomLeft;
  params.bubble_text = l10n_util::GetStringUTF16(
      IDS_SETTINGS_SAFEBROWSING_ENHANCED_IPH_BUBBLE_TEXT);

  // In rare circumstances, this happens outside of a Browser, better ignore
  // than crash.
  // TODO(crbug.com/1219535): Remove and find a better way, e.g. not showing the
  // enhanced protection promo at all.
  if (!chrome::FindBrowserWithWebContents(web_contents)) {
    return;
  }
  base::UmaHistogramEnumeration(
      "SafeBrowsing.EsbPromotionFlow.IphShown",
      SafeBrowsingSettingReferralMethod::kSecurityInterstitial);
  OpenPageAndShowHelpBubble::Start(
      chrome::FindBrowserWithWebContents(web_contents), std::move(params));
#endif
}

}  // namespace security_interstitials
