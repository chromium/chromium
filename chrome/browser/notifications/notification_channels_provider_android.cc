// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_channels_provider_android.h"

#include <algorithm>
#include <utility>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/content_settings_pref_provider.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_partition_key.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/notifications/jni_headers/NotificationSettingsBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::BuildInfo;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

class NotificationChannelsBridgeImpl
    : public NotificationChannelsProviderAndroid::NotificationChannelsBridge {
 public:
  NotificationChannelsBridgeImpl() = default;
  ~NotificationChannelsBridgeImpl() override = default;

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

  void DeleteChannel(const std::string& origin) override {
    JNIEnv* env = AttachCurrentThread();
    Java_NotificationSettingsBridge_deleteChannel(
        env, ConvertUTF8ToJavaString(env, origin));
  }

  void GetChannels(NotificationChannelsProviderAndroid::GetChannelsCallback
                       callback) override {
    JNIEnv* env = AttachCurrentThread();

    NotificationChannelsProviderAndroid::GetChannelsCallback cb =
        base::BindOnce(&NotificationChannelsBridgeImpl::OnGetChannelsDone,
                       weak_factory_.GetWeakPtr(), std::move(callback));
    intptr_t callback_id = reinterpret_cast<intptr_t>(
        new NotificationChannelsProviderAndroid::GetChannelsCallback(
            std::move(cb)));
    Java_NotificationSettingsBridge_getSiteChannels(env, callback_id);
  }

 private:
  void OnGetChannelsDone(
      NotificationChannelsProviderAndroid::GetChannelsCallback callback,
      const std::vector<NotificationChannel>& channels) {
    std::move(callback).Run(channels);
  }

  base::WeakPtrFactory<NotificationChannelsBridgeImpl> weak_factory_{this};
};

ContentSetting ChannelStatusToContentSetting(NotificationChannelStatus status) {
  switch (status) {
    case NotificationChannelStatus::ENABLED:
      return CONTENT_SETTING_ALLOW;
    case NotificationChannelStatus::BLOCKED:
      return CONTENT_SETTING_BLOCK;
    case NotificationChannelStatus::UNAVAILABLE:
      NOTREACHED_IN_MIGRATION();
  }
  return CONTENT_SETTING_DEFAULT;
}

class ChannelsRuleIterator : public content_settings::RuleIterator {
 public:
  explicit ChannelsRuleIterator(std::vector<NotificationChannel> channels)
      : channels_(std::move(channels)) {}

  ChannelsRuleIterator(const ChannelsRuleIterator&) = delete;
  ChannelsRuleIterator& operator=(const ChannelsRuleIterator&) = delete;
  ~ChannelsRuleIterator() override = default;

  bool HasNext() const override { return index_ < channels_.size(); }

  std::unique_ptr<content_settings::Rule> Next() override {
    DCHECK(HasNext());
    auto& channel = channels_[index_];
    DCHECK_NE(channels_[index_].status, NotificationChannelStatus::UNAVAILABLE);
    content_settings::RuleMetaData metadata;
    metadata.set_last_modified(channel.timestamp);
    std::unique_ptr<content_settings::Rule> rule =
        std::make_unique<content_settings::Rule>(
            ContentSettingsPattern::FromURLNoWildcard(GURL(channel.origin)),
            ContentSettingsPattern::Wildcard(),
            base::Value(ChannelStatusToContentSetting(channel.status)),
            metadata);
    index_++;
    return rule;
  }

 private:
  std::vector<NotificationChannel> channels_;
  size_t index_ = 0;
};

}  // anonymous namespace

