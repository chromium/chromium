// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_channels_provider_android.h"

#include <algorithm>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "chrome/android/chrome_jni_headers/NotificationSettingsBridge_jni.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/content_settings_details.h"
#include "components/content_settings/core/browser/content_settings_pref_provider.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

using base::android::AttachCurrentThread;
using base::android::BuildInfo;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace {

class NotificationChannelsBridgeImpl
    : public NotificationChannelsProviderAndroid::NotificationChannelsBridge {
 public:
  NotificationChannelsBridgeImpl() = default;
  ~NotificationChannelsBridgeImpl() override = default;

  bool ShouldUseChannelSettings() override {
    return BuildInfo::GetInstance()->sdk_int() >=
           base::android::SDK_VERSION_OREO;
  }

  NotificationChannel CreateChannel(const std::string& origin,
                                    const base::Time& timestamp,
                                    bool enabled) override {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> jchannel =
        Java_NotificationSettingsBridge_createChannel(
            env, ConvertUTF8ToJavaString(env, origin),
            timestamp.ToInternalValue(), enabled);
    return NotificationChannel(
        ConvertJavaStringToUTF8(Java_SiteChannel_getId(env, jchannel)),
        ConvertJavaStringToUTF8(Java_SiteChannel_getOrigin(env, jchannel)),
        base::Time::FromInternalValue(
            Java_SiteChannel_getTimestamp(env, jchannel)),
        static_cast<NotificationChannelStatus>(
            Java_SiteChannel_getStatus(env, jchannel)));
  }

  NotificationChannelStatus GetChannelStatus(
      const std::string& channel_id) override {
    JNIEnv* env = AttachCurrentThread();
    return static_cast<NotificationChannelStatus>(
        Java_NotificationSettingsBridge_getChannelStatus(
            env, ConvertUTF8ToJavaString(env, channel_id)));
  }

  void DeleteChannel(const std::string& origin) override {
    JNIEnv* env = AttachCurrentThread();
    Java_NotificationSettingsBridge_deleteChannel(
        env, ConvertUTF8ToJavaString(env, origin));
  }

  std::vector<NotificationChannel> GetChannels() override {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobjectArray> raw_channels =
        Java_NotificationSettingsBridge_getSiteChannels(env);
    std::vector<NotificationChannel> channels;
    for (auto jchannel : raw_channels.ReadElements<jobject>()) {
      channels.push_back(NotificationChannel(
          ConvertJavaStringToUTF8(Java_SiteChannel_getId(env, jchannel)),
          ConvertJavaStringToUTF8(Java_SiteChannel_getOrigin(env, jchannel)),
          base::Time::FromInternalValue(
              Java_SiteChannel_getTimestamp(env, jchannel)),
          static_cast<NotificationChannelStatus>(
              Java_SiteChannel_getStatus(env, jchannel))));
    }
    return channels;
  }
};

ContentSetting ChannelStatusToContentSetting(NotificationChannelStatus status) {
  switch (status) {
    case NotificationChannelStatus::ENABLED:
      return CONTENT_SETTING_ALLOW;
    case NotificationChannelStatus::BLOCKED:
      return CONTENT_SETTING_BLOCK;
    case NotificationChannelStatus::UNAVAILABLE:
      NOTREACHED();
  }
  return CONTENT_SETTING_DEFAULT;
}

class ChannelsRuleIterator : public content_settings::RuleIterator {
 public:
  explicit ChannelsRuleIterator(std::vector<NotificationChannel> channels)
      : channels_(std::move(channels)), index_(0) {}

  ~ChannelsRuleIterator() override = default;

  bool HasNext() const override { return index_ < channels_.size(); }

  content_settings::Rule Next() override {
    DCHECK(HasNext());
    DCHECK_NE(channels_[index_].status, NotificationChannelStatus::UNAVAILABLE);
    content_settings::Rule rule = content_settings::Rule(
        ContentSettingsPattern::FromURLNoWildcard(
            GURL(channels_[index_].origin)),
        ContentSettingsPattern::Wildcard(),
        base::Value(ChannelStatusToContentSetting(channels_[index_].status)));
    index_++;
    return rule;
  }

 private:
  std::vector<NotificationChannel> channels_;
  size_t index_;
  DISALLOW_COPY_AND_ASSIGN(ChannelsRuleIterator);
};

// This copies the logic of
// SearchPermissionsService::IsPermissionControlledByDSE, which cannot be
// called from this class as it would introduce a circular dependency between
// the HostContentSettingsMap and the SearchPermissionsService factories.
bool OriginMatchesDefaultSearchEngine(TemplateURLService* template_url_service,
                                      const std::string& origin) {
  if (!template_url_service)
    return false;

  const TemplateURL* default_search_engine =
      template_url_service->GetDefaultSearchProvider();

  if (!default_search_engine)
    return false;

  GURL default_search_engine_url = default_search_engine->GenerateSearchURL(
      template_url_service->search_terms_data());

  return url::IsSameOriginWith(GURL(origin), default_search_engine_url);
}
}  // anonymous namespace

