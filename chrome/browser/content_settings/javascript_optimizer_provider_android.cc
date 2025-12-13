// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/javascript_optimizer_provider_android.h"

#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager_factory.h"
#include "components/content_settings/core/browser/single_value_wildcard_rule_iterator.h"
#include "components/content_settings/core/common/content_settings.h"

using safe_browsing::AdvancedProtectionStatusManager;
using safe_browsing::AdvancedProtectionStatusManagerFactory;

JavascriptOptimizerProviderAndroid::JavascriptOptimizerProviderAndroid(
    Profile* profile,
    bool should_record_metrics)
    : advanced_protection_manager_(
          AdvancedProtectionStatusManagerFactory::GetForProfile(profile)),
      is_under_advanced_protection_(
          advanced_protection_manager_->IsUnderAdvancedProtection()) {
  advanced_protection_manager_->AddObserver(this);
  if (should_record_metrics) {
    RecordHistogramMetrics();
  }
}

JavascriptOptimizerProviderAndroid::JavascriptOptimizerProviderAndroid(
    safe_browsing::AdvancedProtectionStatusManager* advanced_protection_manager,
    bool should_record_metrics)
    : advanced_protection_manager_(advanced_protection_manager),
      is_under_advanced_protection_(
          advanced_protection_manager_->IsUnderAdvancedProtection()) {
  advanced_protection_manager_->AddObserver(this);
  if (should_record_metrics) {
    RecordHistogramMetrics();
  }
}

JavascriptOptimizerProviderAndroid::~JavascriptOptimizerProviderAndroid() =
    default;

std::unique_ptr<content_settings::RuleIterator>
JavascriptOptimizerProviderAndroid::GetRuleIterator(
    ContentSettingsType content_type,
    bool off_the_record) const {
  if (content_type != ContentSettingsType::JAVASCRIPT_OPTIMIZER) {
    return nullptr;
  }
  return is_under_advanced_protection_
             ? std::make_unique<
                   content_settings::SingleValueWildcardRuleIterator>(
                   base::Value(CONTENT_SETTING_BLOCK))
             : nullptr;
}

std::unique_ptr<content_settings::Rule>
JavascriptOptimizerProviderAndroid::GetRule(const GURL& primary_url,
                                            const GURL& secondary_url,
                                            ContentSettingsType content_type,
                                            bool off_the_record) const {
  if (content_type != ContentSettingsType::JAVASCRIPT_OPTIMIZER) {
    return nullptr;
  }
  return is_under_advanced_protection_
             ? std::make_unique<content_settings::Rule>(
                   ContentSettingsPattern::Wildcard(),
                   ContentSettingsPattern::Wildcard(),
                   base::Value(CONTENT_SETTING_BLOCK),
                   content_settings::RuleMetaData{})
             : nullptr;
}

bool JavascriptOptimizerProviderAndroid::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    base::Value&& value,
    const content_settings::ContentSettingConstraints& constraints) {
  return false;
}

void JavascriptOptimizerProviderAndroid::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {}

void JavascriptOptimizerProviderAndroid::ShutdownOnUIThread() {
  CHECK(CalledOnValidThread());
  RemoveAllObservers();
  if (advanced_protection_manager_) {
    advanced_protection_manager_->RemoveObserver(this);
    advanced_protection_manager_ = nullptr;
  }
}

void JavascriptOptimizerProviderAndroid::OnAdvancedProtectionStatusChanged(
    bool enrolled) {
  is_under_advanced_protection_ = enrolled;
  NotifyObservers(ContentSettingsPattern::Wildcard(),
                  ContentSettingsPattern::Wildcard(),
                  ContentSettingsType::JAVASCRIPT_OPTIMIZER);
}

void JavascriptOptimizerProviderAndroid::RecordHistogramMetrics() {
  base::UmaHistogramBoolean(
      "ContentSettings.RegularProfile.DefaultJavascriptOptimizationBlockedByOs",
      is_under_advanced_protection_);
}