static void JNI_NotificationSettingsBridge_OnGetSiteChannelsDone(
    JNIEnv* env,
    jlong callback_id,
    const JavaParamRef<jobjectArray>& j_channels) {
  std::vector<NotificationChannel> channels;
  for (auto jchannel : j_channels.ReadElements<jobject>()) {
    channels.emplace_back(
        ConvertJavaStringToUTF8(Java_SiteChannel_getId(env, jchannel)),
        ConvertJavaStringToUTF8(Java_SiteChannel_getOrigin(env, jchannel)),
        base::Time::FromInternalValue(
            Java_SiteChannel_getTimestamp(env, jchannel)),
        static_cast<NotificationChannelStatus>(
            Java_SiteChannel_getStatus(env, jchannel)));
  }

  // Convert java long long int to c++ pointer, take ownership.
  std::unique_ptr<NotificationChannelsProviderAndroid::GetChannelsCallback> cb(
      reinterpret_cast<
          NotificationChannelsProviderAndroid::GetChannelsCallback*>(
          callback_id));
  std::move(*cb).Run(std::move(channels));
}

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

NotificationChannelsProviderAndroid::NotificationChannelsProviderAndroid(
    PrefService* pref_service)
    : NotificationChannelsProviderAndroid(
          pref_service,
          std::make_unique<NotificationChannelsBridgeImpl>()) {}

NotificationChannelsProviderAndroid::NotificationChannelsProviderAndroid(
    PrefService* pref_service,
    std::unique_ptr<NotificationChannelsBridge> bridge)
    : bridge_(std::move(bridge)),
      clock_(base::DefaultClock::GetInstance()),
      pref_service_(pref_service) {}

NotificationChannelsProviderAndroid::~NotificationChannelsProviderAndroid() =
    default;

void NotificationChannelsProviderAndroid::Initialize(
    content_settings::ProviderInterface* pref_provider,
    TemplateURLService* template_url_service) {
  MigrateToChannelsIfNecessary(pref_provider);

  // Clear blocked channels *after* migrating in case the pref provider
  // contained any erroneously-created channels that need deleting.
  ClearBlockedChannelsIfNecessary(template_url_service);

  InitCachedChannels(base::DoNothing());
}

void NotificationChannelsProviderAndroid::MigrateToChannelsIfNecessary(
    content_settings::ProviderInterface* pref_provider) {
  if (pref_service_->GetBoolean(prefs::kMigratedToSiteNotificationChannels)) {
    return;
  }

  InitCachedChannels(base::BindOnce(
      &NotificationChannelsProviderAndroid::MigrateToChannelsIfNecessaryImpl,
      weak_factory_.GetWeakPtr(), base::Unretained(pref_provider)));
}

void NotificationChannelsProviderAndroid::MigrateToChannelsIfNecessaryImpl(
    content_settings::ProviderInterface* pref_provider) {
  std::vector<std::pair<ContentSettingsPattern, ContentSettingsPattern>>
      patterns;

  // Collect the existing rules and create channels for them.
  {
    std::unique_ptr<content_settings::RuleIterator> it(
        pref_provider->GetRuleIterator(
            ContentSettingsType::NOTIFICATIONS, false /* incognito */,
            content_settings::PartitionKey::WipGetDefault()));

    while (it && it->HasNext()) {
      std::unique_ptr<content_settings::Rule> rule = it->Next();
      CreateChannelForRule(*rule);
      patterns.emplace_back(std::move(rule->primary_pattern),
                            std::move(rule->secondary_pattern));
    }
  }

  // Remove the existing |rules| from the preference provider.
  for (const auto& pattern : patterns) {
    pref_provider->SetWebsiteSetting(
        pattern.first, pattern.second, ContentSettingsType::NOTIFICATIONS,
        base::Value(), {}, content_settings::PartitionKey::WipGetDefault());
  }

  pref_service_->SetBoolean(prefs::kMigratedToSiteNotificationChannels, true);
}

void NotificationChannelsProviderAndroid::ClearBlockedChannelsIfNecessary(
    TemplateURLService* template_url_service) {
  if (pref_service_->GetBoolean(
          prefs::kClearedBlockedSiteNotificationChannels)) {
    return;
  }

  ScheduleGetChannels(
      /*skip_get_if_cached_channels_are_available=*/false,
      base::BindOnce(&NotificationChannelsProviderAndroid::
                         ClearBlockedChannelsIfNecessaryImpl,
                     weak_factory_.GetWeakPtr(),
                     base::Unretained(template_url_service)));
}

