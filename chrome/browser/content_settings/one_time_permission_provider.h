// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_ONE_TIME_PERMISSION_PROVIDER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_ONE_TIME_PERMISSION_PROVIDER_H_

#include <map>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_observer.h"
#include "components/content_settings/core/browser/content_settings_origin_identifier_value_map.h"
#include "components/content_settings/core/browser/user_modifiable_provider.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/permissions/permission_uma_util.h"

class OneTimePermissionsTracker;

// Stores one-time permission grants that expire when any of the following
// conditions is met:
// - All tabs of that origin have been closed or navigated away
// - All tabs of that origin have been discarded
// - All tabs of that origin have been backgrounded (without visible indicator)
//     for more than 5 minutes
// - 24 hours have elapsed since the one-time grant
// - The grant is manually revoked (via page info, settings, or a policy)
class OneTimePermissionProvider
    : public content_settings::UserModifiableProvider,
      OneTimePermissionsTrackerObserver {
 public:
  explicit OneTimePermissionProvider(
      OneTimePermissionsTracker* one_time_permissions_tracker);

  ~OneTimePermissionProvider() override;

  OneTimePermissionProvider(const OneTimePermissionProvider&) = delete;
  OneTimePermissionProvider& operator=(const OneTimePermissionProvider&) =
      delete;

  // UserModifiableProvider:
  std::unique_ptr<content_settings::RuleIterator> GetRuleIterator(
      ContentSettingsType content_type,
      bool incognito) const override;

  bool SetWebsiteSetting(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      base::Value&& value,
      const content_settings::ContentSettingConstraints& constraints) override;

  void ClearAllContentSettingsRules(ContentSettingsType content_type) override;

  void ShutdownOnUIThread() override;

  bool ResetLastVisitTime(const ContentSettingsPattern& primary_pattern,
                          const ContentSettingsPattern& secondary_pattern,
                          ContentSettingsType content_type) override;

  bool UpdateLastVisitTime(const ContentSettingsPattern& primary_pattern,
                           const ContentSettingsPattern& secondary_pattern,
                           ContentSettingsType content_type) override;
  void SetClockForTesting(base::Clock* clock) override;

  // OneTimePermissionsTrackerObserver:
  void OnLastPageFromOriginClosed(const url::Origin&) override;
  void OnAllTabsInBackgroundTimerExpired(const url::Origin&) override;
  void OnCapturingVideoExpired(const url::Origin&) override;
  void OnCapturingAudioExpired(const url::Origin&) override;

  void OnShutdown() override;

 private:
  // Deletes the matching values and records matching UMA events.
  void DeleteValuesMatchingGurl(
      ContentSettingsType content_setting_type,
      const GURL& origin_gurl,
      permissions::OneTimePermissionEvent trigger_event);

  content_settings::OriginIdentifierValueMap value_map_;
  raw_ptr<OneTimePermissionsTracker> one_time_permissions_tracker_ = nullptr;

  // Unowned
  raw_ptr<const base::Clock> clock_;
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_ONE_TIME_PERMISSION_PROVIDER_H_
