// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/javascript_optimizer_provider_android.h"

#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "components/content_settings/core/browser/single_value_wildcard_rule_iterator.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/permissions/android/os_additional_security_permission_util_android.h"

JavascriptOptimizerProviderAndroid::JavascriptOptimizerProviderAndroid(
    bool should_record_metrics)
    : has_permission_callback_(
          base::BindRepeating(&permissions::HasJavascriptOptimizerPermission)) {
  if (should_record_metrics) {
    RecordHistogramMetrics();
  }
}

JavascriptOptimizerProviderAndroid::JavascriptOptimizerProviderAndroid(
    CheckPermissionCallback callback,
    bool should_record_metrics)
    : has_permission_callback_(std::move(callback)) {
  if (should_record_metrics) {
    RecordHistogramMetrics();
  }
}

JavascriptOptimizerProviderAndroid::~JavascriptOptimizerProviderAndroid() {}

std::unique_ptr<content_settings::RuleIterator>
JavascriptOptimizerProviderAndroid::GetRuleIterator(
    ContentSettingsType content_type,
    bool off_the_record,
    const content_settings::PartitionKey& partition_key) const {
  if (content_type != ContentSettingsType::JAVASCRIPT_OPTIMIZER) {
    return nullptr;
  }
  return QueryHasPermission()
             ? nullptr
             : std::make_unique<
                   content_settings::SingleValueWildcardRuleIterator>(
                   base::Value(CONTENT_SETTING_BLOCK));
}

std::unique_ptr<content_settings::Rule>
JavascriptOptimizerProviderAndroid::GetRule(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    bool off_the_record,
    const content_settings::PartitionKey& partition_key) const {
  if (content_type != ContentSettingsType::JAVASCRIPT_OPTIMIZER) {
    return nullptr;
  }
  return QueryHasPermission() ? nullptr
                              : std::make_unique<content_settings::Rule>(
                                    ContentSettingsPattern::Wildcard(),
                                    ContentSettingsPattern::Wildcard(),
                                    base::Value(CONTENT_SETTING_BLOCK),
                                    content_settings::RuleMetaData{});
}

bool JavascriptOptimizerProviderAndroid::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    base::Value&& value,
    const content_settings::ContentSettingConstraints& constraints,
    const content_settings::PartitionKey& partition_key) {
  return false;
}

void JavascriptOptimizerProviderAndroid::ClearAllContentSettingsRules(
    ContentSettingsType content_type,
    const content_settings::PartitionKey& partition_key) {}

void JavascriptOptimizerProviderAndroid::ShutdownOnUIThread() {
  CHECK(CalledOnValidThread());
  RemoveAllObservers();
  has_permission_callback_.Reset();
}

bool JavascriptOptimizerProviderAndroid::QueryHasPermission() const {
  return !has_permission_callback_ || has_permission_callback_.Run();
}

void JavascriptOptimizerProviderAndroid::RecordHistogramMetrics() {
  base::UmaHistogramBoolean(
      "ContentSettings.RegularProfile.DefaultJavascriptOptimizationBlockedByOs",
      !QueryHasPermission());
}
