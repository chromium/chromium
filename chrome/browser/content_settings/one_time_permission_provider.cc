// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/one_time_permission_provider.h"

#include <memory>
#include <set>

#include "base/power_monitor/power_monitor.h"
#include "base/ranges/algorithm.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/permissions/one_time_permissions_tracker.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_factory.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_observer.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/user_modifiable_provider.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_partition_key.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/permission_uma_util.h"
#include "components/permissions/permission_util.h"
#include "url/gurl.h"

OneTimePermissionProvider::OneTimePermissionProvider(
    OneTimePermissionsTracker* one_time_permissions_tracker)
    : one_time_permissions_tracker_(one_time_permissions_tracker),
      clock_(base::DefaultClock::GetInstance()) {
  one_time_permissions_tracker_->AddObserver(this);

  // The PowerMonitor is initialized in content_main_runner_impl.cc before the
  // main function for the browser process is run (which initializes the HCSM).
  // For this reason, the PowerMonitor is always initialized before the observer
  // is added here.
  base::PowerMonitor::GetInstance()->AddPowerSuspendObserver(this);
}

OneTimePermissionProvider::~OneTimePermissionProvider() {
  base::PowerMonitor::GetInstance()->RemovePowerSuspendObserver(this);
}

// TODO(b/307193732): handle the PartitionKey in all relevant methods, including
// when we call NotifyObservers().
std::unique_ptr<content_settings::RuleIterator>
OneTimePermissionProvider::GetRuleIterator(
    ContentSettingsType content_type,
    bool incognito,
    const content_settings::PartitionKey& partition_key) const {
  if (!permissions::PermissionUtil::DoesStoreTemporaryGrantsInHcsm(
          content_type)) {
    return nullptr;
  }
  return value_map_.GetRuleIterator(content_type);
}

std::unique_ptr<content_settings::Rule> OneTimePermissionProvider::GetRule(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    bool off_the_record,
    const content_settings::PartitionKey& partition_key) const {
  if (!permissions::PermissionUtil::DoesStoreTemporaryGrantsInHcsm(
          content_type)) {
    return nullptr;
  }

  base::AutoLock auto_lock(value_map_.GetLock());
  return value_map_.GetRule(primary_url, secondary_url, content_type);
}

bool OneTimePermissionProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_settings_type,
    base::Value&& value,
    const content_settings::ContentSettingConstraints& constraints,
    const content_settings::PartitionKey& partition_key) {
  if (!permissions::PermissionUtil::DoesStoreTemporaryGrantsInHcsm(
          content_settings_type)) {
    return false;
  }

  if (!content_settings::ContentSettingsRegistry::GetInstance()->Get(
          content_settings_type)) {
    // Object permissions cannot be mapped to a ContentSetting and thus cannot
    // be handled by this provider.
    return false;
  }

  auto content_setting = content_settings::ValueToContentSetting(value);

  // For transitions to block and resetting to default, clear the grant and let
  // the pref provider handle the permission as usual.
  if (content_setting == CONTENT_SETTING_DEFAULT ||
      content_setting == CONTENT_SETTING_BLOCK) {
    base::AutoLock lock(value_map_.GetLock());
    auto* previous_one_time_grant = value_map_.GetValue(
        primary_pattern.ToRepresentativeUrl(),
        secondary_pattern.ToRepresentativeUrl(), content_settings_type);

    if (!previous_one_time_grant) {
      // If there was no grant, it means that this content setting is
      // already being handled by the pref provider.
      return false;
    }

    value_map_.DeleteValue(primary_pattern, secondary_pattern,
                           content_settings_type);

    permissions::PermissionUmaUtil::RecordOneTimePermissionEvent(
        content_settings_type,
        permissions::OneTimePermissionEvent::REVOKED_MANUALLY);

    return false;
  }

  if (constraints.session_model() !=
      content_settings::mojom::SessionModel::ONE_TIME) {
    if (content_setting == CONTENT_SETTING_ALLOW) {
      // Transition from Allow once to Allow. Delete setting and let the pref
      // provider handle it.
      base::AutoLock lock(value_map_.GetLock());
      value_map_.DeleteValue(primary_pattern, secondary_pattern,
                             content_settings_type);
    }

    return false;
  }

  DCHECK_EQ(content_settings::ValueToContentSetting(value),
            CONTENT_SETTING_ALLOW);

  base::Time now = clock_->Now();
  content_settings::RuleMetaData metadata;
  metadata.set_session_model(content_settings::mojom::SessionModel::ONE_TIME);
  metadata.set_last_modified(now);
  if (base::FeatureList::IsEnabled(
          content_settings::features::kActiveContentSettingExpiry) &&
      !constraints.lifetime().is_zero()) {
    metadata.SetExpirationAndLifetime(now + constraints.lifetime(),
                                      constraints.lifetime());
  }

  permissions::PermissionUmaUtil::RecordOneTimePermissionEvent(
      content_settings_type,
      permissions::OneTimePermissionEvent::GRANTED_ONE_TIME);

  {
    base::AutoLock lock(value_map_.GetLock());
    value_map_.SetValue(primary_pattern, secondary_pattern,
                        content_settings_type, std::move(value), metadata);
  }

  NotifyObservers(primary_pattern, secondary_pattern, content_settings_type,
                  nullptr);

  // We need to handle transitions from Allow to Allow Once gracefully.
  // In that case we add the Allow Once setting in this provider, but also
  // have to clear the Allow setting in the pref provider. By returning false
  // here, we let the control flow trickle down to the pref provider.
  return false;
}

