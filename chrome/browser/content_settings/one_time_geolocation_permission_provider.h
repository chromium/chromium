// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_ONE_TIME_GEOLOCATION_PERMISSION_PROVIDER_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_ONE_TIME_GEOLOCATION_PERMISSION_PROVIDER_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/permissions/last_tab_standing_tracker_observer.h"
#include "components/content_settings/core/browser/user_modifiable_provider.h"
#include "components/content_settings/core/common/content_settings_pattern.h"

namespace content {
class BrowserContext;
}
class LastTabStandingTracker;

// Stores one-time per-origin geolocation permissions grants that expire as
// soon as the last tab from an origin is closed but after one day at the
// latest.
// TODO(crbug.com/1226750): Use PermissionUtil::CanPermissionBeAllowedOnce to
// decide which permissions can have one-time grants.
class OneTimeGeolocationPermissionProvider
    : public content_settings::UserModifiableProvider,
      LastTabStandingTrackerObserver {
 public:
  using PatternToGrantTimeMap = std::map<ContentSettingsPattern, base::Time>;
  explicit OneTimeGeolocationPermissionProvider(
      content::BrowserContext* browser_context);

  ~OneTimeGeolocationPermissionProvider() override;

  OneTimeGeolocationPermissionProvider(
      const OneTimeGeolocationPermissionProvider&) = delete;
  OneTimeGeolocationPermissionProvider& operator=(
      const OneTimeGeolocationPermissionProvider&) = delete;

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

  // LastTabStandingTrackerObserver:
  void OnLastPageFromOriginClosed(const url::Origin&) override;

  void OnShutdown() override;

 private:
  PatternToGrantTimeMap grants_with_open_tabs_;
  raw_ptr<LastTabStandingTracker> last_tab_standing_tracker_ = nullptr;
};

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_ONE_TIME_GEOLOCATION_PERMISSION_PROVIDER_H_
