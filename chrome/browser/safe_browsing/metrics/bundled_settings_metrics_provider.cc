// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/metrics/bundled_settings_metrics_provider.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/site_protection/site_familiarity_utils.h"
#include "components/content_settings/browser/ui/javascript_optimizer_setting.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace safe_browsing {

BundledSettingsMetricsProvider::BundledSettingsMetricsProvider() = default;
BundledSettingsMetricsProvider::~BundledSettingsMetricsProvider() = default;

void BundledSettingsMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  Profile* profile = cached_profile_.GetMetricsProfile();
  if (!profile) {
    return;
  }

  PrefService* prefs = profile->GetPrefs();
  SecuritySettingsBundleSetting bundle = GetSecurityBundleSetting(*prefs);
  bool is_using_enhanced_bundle =
      (bundle == SecuritySettingsBundleSetting::ENHANCED);
  base::UmaHistogramBoolean("Security.EnhancedBundle.IsEnhancedSelected",
                            is_using_enhanced_bundle);

  // LINT.IfChange
  bool is_default_safe_browsing_state =
      GetDefaultSafeBrowsingState(bundle) == GetSafeBrowsingState(*prefs);
  base::UmaHistogramBoolean(
      is_using_enhanced_bundle
          ? "Security.EnhancedBundle.SafeBrowsingSetting.WasModifiedFromDefault"
          : "Security.StandardBundle.SafeBrowsingSetting."
            "WasModifiedFromDefault",
      !is_default_safe_browsing_state);

  // TODO(crbug.com/490481910): Fix dependency cycle when using
  // GetDefaultJsOptimizerSetting from content_settings.
  content_settings::JavascriptOptimizerSetting default_js_setting =
      (bundle == SecuritySettingsBundleSetting::ENHANCED)
          ? content_settings::JavascriptOptimizerSetting::
                kBlockedForUnfamiliarSites
          : content_settings::JavascriptOptimizerSetting::kAllowed;
  bool is_default_js_optimizer_state =
      default_js_setting ==
      site_protection::ComputeDefaultJavascriptOptimizerSetting(profile);
  base::UmaHistogramBoolean(
      is_using_enhanced_bundle
          ? kEnhancedJavascriptOptimizerWasModifiedHistogram
          : kStandardJavascriptOptimizerWasModifiedHistogram,
      !is_default_js_optimizer_state);
  // LINT.ThenChange(//chrome/browser/resources/settings/privacy_page/security/security_page_v2.ts,//chrome/browser/safe_browsing/safe_browsing_service.cc)
}

}  // namespace safe_browsing
