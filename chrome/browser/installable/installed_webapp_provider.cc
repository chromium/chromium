// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/installable/installed_webapp_provider.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/installable/installed_webapp_bridge.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "url/gurl.h"

using content_settings::RuleIterator;

namespace {

class InstalledWebappIterator : public content_settings::RuleIterator {
 public:
  explicit InstalledWebappIterator(InstalledWebappProvider::RuleList rules)
      : rules_(std::move(rules)) {}
  ~InstalledWebappIterator() override = default;

  bool HasNext() const override { return index_ < rules_.size(); }

  content_settings::Rule Next() override {
    DCHECK(HasNext());
    const GURL& origin = rules_[index_].first;
    ContentSetting setting = rules_[index_].second;
    index_++;

    return content_settings::Rule(
        ContentSettingsPattern::FromURLNoWildcard(origin),
        ContentSettingsPattern::Wildcard(), base::Value(setting), base::Time(),
        content_settings::SessionModel::Durable);
  }

 private:
  size_t index_ = 0;
  InstalledWebappProvider::RuleList rules_;

  DISALLOW_COPY_AND_ASSIGN(InstalledWebappIterator);
};

bool IsSupportedContentType(ContentSettingsType content_type) {
  switch (content_type) {
    case ContentSettingsType::NOTIFICATIONS:
      return true;
    case ContentSettingsType::GEOLOCATION:
      return base::FeatureList::IsEnabled(
          chrome::android::kTrustedWebActivityLocationDelegation);
    default:
      return false;
  }
}

}  // namespace

InstalledWebappProvider::InstalledWebappProvider() {
  InstalledWebappBridge::SetProviderInstance(this);
}
InstalledWebappProvider::~InstalledWebappProvider() {
  InstalledWebappBridge::SetProviderInstance(nullptr);
}

std::unique_ptr<RuleIterator> InstalledWebappProvider::GetRuleIterator(
    ContentSettingsType content_type,
    bool incognito) const {
  if (incognito)
    return nullptr;

  if (IsSupportedContentType(content_type)) {
    return std::make_unique<InstalledWebappIterator>(
        InstalledWebappBridge::GetInstalledWebappPermissions(content_type));
  }
  return nullptr;
}

bool InstalledWebappProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    std::unique_ptr<base::Value>&& value,
    const content_settings::ContentSettingConstraints& constraints) {
  // You can't set settings through this provider.
  return false;
}

void InstalledWebappProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {
  // You can't set settings through this provider.
}

void InstalledWebappProvider::ShutdownOnUIThread() {
  DCHECK(CalledOnValidThread());
  RemoveAllObservers();
}

void InstalledWebappProvider::Notify(ContentSettingsType content_type) {
  NotifyObservers(ContentSettingsPattern(), ContentSettingsPattern(),
                  content_type);
}
