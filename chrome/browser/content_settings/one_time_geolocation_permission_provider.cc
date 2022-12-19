// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "one_time_geolocation_permission_provider.h"

#include "chrome/browser/permissions/last_tab_standing_tracker.h"
#include "chrome/browser/permissions/last_tab_standing_tracker_factory.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "url/gurl.h"

namespace {

class OneTimeRuleIterator : public content_settings::RuleIterator {
 public:
  using PatternToGrantTimeMap =
      OneTimeGeolocationPermissionProvider::PatternToGrantTimeMap;

  explicit OneTimeRuleIterator(
      const PatternToGrantTimeMap& pattern_to_grant_time_map)
      : begin_iterator_(pattern_to_grant_time_map.begin()),
        end_iterator_(pattern_to_grant_time_map.end()) {}

  ~OneTimeRuleIterator() override = default;

  bool HasNext() const override { return begin_iterator_ != end_iterator_; }

  content_settings::Rule Next() override {
    content_settings::Rule rule(
        begin_iterator_->first, ContentSettingsPattern::Wildcard(),
        content_settings::ContentSettingToValue(CONTENT_SETTING_ALLOW),
        {
            .last_modified = begin_iterator_->second,
            .expiration = begin_iterator_->second + base::Days(1),
            .session_model = content_settings::SessionModel::OneTime,
        });
    begin_iterator_++;
    return rule;
  }

 private:
  PatternToGrantTimeMap::const_iterator begin_iterator_;
  const PatternToGrantTimeMap::const_iterator end_iterator_;
};

}  // namespace

OneTimeGeolocationPermissionProvider::OneTimeGeolocationPermissionProvider(
    content::BrowserContext* browser_context) {
  last_tab_standing_tracker_ =
      LastTabStandingTrackerFactory::GetForBrowserContext(browser_context);
  last_tab_standing_tracker_->AddObserver(this);
}

OneTimeGeolocationPermissionProvider::~OneTimeGeolocationPermissionProvider() {
  if (last_tab_standing_tracker_)
    last_tab_standing_tracker_->RemoveObserver(this);
}

std::unique_ptr<content_settings::RuleIterator>
OneTimeGeolocationPermissionProvider::GetRuleIterator(
    ContentSettingsType content_type,
    bool incognito) const {
  if (content_type != ContentSettingsType::GEOLOCATION)
    return nullptr;
  return std::make_unique<OneTimeRuleIterator>(grants_with_open_tabs_);
}

bool OneTimeGeolocationPermissionProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_settings_type,
    base::Value&& value,
    const content_settings::ContentSettingConstraints& constraints) {
  if (content_settings_type != ContentSettingsType::GEOLOCATION)
    return false;
  // This block handles transitions from Allow Once to Ask/Block by clearing
  // the one time grant and letting the pref provider handle the permission as
  // usual.
  if (constraints.session_model != content_settings::SessionModel::OneTime) {
    auto matching_iterator = grants_with_open_tabs_.find(primary_pattern);
    if (matching_iterator != grants_with_open_tabs_.end())
      grants_with_open_tabs_.erase(matching_iterator);
    return false;
  }
  DCHECK_EQ(content_settings::ValueToContentSetting(value),
            CONTENT_SETTING_ALLOW);
  grants_with_open_tabs_[primary_pattern] = base::Time::Now();
  // We need to handle transitions from Allow to Allow Once gracefully.
  // In that case we add the Allow Once setting in this provider, but also
  // have to clear the Allow setting in the pref provider. By returning false
  // here, we let the control flow trickle down to the pref provider.
  return false;
}

bool OneTimeGeolocationPermissionProvider::ResetLastVisitTime(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  // LastVisit time is not tracked for one-time permissions.
  return false;
}

bool OneTimeGeolocationPermissionProvider::UpdateLastVisitTime(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  // LastVisit time is not tracked for one-time permissions.
  return false;
}

void OneTimeGeolocationPermissionProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {
  if (content_type == ContentSettingsType::GEOLOCATION)
    return;
  grants_with_open_tabs_.clear();
}

void OneTimeGeolocationPermissionProvider::ShutdownOnUIThread() {
  RemoveAllObservers();
}

void OneTimeGeolocationPermissionProvider::SetClockForTesting(
    base::Clock* clock) {
  NOTREACHED();
}

// All pages with the given origin have either been closed or navigated away
// from. We remove all permissions associated with the origin.
void OneTimeGeolocationPermissionProvider::OnLastPageFromOriginClosed(
    const url::Origin& origin) {
  for (auto pattern_and_grant_time : grants_with_open_tabs_) {
    if (pattern_and_grant_time.first.Matches(origin.GetURL())) {
      grants_with_open_tabs_.erase(pattern_and_grant_time.first);
      break;
    }
  }
}

void OneTimeGeolocationPermissionProvider::OnShutdown() {
  last_tab_standing_tracker_ = nullptr;
}
