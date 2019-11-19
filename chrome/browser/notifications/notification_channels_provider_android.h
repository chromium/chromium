// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_CHANNELS_PROVIDER_ANDROID_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_CHANNELS_PROVIDER_ANDROID_H_

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/user_modifiable_provider.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

class TemplateURLService;

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.notifications
enum NotificationChannelStatus { ENABLED, BLOCKED, UNAVAILABLE };

struct NotificationChannel {
  NotificationChannel(const std::string& id,
                      const std::string& origin,
                      const base::Time& timestamp,
                      NotificationChannelStatus status);
  NotificationChannel(const NotificationChannel& other);
  bool operator==(const NotificationChannel& other) const {
    return origin == other.origin && status == other.status;
  }
  const std::string id;
  const std::string origin;
  const base::Time timestamp;
  NotificationChannelStatus status = NotificationChannelStatus::UNAVAILABLE;
};

// This class provides notification content settings from system notification
// channels on Android O+. This provider takes precedence over pref-provided
// content settings, but defers to supervised user and policy settings - see
// ordering of the ProviderType enum values in HostContentSettingsMap.
class NotificationChannelsProviderAndroid
    : public content_settings::UserModifiableProvider {
 public:
  // Helper class to make the JNI calls.
  class NotificationChannelsBridge {
   public:
    virtual ~NotificationChannelsBridge() = default;
    virtual bool ShouldUseChannelSettings() = 0;
    virtual NotificationChannel CreateChannel(const std::string& origin,
                                              const base::Time& timestamp,
                                              bool enabled) = 0;
    virtual NotificationChannelStatus GetChannelStatus(
        const std::string& origin) = 0;
    virtual void DeleteChannel(const std::string& origin) = 0;
    virtual std::vector<NotificationChannel> GetChannels() = 0;
  };

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  NotificationChannelsProviderAndroid();
  ~NotificationChannelsProviderAndroid() override;

  // Migrates any notification settings from the passed-in provider to
  // channels, unless they were already migrated or channels should not be used.
  void MigrateToChannelsIfNecessary(
      PrefService* prefs,
      content_settings::ProviderInterface* pref_provider);

  // Deletes any existing blocked site channels, unless this one-off deletion
  // already occurred. See https://crbug.com/835232.
  void ClearBlockedChannelsIfNecessary(
      PrefService* prefs,
      TemplateURLService* template_url_service);

  // UserModifiableProvider methods.
  std::unique_ptr<content_settings::RuleIterator> GetRuleIterator(
      ContentSettingsType content_type,
      const content_settings::ResourceIdentifier& resource_identifier,
      bool incognito) const override;
  bool SetWebsiteSetting(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const content_settings::ResourceIdentifier& resource_identifier,
      std::unique_ptr<base::Value>&& value) override;
  void ClearAllContentSettingsRules(ContentSettingsType content_type) override;
  void ShutdownOnUIThread() override;
  base::Time GetWebsiteSettingLastModified(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const content_settings::ResourceIdentifier& resource_identifier) override;

 private:
  explicit NotificationChannelsProviderAndroid(
      std::unique_ptr<NotificationChannelsBridge> bridge,
      std::unique_ptr<base::Clock> clock);
  friend class NotificationChannelsProviderAndroidTest;

  std::vector<NotificationChannel> UpdateCachedChannels() const;

  void CreateChannelIfRequired(const std::string& origin_string,
                               NotificationChannelStatus new_channel_status);

  void CreateChannelForRule(const content_settings::Rule& rule);

  void InitCachedChannels();

  std::unique_ptr<NotificationChannelsBridge> bridge_;

  bool platform_supports_channels_;

  std::unique_ptr<base::Clock> clock_;

  // Flag to keep track of whether |cached_channels_| has been initialized yet.
  bool initialized_cached_channels_;

  // Map of origin - NotificationChannel. Channel status may be out of date.
  // This cache is completely refreshed every time GetRuleIterator is called;
  // entries are also added and deleted when channels are added and deleted.
  // This cache serves three purposes:
  //
  // 1. For looking up the channel ID for an origin.
  //
  // 2. For looking up the channel creation timestamp for an origin.
  //
  // 3. To check if any channels have changed status since the last time
  //    they were checked, in order to notify observers. This is necessary to
  //    detect channels getting blocked/enabled by the user, in the absence of a
  //    callback for this event.
  std::map<std::string, NotificationChannel> cached_channels_;

  base::WeakPtrFactory<NotificationChannelsProviderAndroid> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NotificationChannelsProviderAndroid);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_CHANNELS_PROVIDER_ANDROID_H_