// static
void NotificationChannelsProviderAndroid::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kClearedBlockedSiteNotificationChannels,
                                false /* default_value */);
  registry->RegisterBooleanPref(prefs::kMigratedToSiteNotificationChannels,
                                false);
}

NotificationChannel::NotificationChannel(const std::string& id,
                                         const std::string& origin,
                                         const base::Time& timestamp,
                                         NotificationChannelStatus status)
    : id(id), origin(origin), timestamp(timestamp), status(status) {}

NotificationChannel::NotificationChannel(const NotificationChannel& other) =
    default;

NotificationChannelsProviderAndroid::NotificationChannelsProviderAndroid()
    : NotificationChannelsProviderAndroid(
          std::make_unique<NotificationChannelsBridgeImpl>(),
          std::make_unique<base::DefaultClock>()) {}

NotificationChannelsProviderAndroid::NotificationChannelsProviderAndroid(
    std::unique_ptr<NotificationChannelsBridge> bridge,
    std::unique_ptr<base::Clock> clock)
    : bridge_(std::move(bridge)),
      platform_supports_channels_(bridge_->ShouldUseChannelSettings()),
      clock_(std::move(clock)),
      initialized_cached_channels_(false) {}

NotificationChannelsProviderAndroid::~NotificationChannelsProviderAndroid() =
    default;

void NotificationChannelsProviderAndroid::MigrateToChannelsIfNecessary(
    PrefService* prefs,
    content_settings::ProviderInterface* pref_provider) {
  if (!platform_supports_channels_ ||
      prefs->GetBoolean(prefs::kMigratedToSiteNotificationChannels)) {
    return;
  }
  InitCachedChannels();

  std::vector<content_settings::Rule> rules;

  // Collect the existing rules and create channels for them.
  {
    std::unique_ptr<content_settings::RuleIterator> it(
        pref_provider->GetRuleIterator(ContentSettingsType::NOTIFICATIONS,
                                       std::string(), false /* incognito */));

    while (it && it->HasNext()) {
      content_settings::Rule rule = it->Next();
      CreateChannelForRule(rule);
      rules.push_back(std::move(rule));
    }
  }

  // Remove the existing |rules| from the preference provider.
  for (const auto& rule : rules) {
    pref_provider->SetWebsiteSetting(
        rule.primary_pattern, rule.secondary_pattern,
        ContentSettingsType::NOTIFICATIONS,
        content_settings::ResourceIdentifier(), nullptr);
  }

  prefs->SetBoolean(prefs::kMigratedToSiteNotificationChannels, true);
}

