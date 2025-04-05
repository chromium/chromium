// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_JAVASCRIPT_OPTIMIZER_PROVIDER_ANDROID_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_JAVASCRIPT_OPTIMIZER_PROVIDER_ANDROID_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "components/content_settings/core/browser/content_settings_observable_provider.h"

class Profile;

// Provides ContentSettingsType::JAVASCRIPT_OPTIMIZER default from OS.
class JavascriptOptimizerProviderAndroid
    : public content_settings::ObservableProvider,
      public safe_browsing::AdvancedProtectionStatusManager::
          StatusChangedObserver {
 public:
  JavascriptOptimizerProviderAndroid(Profile* profile,
                                     bool should_record_metrics);
  JavascriptOptimizerProviderAndroid(
      safe_browsing::AdvancedProtectionStatusManager*
          advanced_protection_manager,
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
  void OnAdvancedProtectionStatusChanged(bool enrolled) override;

  void RecordHistogramMetrics();

  raw_ptr<safe_browsing::AdvancedProtectionStatusManager>
      advanced_protection_manager_;
  bool is_under_advanced_protection_;
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_JAVASCRIPT_OPTIMIZER_PROVIDER_ANDROID_H_
