// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fingerprinting_protection/chrome_fingerprinting_protection_web_contents_helper_factory.h"

#include "chrome/browser/browser_process.h"
#include "components/fingerprinting_protection_filter/browser/fingerprinting_protection_web_contents_helper.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tracking_protection_settings.h"
#include "components/subresource_filter/content/shared/browser/ruleset_service.h"
#include "content/public/browser/web_contents.h"

void CreateFingerprintingProtectionWebContentsHelper(
    content::WebContents* web_contents,
    PrefService* pref_service,
    privacy_sandbox::TrackingProtectionSettings* tracking_protection_settings,
    bool is_incognito) {
  subresource_filter::RulesetService* ruleset_service =
      g_browser_process->fingerprinting_protection_ruleset_service();
  subresource_filter::VerifiedRulesetDealer::Handle* dealer =
      ruleset_service ? ruleset_service->GetRulesetDealer() : nullptr;
  fingerprinting_protection_filter::FingerprintingProtectionWebContentsHelper::
      CreateForWebContents(web_contents, pref_service,
                           tracking_protection_settings, dealer, is_incognito);
}