bool OneTimePermissionProvider::UpdateLastUsedTime(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const base::Time time,
    const content_settings::PartitionKey& partition_key) {
  // Last used time is not tracked for one-time permissions.
  return false;
}

bool OneTimePermissionProvider::ResetLastVisitTime(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const content_settings::PartitionKey& partition_key) {
  // LastVisit time is not currently tracked for one-time permissions.
  return false;
}

bool OneTimePermissionProvider::UpdateLastVisitTime(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const content_settings::PartitionKey& partition_key) {
  // LastVisit time is not tracked for one-time permissions.
  return false;
}

std::optional<base::TimeDelta> OneTimePermissionProvider::RenewContentSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType type,
    std::optional<ContentSetting> setting_to_match,
    const content_settings::PartitionKey& partition_key) {
  // Setting renewal is not supported for one-time permissions.
  return std::nullopt;
}

void OneTimePermissionProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type,
    const content_settings::PartitionKey& partition_key) {
  if (permissions::PermissionUtil::DoesStoreTemporaryGrantsInHcsm(
          content_type)) {
    return;
  }
  base::AutoLock lock(value_map_.GetLock());
  value_map_.DeleteValues(content_type);
}

void OneTimePermissionProvider::ShutdownOnUIThread() {
  RemoveAllObservers();
}

void OneTimePermissionProvider::SetClockForTesting(const base::Clock* clock) {
  clock_ = clock;
}

void OneTimePermissionProvider::ExpireWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_settings_type,
    const content_settings::PartitionKey& partition_key) {
  // Custom scope because NotifyObservers also requires value_map_'s exclusive
  // lock.
  {
    base::AutoLock lock(value_map_.GetLock());
    if (value_map_.GetValue(primary_pattern.ToRepresentativeUrl(),
                            secondary_pattern.ToRepresentativeUrl(),
                            content_settings_type) == nullptr) {
      return;
    }

    value_map_.DeleteValue(primary_pattern, secondary_pattern,
                           content_settings_type);
  }
  permissions::PermissionUmaUtil::RecordOneTimePermissionEvent(
      content_settings_type,
      permissions::OneTimePermissionEvent::EXPIRED_AFTER_MAXIMUM_LIFETIME);
  NotifyObservers(primary_pattern, secondary_pattern, content_settings_type,
                  /*partition_key=*/nullptr);
}

void OneTimePermissionProvider::OnSuspend() {
  std::vector<ContentSettingEntry> patterns_to_delete;
  auto* registry = content_settings::ContentSettingsRegistry::GetInstance();

  for (const auto* info : *registry) {
    auto setting_type = info->website_settings_info()->type();
    if (permissions::PermissionUtil::DoesStoreTemporaryGrantsInHcsm(
            setting_type)) {
      std::unique_ptr<content_settings::RuleIterator> rule_iterator(
          value_map_.GetRuleIterator(setting_type));

      while (rule_iterator && rule_iterator->HasNext()) {
        auto rule = rule_iterator->Next();
        patterns_to_delete.emplace_back(setting_type, rule->primary_pattern,
                                        rule->secondary_pattern);
        permissions::PermissionUmaUtil::RecordOneTimePermissionEvent(
            setting_type,
            permissions::OneTimePermissionEvent::EXPIRED_ON_SUSPEND);
      }
    }
  }

  DeleteEntriesAndNotify(patterns_to_delete);
}

