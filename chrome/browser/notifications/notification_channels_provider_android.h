// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_CHANNELS_PROVIDER_ANDROID_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_CHANNELS_PROVIDER_ANDROID_H_

#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <tuple>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/time/time.h"
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
//
// PartitionKey is ignored by this provider because the content settings should
// apply across partitions.
class NotificationChannelsProviderAndroid
    : public content_settings::UserModifiableProvider {
 public:
  using GetChannelsCallback =
      base::OnceCallback<void(const std::vector<NotificationChannel>&)>;
  // Helper class to make the JNI calls.
  class NotificationChannelsBridge {
   public:
    virtual ~NotificationChannelsBridge() = default;
    virtual NotificationChannel CreateChannel(const std::string& origin,
                                              const base::Time& timestamp,
                                              bool enabled) = 0;
    virtual void DeleteChannel(const std::string& origin) = 0;
    virtual void GetChannels(GetChannelsCallback callback) = 0;
  };

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  explicit NotificationChannelsProviderAndroid(PrefService* pref_service);
  NotificationChannelsProviderAndroid(
      const NotificationChannelsProviderAndroid&) = delete;
  NotificationChannelsProviderAndroid& operator=(
      const NotificationChannelsProviderAndroid&) = delete;
  ~NotificationChannelsProviderAndroid() override;

  // Initialize cached channels, do migration and clear blocked channels if
  // necessary.
  void Initialize(content_settings::ProviderInterface* pref_provider,
                  TemplateURLService* template_url_service);

  // UserModifiableProvider methods.
  std::unique_ptr<content_settings::RuleIterator> GetRuleIterator(
      ContentSettingsType content_type,
      bool incognito,
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
  bool UpdateLastUsedTime(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      const base::Time time,
      const content_settings::PartitionKey& partition_key) override;
  bool ResetLastVisitTime(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const content_settings::PartitionKey& partition_key) override;
  bool UpdateLastVisitTime(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const content_settings::PartitionKey& partition_key) override;
  std::optional<base::TimeDelta> RenewContentSetting(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      std::optional<ContentSetting> setting_to_match,
      const content_settings::PartitionKey& partition_key) override;
  void SetClockForTesting(const base::Clock* clock) override;

 protected:
  // Migrates any notification settings from the passed-in provider to
  // channels, unless they were already migrated or channels should not be used.
  void MigrateToChannelsIfNecessary(
      content_settings::ProviderInterface* pref_provider);

  // Deletes any existing blocked site channels, unless this one-off deletion
  // already occurred. See https://crbug.com/835232.
  void ClearBlockedChannelsIfNecessary(
      TemplateURLService* template_url_service);

 private:
  NotificationChannelsProviderAndroid(
      PrefService* pref_service,
      std::unique_ptr<NotificationChannelsBridge> bridge);
  friend class NotificationChannelsProviderAndroidTest;

  // Don't call this directly.
  // Helper methods for implementing MigrateToChannelsIfNecessary(). Called
  // when `cached_channels_` are initialized.
  void MigrateToChannelsIfNecessaryImpl(
      content_settings::ProviderInterface* pref_provider);

  // Don't call this directly.
  // Helper methods for implementing ClearBlockedChannelsIfNecessary(). Called
  // when updated channels are retrieved.
  void ClearBlockedChannelsIfNecessaryImpl(
      TemplateURLService* template_url_service,
      const std::vector<NotificationChannel>& channels);

  // Don't call this directly.
  // Helper methods for implementing ClearAllContentSettingsRules(). Called
  // when updated channels are retrieved.
  void ClearAllChannelsImpl(ContentSettingsType content_type,
                            const std::vector<NotificationChannel>& channels);

  // Don't call this directly.
  // Helper methods for implementing SetWebsiteSetting(). Called when
  // `cached_channels_` are initialized.
  void UpdateChannelForWebsiteImpl(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      ContentSetting content_setting,
      const content_settings::ContentSettingConstraints& constraints);

  // Don't call this directly. Pass this as a callback to ScheduleGetChannels().
  // Update cached channels. If `only_initialize_null_cached_channels`, cached
  // channel will get updated only if it is null. Otherwise, all the observers
  // will be notified if cached channel is updated. Once cached channels are
  // updated, `on_channel_updated_cb` will be invoked.  This method is posted by
  // ScheduleGetChannels() and runs when notification channels are retrieved
  // from Android.
  void UpdateCachedChannelsImpl(
      bool only_initialize_null_cached_channels,
      base::OnceClosure on_channel_updated_cb,
      const std::vector<NotificationChannel>& channels);

  // Create notification channel if required.
  void CreateChannelIfRequired(const std::string& origin_string,
                               NotificationChannelStatus new_channel_status);

  // Create notification channel for a given rule
  void CreateChannelForRule(const content_settings::Rule& rule);

  // Called to initialize cached channels. Once complete,
  // `on_channels_initialized_cb` will be invoked.
  void InitCachedChannels(base::OnceClosure on_channels_initialized_cb);

  // Schedule an pending operation to get Java notification channels. Once
  // the previous pending operation completes, GetChannelsImpl() will be
  // invoked.
  void ScheduleGetChannels(bool skip_get_if_cached_channels_are_available,
                           GetChannelsCallback callback);

  // Don't call this directly. Call ScheduleGetChannels() instead.
  // Gets channels from java side and invoke `get_channels_cb` and
  // `on_task_completed_cb` on completion. If
  // `skip_get_if_cached_channels_are_available` is true, callbacks will be
  // invoked immediately if cached channels are not empty.
  void GetChannelsImpl(bool skip_get_if_cached_channels_are_available,
                       GetChannelsCallback get_channels_cb,
                       base::OnceClosure on_task_completed_cb);

  // Called when GetChannels() completes.
  void OnGetChannelsDone(GetChannelsCallback get_channels_cb,
                         base::OnceClosure on_task_completed_cb,
                         const std::vector<NotificationChannel>& channels);

  // Called to process the next pending operation.
  void ProcessPendingOperations();

  // Called when a pending operation completes.
  void OnCurrentOperationFinished();

  void RecordCachedChannelStatus();

  std::unique_ptr<NotificationChannelsBridge> bridge_;

  raw_ptr<const base::Clock> clock_;

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
  std::optional<std::map<std::string, NotificationChannel>> cached_channels_;

  using PendingCallback = base::OnceCallback<void(base::OnceClosure)>;
  // This is a list of postponed calls to update cached_channels_.
  std::queue<PendingCallback> pending_operations_;

  // PrefService associated with this instance.
  raw_ptr<PrefService> pref_service_;

  bool is_processing_pending_operations_ = false;

  bool has_get_rule_iterator_called_ = false;

  base::WeakPtrFactory<NotificationChannelsProviderAndroid> weak_factory_{this};
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_CHANNELS_PROVIDER_ANDROID_H_
