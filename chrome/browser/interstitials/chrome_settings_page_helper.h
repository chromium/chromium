// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTERSTITIALS_CHROME_SETTINGS_PAGE_HELPER_H_
#define CHROME_BROWSER_INTERSTITIALS_CHROME_SETTINGS_PAGE_HELPER_H_

#include <memory>

#include "components/safe_browsing/core/common/safebrowsing_referral_methods.h"
#include "components/security_interstitials/content/settings_page_helper.h"

namespace content {
class WebContents;
}

namespace security_interstitials {

// This class is used to open a Chrome setting page in a security interstitial.
// The implementation is different on desktop platforms and on Android. On
// desktop, it opens a new tab and navigation to chrome://settings/*.
// TODO(crbug.com/40720989): On Android, it creates an intent to launch a
// Settings activity.
class ChromeSettingsPageHelper : public SettingsPageHelper {
 public:
  static std::unique_ptr<security_interstitials::SettingsPageHelper>
  CreateChromeSettingsPageHelper();

  ChromeSettingsPageHelper() = default;
  ~ChromeSettingsPageHelper() override = default;
  ChromeSettingsPageHelper(const ChromeSettingsPageHelper&) = delete;
  ChromeSettingsPageHelper& operator=(const ChromeSettingsPageHelper&) = delete;

  // SettingsPageHelper::
  void OpenEnhancedProtectionSettings(
      content::WebContents* web_contents) const override;
  void OpenEnhancedProtectionSettingsWithIph(
      content::WebContents* web_contents,
      safe_browsing::SafeBrowsingSettingReferralMethod referral_method)
      const override;
};

}  // namespace security_interstitials

#endif  // CHROME_BROWSER_INTERSTITIALS_CHROME_SETTINGS_PAGE_HELPER_H_
