// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_JAVASCRIPT_OPTIMIZER_PROVIDER_ANDROID_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_JAVASCRIPT_OPTIMIZER_PROVIDER_ANDROID_H_

#include "base/functional/callback_forward.h"
#include "components/content_settings/core/browser/content_settings_observable_provider.h"

// Provides ContentSettingsType::JAVASCRIPT_OPTIMIZER default from OS.
class JavascriptOptimizerProviderAndroid
    : public content_settings::ObservableProvider {
 public:
  // Callback returns whether the OS has granted permission to use the
  // Javascript Optimizer.
  typedef base::RepeatingCallback<bool()> CheckPermissionCallback;

  explicit JavascriptOptimizerProviderAndroid(bool should_record_metrics);

  // The callback must be thread-safe.
  JavascriptOptimizerProviderAndroid(CheckPermissionCallback,
                                     bool should_record_metrics);

  JavascriptOptimizerProviderAndroid(
      const JavascriptOptimizerProviderAndroid&) = delete;
  JavascriptOptimizerProviderAndroid& operator=(
      const JavascriptOptimizerProviderAndroid&) = delete;

  ~JavascriptOptimizerProviderAndroid() override;

  // ProviderInterface:
  std::unique_ptr<content_settings::RuleIterator> GetRuleIterator(
      ContentSettingsType content_type,
      bool off_the_record,
      const content_settings::PartitionKey& partition_key) const override;
  std::unique_ptr<content_settings::Rule> GetRule(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      bool off_the_record,
      const content_settings::PartitionKey& partition_key) const override;

  bool SetWebsiteSetting(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      base::Value&& value,
      const content_settings::ContentSettingConstraints& constraints,
      const content_settings::PartitionKey& partition_key) override;

  void ClearAllContentSettingsRules(
      ContentSettingsType content_type,
      const content_settings::PartitionKey& partition_key) override;

  void ShutdownOnUIThread() override;

 private:
  bool QueryHasPermission() const;

  void RecordHistogramMetrics();

  // Thread-safe.
  CheckPermissionCallback has_permission_callback_;
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_JAVASCRIPT_OPTIMIZER_PROVIDER_ANDROID_H_