void NotificationChannelsProviderAndroid::ClearBlockedChannelsIfNecessary(
    PrefService* prefs,
    TemplateURLService* template_url_service) {
  if (!platform_supports_channels_ ||
      prefs->GetBoolean(prefs::kClearedBlockedSiteNotificationChannels)) {
    return;
  }

  for (const NotificationChannel& channel : bridge_->GetChannels()) {
    if (channel.status != NotificationChannelStatus::BLOCKED)
      continue;
    if (OriginMatchesDefaultSearchEngine(template_url_service,
                                         channel.origin)) {
      // Do not clear the DSE permission, as it should always be ALLOW or BLOCK.
      continue;
    }
    bridge_->DeleteChannel(channel.id);
  }

  // Reset the cache.
  cached_channels_.clear();
  initialized_cached_channels_ = false;

  prefs->SetBoolean(prefs::kClearedBlockedSiteNotificationChannels, true);
}

std::unique_ptr<content_settings::RuleIterator>
NotificationChannelsProviderAndroid::GetRuleIterator(
    ContentSettingsType content_type,
    const content_settings::ResourceIdentifier& resource_identifier,
    bool incognito) const {
  if (content_type != ContentSettingsType::NOTIFICATIONS || incognito ||
      !platform_supports_channels_) {
    return nullptr;
  }
  std::vector<NotificationChannel> channels = UpdateCachedChannels();
  return channels.empty()
             ? nullptr
             : std::make_unique<ChannelsRuleIterator>(std::move(channels));
}

std::vector<NotificationChannel>
NotificationChannelsProviderAndroid::UpdateCachedChannels() const {
  std::vector<NotificationChannel> channels = bridge_->GetChannels();
  std::map<std::string, NotificationChannel> updated_channels_map;
  for (const auto& channel : channels)
    updated_channels_map.emplace(channel.origin, channel);
  if (updated_channels_map != cached_channels_) {
    // This const_cast is not ideal but tolerated because it doesn't change the
    // underlying state of NotificationChannelsProviderAndroid, and allows us to
    // notify observers as soon as we detect changes to channels.
    auto* provider = const_cast<NotificationChannelsProviderAndroid*>(this);
    base::CreateSingleThreadTaskRunner({content::BrowserThread::UI})
        ->PostTask(FROM_HERE,
                   base::BindOnce(
                       &NotificationChannelsProviderAndroid::NotifyObservers,
                       provider->weak_factory_.GetWeakPtr(),
                       ContentSettingsPattern(), ContentSettingsPattern(),
                       ContentSettingsType::NOTIFICATIONS, std::string()));
    provider->cached_channels_ = std::move(updated_channels_map);
    provider->initialized_cached_channels_ = true;
  }
  return channels;
}

bool NotificationChannelsProviderAndroid::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const content_settings::ResourceIdentifier& resource_identifier,
    std::unique_ptr<base::Value>&& value) {
  if (content_type != ContentSettingsType::NOTIFICATIONS ||
      !platform_supports_channels_) {
    return false;
  }
  // This provider only handles settings for specific origins.
  if (primary_pattern == ContentSettingsPattern::Wildcard() &&
      secondary_pattern == ContentSettingsPattern::Wildcard() &&
      resource_identifier.empty()) {
    return false;
  }

  InitCachedChannels();

  url::Origin origin = url::Origin::Create(GURL(primary_pattern.ToString()));
  DCHECK(!origin.opaque());
  const std::string origin_string = origin.Serialize();

  ContentSetting setting = content_settings::ValueToContentSetting(value.get());
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
      CreateChannelIfRequired(origin_string,
                              NotificationChannelStatus::ENABLED);
      break;
    case CONTENT_SETTING_BLOCK:
      CreateChannelIfRequired(origin_string,
                              NotificationChannelStatus::BLOCKED);
      break;
    case CONTENT_SETTING_DEFAULT: {
      auto channel_to_delete = cached_channels_.find(origin_string);
      if (channel_to_delete != cached_channels_.end()) {
        bridge_->DeleteChannel(channel_to_delete->second.id);
        cached_channels_.erase(channel_to_delete);
      }
      return false;
    }
    default:
      // We rely on notification settings being one of ALLOW/BLOCK/DEFAULT.
      NOTREACHED();
      break;
  }
  value.reset();
  return true;
}

