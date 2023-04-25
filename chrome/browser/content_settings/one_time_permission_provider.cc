// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/one_time_permission_provider.h"

#include <memory>
#include <set>

#include "base/ranges/algorithm.h"
#include "base/time/default_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/permissions/one_time_permissions_tracker.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_factory.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "url/gurl.h"

OneTimePermissionProvider::OneTimePermissionProvider(
    OneTimePermissionsTracker* one_time_permissions_tracker)
    : one_time_permissions_tracker_(one_time_permissions_tracker),
      clock_(base::DefaultClock::GetInstance()) {
  one_time_permissions_tracker_->AddObserver(this);
}

OneTimePermissionProvider::~OneTimePermissionProvider() = default;

std::unique_ptr<content_settings::RuleIterator>
OneTimePermissionProvider::GetRuleIterator(ContentSettingsType content_type,
                                           bool incognito) const {
  if (!permissions::PermissionUtil::CanPermissionBeAllowedOnce(content_type)) {
    return nullptr;
  }
  return value_map_.GetRuleIterator(content_type, &lock_);
}

bool OneTimePermissionProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_settings_type,
    base::Value&& value,
    const content_settings::ContentSettingConstraints& constraints) {
  if (!permissions::PermissionUtil::CanPermissionBeAllowedOnce(
          content_settings_type)) {
    return false;
  }

  // This block handles transitions from Allow Once to Ask/Block by clearing
  // the one time grant and letting the pref provider handle the permission as
  // usual.
  if (constraints.session_model != content_settings::SessionModel::OneTime) {
    value_map_.DeleteValue(primary_pattern, secondary_pattern,
                           content_settings_type);

    permissions::PermissionUmaUtil::RecordOneTimePermissionEvent(
        content_settings_type,
        permissions::OneTimePermissionEvent::REVOKED_MANUALLY);

    return false;
  }
  DCHECK_EQ(content_settings::ValueToContentSetting(value),
            CONTENT_SETTING_ALLOW);
  value_map_.SetValue(
      primary_pattern, secondary_pattern, content_settings_type,
      std::move(value),
      {
          .last_modified = clock_->Now(),
          .expiration = clock_->Now() + base::Days(1),
          .session_model = content_settings::SessionModel::OneTime,
      });

  permissions::PermissionUmaUtil::RecordOneTimePermissionEvent(
      content_settings_type,
      permissions::OneTimePermissionEvent::GRANTED_ONE_TIME);

  // We need to handle transitions from Allow to Allow Once gracefully.
  // In that case we add the Allow Once setting in this provider, but also
  // have to clear the Allow setting in the pref provider. By returning false
  // here, we let the control flow trickle down to the pref provider.
  return false;
}

bool OneTimePermissionProvider::ResetLastVisitTime(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  // LastVisit time is not currently tracked for one-time permissions.
  return false;
}

bool OneTimePermissionProvider::UpdateLastVisitTime(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  // LastVisit time is not tracked for one-time permissions.
  return false;
}

void OneTimePermissionProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {
  if (permissions::PermissionUtil::CanPermissionBeAllowedOnce(content_type)) {
    return;
  }
  value_map_.DeleteValues(content_type);
}

void OneTimePermissionProvider::ShutdownOnUIThread() {
  RemoveAllObservers();
}

void OneTimePermissionProvider::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

// All tabs with the given origin have either been closed or navigated away
// from. We remove all permissions associated with the origin.
void OneTimePermissionProvider::OnLastPageFromOriginClosed(
    const url::Origin& origin) {
  for (auto setting_type : {ContentSettingsType::GEOLOCATION,
                            ContentSettingsType::MEDIASTREAM_CAMERA,
                            ContentSettingsType::MEDIASTREAM_MIC}) {
    DeleteValuesMatchingGurl(
        setting_type, origin.GetURL(),
        permissions::OneTimePermissionEvent::ALL_TABS_CLOSED_OR_DISCARDED);
  }
}

// All tabs with the given origin have either been in the background for a
// certain time or not used for a certain time. This situation currently only
// expires geolocation. We remove the geolocation permission associated with the
// origin.
void OneTimePermissionProvider::OnAllTabsInBackgroundTimerExpired(
    const url::Origin& origin) {
  DeleteValuesMatchingGurl(
      ContentSettingsType::GEOLOCATION, origin.GetURL(),
      permissions::OneTimePermissionEvent::EXPIRED_IN_BACKGROUND);
}

// All tabs to the origin have not shown a tab indicator for video for a certain
// time and have been in the background. We remove the camera permission
// associated with the origin.
void OneTimePermissionProvider::OnCapturingVideoExpired(
    const url::Origin& origin) {
  DeleteValuesMatchingGurl(
      ContentSettingsType::MEDIASTREAM_CAMERA, origin.GetURL(),
      permissions::OneTimePermissionEvent::EXPIRED_IN_BACKGROUND);
}

// All tabs to the origin have not shown a tab indicator for microphone access
// for a certain time and have been in the background. We remove the microphone
// permission associated with the origin.
void OneTimePermissionProvider::OnCapturingAudioExpired(
    const url::Origin& origin) {
  DeleteValuesMatchingGurl(
      ContentSettingsType::MEDIASTREAM_MIC, origin.GetURL(),
      permissions::OneTimePermissionEvent::EXPIRED_IN_BACKGROUND);
}

void OneTimePermissionProvider::DeleteValuesMatchingGurl(
    ContentSettingsType content_setting_type,
    const GURL& origin_gurl,
    permissions::OneTimePermissionEvent trigger_event) {
  std::set<content_settings::OriginIdentifierValueMap::PatternPair>
      patterns_to_delete;
  std::unique_ptr<content_settings::RuleIterator> rule_iterator(
      value_map_.GetRuleIterator(content_setting_type, nullptr));

  while (rule_iterator && rule_iterator->HasNext()) {
    auto rule = rule_iterator->Next();
    if (rule.primary_pattern.Matches(origin_gurl) &&
        rule.secondary_pattern.Matches(origin_gurl)) {
      patterns_to_delete.insert({rule.primary_pattern, rule.secondary_pattern});
      if (rule.metadata.expiration >= clock_->Now()) {
        permissions::PermissionUmaUtil::RecordOneTimePermissionEvent(
            content_setting_type, trigger_event);
      }
    }
  }
  rule_iterator.reset();

  for (const auto& pattern : patterns_to_delete) {
    value_map_.DeleteValue(pattern.primary_pattern, pattern.secondary_pattern,
                           content_setting_type);
  }
}

void OneTimePermissionProvider::OnShutdown() {
  if (one_time_permissions_tracker_) {
    one_time_permissions_tracker_->RemoveObserver(this);
    one_time_permissions_tracker_ = nullptr;
  }
}