void NotificationChannelsProviderAndroid::ClearBlockedChannelsIfNecessaryImpl(
    TemplateURLService* template_url_service,
    const std::vector<NotificationChannel>& channels) {
  url::Origin default_search_engine_origin;
  if (template_url_service) {
    default_search_engine_origin =
        template_url_service->GetDefaultSearchProviderOrigin();
  }

  for (const NotificationChannel& channel : channels) {
    if (channel.status != NotificationChannelStatus::BLOCKED)
      continue;
    if (default_search_engine_origin.IsSameOriginWith(GURL(channel.origin))) {
      // Do not clear the DSE permission, as it should always be ALLOW or BLOCK.
      continue;
    }
    bridge_->DeleteChannel(channel.id);
  }

  // Reset the cache.
  cached_channels_.reset();

  pref_service_->SetBoolean(prefs::kClearedBlockedSiteNotificationChannels,
                            true);
}

std::unique_ptr<content_settings::RuleIterator>
NotificationChannelsProviderAndroid::GetRuleIterator(
    ContentSettingsType content_type,
    bool incognito,
    const content_settings::PartitionKey& partition_key) const {
  if (content_type != ContentSettingsType::NOTIFICATIONS || incognito) {
    return nullptr;
  }

  // This const_cast is not ideal but tolerated because it allows us to
  // notify observers as soon as we detect changes to channels.
  auto* provider = const_cast<NotificationChannelsProviderAndroid*>(this);
  provider->RecordCachedChannelStatus();

  if (!cached_channels_) {
    return nullptr;
  }

  std::vector<NotificationChannel> channels;
  for (const auto& cached_channel : *cached_channels_) {
    channels.push_back(cached_channel.second);
  }

  // The returned RuleIterator is from cached channels, so it might not
  // contain up-to-date information if user has modified notification settings,
  // As a result, schedule an channel update to inform all observers if
  // something has changed.
  provider->ScheduleGetChannels(
      /*skip_get_if_cached_channels_are_available=*/false,
      base::BindOnce(
          &NotificationChannelsProviderAndroid::UpdateCachedChannelsImpl,
          provider->weak_factory_.GetWeakPtr(),
          /*only_initialize_null_cached_channels=*/false, base::DoNothing()));

  return channels.empty()
             ? nullptr
             : std::make_unique<ChannelsRuleIterator>(std::move(channels));
}

bool NotificationChannelsProviderAndroid::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    base::Value&& value,
    const content_settings::ContentSettingConstraints& constraints,
    const content_settings::PartitionKey& partition_key) {
  if (content_type != ContentSettingsType::NOTIFICATIONS) {
    return false;
  }
  // This provider only handles settings for specific origins.
  if (primary_pattern == ContentSettingsPattern::Wildcard() &&
      secondary_pattern == ContentSettingsPattern::Wildcard()) {
    return false;
  }

  // These constraints are not supported for notifications on Android.
  DCHECK_EQ(constraints.expiration(), base::Time());
  DCHECK_EQ(constraints.session_model(),
            content_settings::mojom::SessionModel::DURABLE);
  DCHECK_EQ(constraints.track_last_visit_for_autoexpiration(), false);

  ContentSetting setting = content_settings::ValueToContentSetting(value);
  InitCachedChannels(base::BindOnce(
      &NotificationChannelsProviderAndroid::UpdateChannelForWebsiteImpl,
      weak_factory_.GetWeakPtr(), primary_pattern, secondary_pattern,
      content_type, setting, constraints));

  if (setting == CONTENT_SETTING_DEFAULT) {
    return false;
  }
  value = base::Value();
  return true;
}