void NotificationChannelsProviderAndroid::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {
  if (content_type != ContentSettingsType::NOTIFICATIONS ||
      !platform_supports_channels_) {
    return;
  }
  std::vector<NotificationChannel> channels = bridge_->GetChannels();
  for (auto channel : channels)
    bridge_->DeleteChannel(channel.id);
  cached_channels_.clear();

  if (channels.size() > 0) {
    NotifyObservers(ContentSettingsPattern(), ContentSettingsPattern(),
                    content_type, std::string());
  }
}

void NotificationChannelsProviderAndroid::ShutdownOnUIThread() {
  RemoveAllObservers();
}

base::Time NotificationChannelsProviderAndroid::GetWebsiteSettingLastModified(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const content_settings::ResourceIdentifier& resource_identifier) {
  if (content_type != ContentSettingsType::NOTIFICATIONS ||
      !platform_supports_channels_) {
    return base::Time();
  }
  url::Origin origin = url::Origin::Create(GURL(primary_pattern.ToString()));
  if (origin.opaque())
    return base::Time();
  const std::string origin_string = origin.Serialize();

  InitCachedChannels();
  auto channel_entry = cached_channels_.find(origin_string);
  if (channel_entry == cached_channels_.end())
    return base::Time();

  return channel_entry->second.timestamp;
}

// InitCachedChannels() must be called prior to calling this method.
void NotificationChannelsProviderAndroid::CreateChannelIfRequired(
    const std::string& origin_string,
    NotificationChannelStatus new_channel_status) {
  auto channel_entry = cached_channels_.find(origin_string);
  if (channel_entry == cached_channels_.end()) {
    base::Time timestamp = clock_->Now();

    NotificationChannel channel = bridge_->CreateChannel(
        origin_string, timestamp,
        new_channel_status == NotificationChannelStatus::ENABLED);
    cached_channels_.emplace(origin_string, std::move(channel));

    NotifyObservers(ContentSettingsPattern(), ContentSettingsPattern(),
                    ContentSettingsType::NOTIFICATIONS, std::string());
  } else {
    auto old_channel_status =
        bridge_->GetChannelStatus(channel_entry->second.id);
    DCHECK_EQ(old_channel_status, new_channel_status);
  }
}

// InitCachedChannels() must be called prior to calling this method.
void NotificationChannelsProviderAndroid::CreateChannelForRule(
    const content_settings::Rule& rule) {
  url::Origin origin =
      url::Origin::Create(GURL(rule.primary_pattern.ToString()));
  DCHECK(!origin.opaque());
  const std::string origin_string = origin.Serialize();
  ContentSetting content_setting =
      content_settings::ValueToContentSetting(&rule.value);
  switch (content_setting) {
    case CONTENT_SETTING_ALLOW:
      CreateChannelIfRequired(origin_string,
                              NotificationChannelStatus::ENABLED);
      break;
    case CONTENT_SETTING_BLOCK:
      CreateChannelIfRequired(origin_string,
                              NotificationChannelStatus::BLOCKED);
      break;
    default:
      // We assume notification preferences are either ALLOW/BLOCK.
      NOTREACHED();
      break;
  }
}

// This method must be called prior to accessing |cached_channels_|.
void NotificationChannelsProviderAndroid::InitCachedChannels() {
  if (initialized_cached_channels_)
    return;
  DCHECK_EQ(cached_channels_.size(), 0u);
  std::vector<NotificationChannel> channels = bridge_->GetChannels();
  for (auto channel : channels)
    cached_channels_.emplace(channel.origin, std::move(channel));
  initialized_cached_channels_ = true;
}