// All tabs with the given origin have either been closed or navigated away
// from. We remove all permissions associated with the origin.
void OneTimePermissionProvider::OnLastPageFromOriginClosed(
    const url::Origin& origin) {
  for (auto setting_type :
       content_settings::GetTypesWithTemporaryGrantsInHcsm()) {
    DeleteEntriesMatchingGURL(
        setting_type, origin.GetURL(),
        permissions::OneTimePermissionEvent::ALL_TABS_CLOSED_OR_DISCARDED);
  }
}

// All tabs with the given origin have either been in the background for a
// certain time or not used for a certain time. This situation currently only
// expires geolocation. We remove the geolocation permission associated with
// the origin.
void OneTimePermissionProvider::OnAllTabsInBackgroundTimerExpired(
    const url::Origin& origin,
    const OneTimePermissionsTrackerObserver::BackgroundExpiryType&
        expiry_type) {
  switch (expiry_type) {
    case BackgroundExpiryType::kTimeout:
      DeleteEntriesMatchingGURL(
          ContentSettingsType::GEOLOCATION, origin.GetURL(),
          permissions::OneTimePermissionEvent::EXPIRED_IN_BACKGROUND);
      return;
    case BackgroundExpiryType::kLongTimeout:
      return;
  }
}

// All tabs to the origin have not shown a tab indicator for video for a
// certain time and have been in the background. We remove the camera
// permission associated with the origin.
void OneTimePermissionProvider::OnCapturingVideoExpired(
    const url::Origin& origin) {
  DeleteEntriesMatchingGURL(
      ContentSettingsType::MEDIASTREAM_CAMERA, origin.GetURL(),
      permissions::OneTimePermissionEvent::EXPIRED_IN_BACKGROUND);
}

// All tabs to the origin have not shown a tab indicator for microphone access
// for a certain time and have been in the background. We remove the
// microphone permission associated with the origin.
void OneTimePermissionProvider::OnCapturingAudioExpired(
    const url::Origin& origin) {
  DeleteEntriesMatchingGURL(
      ContentSettingsType::MEDIASTREAM_MIC, origin.GetURL(),
      permissions::OneTimePermissionEvent::EXPIRED_IN_BACKGROUND);
}

void OneTimePermissionProvider::DeleteEntriesAndNotify(
    const std::vector<OneTimePermissionProvider::ContentSettingEntry>&
        entries_to_delete) {
  {
    base::AutoLock lock(value_map_.GetLock());
    for (const auto& pattern : entries_to_delete) {
      value_map_.DeleteValue(pattern.primary_pattern, pattern.secondary_pattern,
                             pattern.type);

      // In all the observable `OneTimePermissionsTrackerObserver` events
      // which are emitted from the tracker, the tracker knows for which
      // origins and content settings it can halt bookkeeping. However, the
      // tracker isn't aware of externally deleted content settings. To
      // prevent it from triggering observers for an already deleted content
      // setting, we need to inform it about the deletion here (and only
      // here).
      one_time_permissions_tracker_->CleanupStateForExpiredContentSetting(
          pattern.type, pattern.primary_pattern, pattern.secondary_pattern);
    }
  }

  for (const auto& pattern : entries_to_delete) {
    NotifyObservers(pattern.primary_pattern, pattern.secondary_pattern,
                    pattern.type, /*partition_key=*/nullptr);
  }
}

void OneTimePermissionProvider::DeleteEntriesMatchingGURL(
    ContentSettingsType content_setting_type,
    const GURL& origin_gurl,
    permissions::OneTimePermissionEvent trigger_event) {
  std::vector<ContentSettingEntry> patterns_to_delete;
  std::unique_ptr<content_settings::RuleIterator> rule_iterator(
      value_map_.GetRuleIterator(content_setting_type));

  while (rule_iterator && rule_iterator->HasNext()) {
    auto rule = rule_iterator->Next();
    if (rule->primary_pattern.Matches(origin_gurl) &&
        rule->secondary_pattern.Matches(origin_gurl)) {
      patterns_to_delete.emplace_back(
          content_setting_type, rule->primary_pattern, rule->secondary_pattern);
      permissions::PermissionUmaUtil::RecordOneTimePermissionEvent(
          content_setting_type, trigger_event);
    }
  }

  rule_iterator.reset();

  DeleteEntriesAndNotify(patterns_to_delete);
}

void OneTimePermissionProvider::OnShutdown() {
  if (one_time_permissions_tracker_) {
    one_time_permissions_tracker_->RemoveObserver(this);
    one_time_permissions_tracker_ = nullptr;
  }
}