void NotificationChannelsProviderAndroid::UpdateChannelForWebsiteImpl(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    ContentSetting content_setting,
    const content_settings::ContentSettingConstraints& constraints) {
  url::Origin origin = url::Origin::Create(GURL(primary_pattern.ToString()));
  DCHECK(!origin.opaque());
  const std::string origin_string = origin.Serialize();
  switch (content_setting) {
    case CONTENT_SETTING_ALLOW:
      CreateChannelIfRequired(origin_string,
                              NotificationChannelStatus::ENABLED);
      break;
    case CONTENT_SETTING_BLOCK:
      CreateChannelIfRequired(origin_string,
                              NotificationChannelStatus::BLOCKED);
      break;
    case CONTENT_SETTING_DEFAULT: {
      auto channel_to_delete = cached_channels_->find(origin_string);
      if (channel_to_delete != cached_channels_->end()) {
        bridge_->DeleteChannel(channel_to_delete->second.id);
        cached_channels_->erase(channel_to_delete);
        NotifyObservers(primary_pattern, secondary_pattern, content_type,
                        /*partition_key=*/nullptr);
      }
      break;
    }
    default:
      // We rely on notification settings being one of ALLOW/BLOCK/DEFAULT.
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void NotificationChannelsProviderAndroid::ClearAllContentSettingsRules(
    ContentSettingsType content_type,
    const content_settings::PartitionKey& partition_key) {
  if (content_type != ContentSettingsType::NOTIFICATIONS) {
    return;
  }

  ScheduleGetChannels(
      /*skip_get_if_cached_channels_are_available=*/false,
      base::BindOnce(&NotificationChannelsProviderAndroid::ClearAllChannelsImpl,
                     weak_factory_.GetWeakPtr(), content_type));
}

void NotificationChannelsProviderAndroid::ClearAllChannelsImpl(
    ContentSettingsType content_type,
    const std::vector<NotificationChannel>& channels) {
  for (auto channel : channels) {
    bridge_->DeleteChannel(channel.id);
  }
  cached_channels_->clear();

  if (channels.size() > 0) {
    NotifyObservers(ContentSettingsPattern::Wildcard(),
                    ContentSettingsPattern::Wildcard(), content_type,
                    /*partition_key=*/nullptr);
  }
}

void NotificationChannelsProviderAndroid::ShutdownOnUIThread() {
  RemoveAllObservers();
}

bool NotificationChannelsProviderAndroid::UpdateLastUsedTime(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    const base::Time time,
    const content_settings::PartitionKey& partition_key) {
  // Last used tracking is not implemented for this type.
  return false;
}

bool NotificationChannelsProviderAndroid::ResetLastVisitTime(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const content_settings::PartitionKey& partition_key) {
  // Last visited tracking is not implemented for this type.
  return false;
}

bool NotificationChannelsProviderAndroid::UpdateLastVisitTime(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const content_settings::PartitionKey& partition_key) {
  // Last visited tracking is not implemented for this type.
  return false;
}

std::optional<base::TimeDelta>
NotificationChannelsProviderAndroid::RenewContentSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    std::optional<ContentSetting> setting_to_match,
    const content_settings::PartitionKey& partition_key) {
  // Setting renewal is not implemented for this type.
  return std::nullopt;
}

void NotificationChannelsProviderAndroid::SetClockForTesting(
    const base::Clock* clock) {
  clock_ = clock;
}

// InitCachedChannels() must be called prior to calling this method.
void NotificationChannelsProviderAndroid::CreateChannelIfRequired(
    const std::string& origin_string,
    NotificationChannelStatus new_channel_status) {
  auto channel_entry = cached_channels_->find(origin_string);
  if (channel_entry == cached_channels_->end()) {
    base::Time timestamp = clock_->Now();

    NotificationChannel channel = bridge_->CreateChannel(
        origin_string, timestamp,
        new_channel_status == NotificationChannelStatus::ENABLED);
    cached_channels_->emplace(origin_string, std::move(channel));

    NotifyObservers(
        ContentSettingsPattern::Wildcard(), ContentSettingsPattern::Wildcard(),
        ContentSettingsType::NOTIFICATIONS, /*partition_key=*/nullptr);
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
      content_settings::ValueToContentSetting(rule.value);
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
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

// This method must be called prior to accessing |cached_channels_|.
void NotificationChannelsProviderAndroid::InitCachedChannels(
    base::OnceClosure on_channels_initialized_cb) {
  ScheduleGetChannels(
      /*skip_get_if_cached_channels_are_available=*/true,
      base::BindOnce(
          &NotificationChannelsProviderAndroid::UpdateCachedChannelsImpl,
          weak_factory_.GetWeakPtr(),
          /*only_initialize_null_cached_channels=*/true,
          std::move(on_channels_initialized_cb)));
}

void NotificationChannelsProviderAndroid::UpdateCachedChannelsImpl(
    bool only_initialize_null_cached_channels,
    base::OnceClosure on_channel_updated_cb,
    const std::vector<NotificationChannel>& channels) {
  std::map<std::string, NotificationChannel> updated_channels_map;
  for (const auto& channel : channels) {
    updated_channels_map.emplace(channel.origin, channel);
  }

  if (only_initialize_null_cached_channels) {
    if (!cached_channels_) {
      cached_channels_ = std::move(updated_channels_map);
    }
  } else {
    if (updated_channels_map != cached_channels_.value()) {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&NotificationChannelsProviderAndroid::NotifyObservers,
                         weak_factory_.GetWeakPtr(),
                         ContentSettingsPattern::Wildcard(),
                         ContentSettingsPattern::Wildcard(),
                         ContentSettingsType::NOTIFICATIONS,
                         /*partition_key=*/nullptr));
      cached_channels_ = std::move(updated_channels_map);
    }
  }
  std::move(on_channel_updated_cb).Run();
}

void NotificationChannelsProviderAndroid::ScheduleGetChannels(
    bool skip_get_if_cached_channels_are_available,
    GetChannelsCallback get_channels_cb) {
  pending_operations_.push(base::BindOnce(
      &NotificationChannelsProviderAndroid::GetChannelsImpl,
      weak_factory_.GetWeakPtr(), skip_get_if_cached_channels_are_available,
      std::move(get_channels_cb)));
  ProcessPendingOperations();
}

void NotificationChannelsProviderAndroid::GetChannelsImpl(
    bool skip_get_if_cached_channels_are_available,
    GetChannelsCallback get_channels_cb,
    base::OnceClosure on_task_completed_cb) {
  if (skip_get_if_cached_channels_are_available && cached_channels_) {
    OnGetChannelsDone(std::move(get_channels_cb),
                      std::move(on_task_completed_cb),
                      std::vector<NotificationChannel>());
    return;
  }
  bridge_->GetChannels(
      base::BindOnce(&NotificationChannelsProviderAndroid::OnGetChannelsDone,
                     weak_factory_.GetWeakPtr(), std::move(get_channels_cb),
                     std::move(on_task_completed_cb)));
}

void NotificationChannelsProviderAndroid::OnGetChannelsDone(
    GetChannelsCallback get_channels_cb,
    base::OnceClosure on_task_completed_cb,
    const std::vector<NotificationChannel>& channels) {
  std::move(get_channels_cb).Run(channels);
  std::move(on_task_completed_cb).Run();
}

void NotificationChannelsProviderAndroid::ProcessPendingOperations() {
  if (is_processing_pending_operations_ || pending_operations_.empty()) {
    return;
  }

  is_processing_pending_operations_ = true;
  PendingCallback callback = std::move(pending_operations_.front());
  pending_operations_.pop();
  std::move(callback).Run(base::BindOnce(
      &NotificationChannelsProviderAndroid::OnCurrentOperationFinished,
      weak_factory_.GetWeakPtr()));
}

void NotificationChannelsProviderAndroid::OnCurrentOperationFinished() {
  DCHECK(is_processing_pending_operations_);
  is_processing_pending_operations_ = false;
  ProcessPendingOperations();
}

void NotificationChannelsProviderAndroid::RecordCachedChannelStatus() {
  if (!has_get_rule_iterator_called_) {
    base::UmaHistogramBoolean(
        "Notifications.Android.CachedChannelsStatusOnFirstGetRuleIterator",
        !!cached_channels_);
    has_get_rule_iterator_called_ = true;
  }
}
