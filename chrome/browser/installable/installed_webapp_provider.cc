// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/installable/installed_webapp_provider.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/installable/installed_webapp_bridge.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "url/gurl.h"

using content_settings::ResourceIdentifier;
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
        ContentSettingsPattern::Wildcard(), base::Value(setting));
  }

 private:
  size_t index_ = 0;
  InstalledWebappProvider::RuleList rules_;

  DISALLOW_COPY_AND_ASSIGN(InstalledWebappIterator);
};

}  // namespace

InstalledWebappProvider::InstalledWebappProvider() {
  InstalledWebappBridge::SetProviderInstance(this);
}
InstalledWebappProvider::~InstalledWebappProvider() {
  InstalledWebappBridge::SetProviderInstance(nullptr);
}

std::unique_ptr<RuleIterator> InstalledWebappProvider::GetRuleIterator(
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    bool incognito) const {
  if (content_type != ContentSettingsType::NOTIFICATIONS || incognito) {
    return nullptr;
  }

  return std::make_unique<InstalledWebappIterator>(
      InstalledWebappBridge::GetInstalledWebappNotificationPermissions());
}

bool InstalledWebappProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const ResourceIdentifier& resource_identifier,
    std::unique_ptr<base::Value>&& value) {
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

void InstalledWebappProvider::Notify() {
  NotifyObservers(ContentSettingsPattern(), ContentSettingsPattern(),
                  ContentSettingsType::NOTIFICATIONS, std::string());
}
