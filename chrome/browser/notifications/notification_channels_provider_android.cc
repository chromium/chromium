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
constexpr char kPendingChannelId[] = "pending";

// Returns the callback to run when notification channel state changes.
base::RepeatingCallback<void(const NotificationChannel& channel)>&
GetChannelStateChangedCallback() {
  static base::NoDestructor<
      base::RepeatingCallback<void(const NotificationChannel& channel)>>
      channel_state_changed_callback;
  return *channel_state_changed_callback;
}

NotificationChannel CreatePendingChannel(const std::string& origin_string,
                                         ContentSetting content_setting,
                                         base::Time timestamp) {
  NotificationChannelStatus status = NotificationChannelStatus::UNAVAILABLE;
  switch (content_setting) {
    case CONTENT_SETTING_ALLOW:
      status = NotificationChannelStatus::ENABLED;
      break;
    case CONTENT_SETTING_BLOCK:
      status = NotificationChannelStatus::BLOCKED;
      break;
    case CONTENT_SETTING_DEFAULT:
      break;
    default:
      NOTREACHED();
  }
  return NotificationChannel(kPendingChannelId, origin_string, timestamp,
                             status);
}

std::string GetOriginStringFromPattern(const ContentSettingsPattern& pattern) {
  url::Origin origin = url::Origin::Create(GURL(pattern.ToString()));
  DCHECK(!origin.opaque());
  return origin.Serialize();
}

class NotificationChannelsBridgeImpl
    : public NotificationChannelsProviderAndroid::NotificationChannelsBridge {
 public:
  NotificationChannelsBridgeImpl(NotificationChannelsProviderAndroid*
                                     notification_channel_provider_android)
      : notification_channel_provider_android_(
            notification_channel_provider_android) {
    if (GetChannelStateChangedCallback().is_null()) {
      GetChannelStateChangedCallback() = base::BindRepeating(
          &NotificationChannelsBridgeImpl::OnChannelStateChanged,
          weak_factory_.GetWeakPtr());
    } else {
      LOG(WARNING) << "Notification channels are used by two profiles. Changes"
                      "will only affect the first profile.";
    }
  }

  ~NotificationChannelsBridgeImpl() override {
    GetChannelStateChangedCallback().Reset();
  }

  NotificationChannel CreateChannel(const std::string& origin,
                                    const base::Time& timestamp,
                                    bool enabled) override {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> jchannel =
        Java_NotificationSettingsBridge_createChannel(
            env, origin, timestamp.ToInternalValue(), enabled);
    return NotificationChannel(
        Java_SiteChannel_getId(env, jchannel),
        Java_SiteChannel_getOrigin(env, jchannel),
        base::Time::FromInternalValue(
            Java_SiteChannel_getTimestamp(env, jchannel)),
        static_cast<NotificationChannelStatus>(
            Java_SiteChannel_getStatus(env, jchannel)));
  }

  void DeleteChannel(const std::string& origin) override {
    JNIEnv* env = AttachCurrentThread();
    Java_NotificationSettingsBridge_deleteChannel(env, origin);
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

  void OnChannelStateChanged(const NotificationChannel& channel) {
    notification_channel_provider_android_->OnChannelStateChanged(channel);
  }

  raw_ptr<NotificationChannelsProviderAndroid>
      notification_channel_provider_android_;

  base::WeakPtrFactory<NotificationChannelsBridgeImpl> weak_factory_{this};
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
            std::move(metadata));
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
    channels.emplace_back(Java_SiteChannel_getId(env, jchannel),
                          Java_SiteChannel_getOrigin(env, jchannel),
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

static void JNI_NotificationSettingsBridge_OnChannelStateChanged(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_channel_id,
    const JavaParamRef<jstring>& j_origin,
    jboolean blocked) {
  if (GetChannelStateChangedCallback().is_null()) {
    return;
  }

  NotificationChannelStatus status = blocked
                                         ? NotificationChannelStatus::BLOCKED
                                         : NotificationChannelStatus::ENABLED;

  GetChannelStateChangedCallback().Run(NotificationChannel(
      base::android::ConvertJavaStringToUTF8(env, j_channel_id),
      base::android::ConvertJavaStringToUTF8(env, j_origin), base::Time::Now(),
      status));
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

NotificationChannelsProviderAndroid::NotificationChannelsProviderAndroid(
    PrefService* pref_service)
    : NotificationChannelsProviderAndroid(
          pref_service,
          std::make_unique<NotificationChannelsBridgeImpl>(this)) {}

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

  GetCachedChannelsIfNecessary(base::DoNothing());
}

void NotificationChannelsProviderAndroid::MigrateToChannelsIfNecessary(
    content_settings::ProviderInterface* pref_provider) {
  if (!pref_service_ ||
      pref_service_->GetBoolean(prefs::kMigratedToSiteNotificationChannels)) {
    return;
  }

  // If there are no existing rules, no need to migrate.
  std::unique_ptr<content_settings::RuleIterator> it(
      pref_provider->GetRuleIterator(
          ContentSettingsType::NOTIFICATIONS, false /* off_the_record */,
          content_settings::PartitionKey::WipGetDefault()));
  if (!it || !it->HasNext()) {
    return;
  }

  GetCachedChannelsIfNecessary(base::BindOnce(
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
            ContentSettingsType::NOTIFICATIONS, false /* off_the_record */,
            content_settings::PartitionKey::WipGetDefault()));

    while (it && it->HasNext()) {
      std::unique_ptr<content_settings::Rule> rule = it->Next();
      CreateChannelForRule(*rule);
      patterns.emplace_back(std::move(rule->primary_pattern),
                            std::move(rule->secondary_pattern));
    }
  }

  for (const auto& pattern : patterns) {
    pref_provider->SetWebsiteSetting(
        pattern.first, pattern.second, ContentSettingsType::NOTIFICATIONS,
        base::Value(), {}, content_settings::PartitionKey::WipGetDefault());
  }

  if (pref_service_) {
    pref_service_->SetBoolean(prefs::kMigratedToSiteNotificationChannels, true);
  }
}

void NotificationChannelsProviderAndroid::ClearBlockedChannelsIfNecessary(
    TemplateURLService* template_url_service) {
  if (!pref_service_ || pref_service_->GetBoolean(
                            prefs::kClearedBlockedSiteNotificationChannels)) {
    return;
  }

  url::Origin default_search_engine_origin;
  if (template_url_service) {
    default_search_engine_origin =
        template_url_service->GetDefaultSearchProviderOrigin();
  }
  ScheduleGetChannels(
      /*skip_get_if_cached_channels_are_available=*/false,
      base::BindOnce(&NotificationChannelsProviderAndroid::
                         ClearBlockedChannelsIfNecessaryImpl,
                     weak_factory_.GetWeakPtr(), default_search_engine_origin));
}

void NotificationChannelsProviderAndroid::ClearBlockedChannelsIfNecessaryImpl(
    const url::Origin& default_search_engine_origin,
    const std::vector<NotificationChannel>& channels) {
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

  if (pref_service_) {
    pref_service_->SetBoolean(prefs::kClearedBlockedSiteNotificationChannels,
                              true);
  }
}

void NotificationChannelsProviderAndroid::OnChannelStateChanged(
    const NotificationChannel& channel) {
  if (!cached_channels_) {
    cached_channels_ = std::map<std::string, NotificationChannel>();
  }
  auto iter = cached_channels_->find(channel.origin);

  if (iter != cached_channels_->end()) {
    if (iter->second == channel) {
      return;
    }
    iter->second.status = channel.status;
  } else {
    cached_channels_->emplace(channel.origin, channel);
  }
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&NotificationChannelsProviderAndroid::NotifyObservers,
                     weak_factory_.GetWeakPtr(),
                     ContentSettingsPattern::Wildcard(),
                     ContentSettingsPattern::Wildcard(),
                     ContentSettingsType::NOTIFICATIONS,
                     /*partition_key=*/nullptr));
}

std::unique_ptr<content_settings::RuleIterator>
NotificationChannelsProviderAndroid::GetRuleIterator(
    ContentSettingsType content_type,
    bool off_the_record,
    const content_settings::PartitionKey& partition_key) const {
  if (content_type != ContentSettingsType::NOTIFICATIONS || off_the_record) {
    return nullptr;
  }

  // This const_cast is not ideal but tolerated because it allows us to
  // notify observers as soon as we detect changes to channels.
  auto* provider = const_cast<NotificationChannelsProviderAndroid*>(this);
  provider->RecordCachedChannelStatus();

  // Combine cached_channels_ with pending_channels_ and remove duplicated
  // origins.
  std::map<std::string, NotificationChannel> origin_channel_map;
  std::set<std::string> origins_pending_default_settings;
  for (const auto& pending_channel : pending_channels_) {
    if (pending_channel.second.status ==
        NotificationChannelStatus::UNAVAILABLE) {
      origins_pending_default_settings.emplace(pending_channel.first);
    } else {
      origin_channel_map.emplace(pending_channel.first, pending_channel.second);
    }
  }
  if (cached_channels_) {
    for (const auto& cached_channel : *cached_channels_) {
      if (origins_pending_default_settings.find(cached_channel.first) ==
          origins_pending_default_settings.end()) {
        origin_channel_map.emplace(cached_channel.first, cached_channel.second);
      }
    }
  }

  std::vector<NotificationChannel> channels;
  for (const auto& channel : origin_channel_map) {
    channels.push_back(channel.second);
  }

  // The returned RuleIterator is from cached channels, so it might not
  // contain up-to-date information if user has modified notification settings,
  // As a result, schedule an channel update to inform all observers if
  // something has changed.

  provider->EnsureUpdatedSettings(base::DoNothing());

  return channels.empty()
             ? nullptr
             : std::make_unique<ChannelsRuleIterator>(std::move(channels));
}

void NotificationChannelsProviderAndroid::EnsureUpdatedSettings(
    base::OnceClosure callback) {
  ScheduleGetChannels(
      /*skip_get_if_cached_channels_are_available=*/false,
      base::BindOnce(
          &NotificationChannelsProviderAndroid::UpdateCachedChannelsImpl,
          weak_factory_.GetWeakPtr(),
          /*only_initialize_null_cached_channels=*/false, std::move(callback)));
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
  if (setting != CONTENT_SETTING_DEFAULT && setting != CONTENT_SETTING_ALLOW &&
      setting != CONTENT_SETTING_BLOCK) {
    return false;
  }
  std::string origin_string = GetOriginStringFromPattern(primary_pattern);
  // Create a new pending channel for future GetRuleIterator() call. The
  // new channel should override the previous pending channel for the same
  // origin.
  NotificationChannel channel =
      CreatePendingChannel(origin_string, setting, clock_->Now());
  bool rule_changed = !IsSameAsCachedRule(channel);
  pending_channels_.insert_or_assign(origin_string, channel);
  GetCachedChannelsIfNecessary(base::BindOnce(
      &NotificationChannelsProviderAndroid::UpdateChannelForWebsiteImpl,
      weak_factory_.GetWeakPtr(), primary_pattern, secondary_pattern,
      content_type, setting, constraints.Clone(), channel));
  if (rule_changed) {
    NotifyObservers(primary_pattern, secondary_pattern, content_type,
                    /*partition_key=*/nullptr);
  }

  if (setting == CONTENT_SETTING_DEFAULT) {
    return false;
  }
  value = base::Value();
  return true;
}

bool NotificationChannelsProviderAndroid::IsSameAsCachedRule(
    const NotificationChannel& channel) {
  if (auto iter = pending_channels_.find(channel.origin);
      iter != pending_channels_.end()) {
    return (channel == iter->second);
  } else if (cached_channels_.has_value()) {
    if (auto iter2 = cached_channels_->find(channel.origin);
        iter2 != cached_channels_->end()) {
      return (channel == iter2->second);
    }
  }
  return false;
}

void NotificationChannelsProviderAndroid::UpdateChannelForWebsiteImpl(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    ContentSetting content_setting,
    const content_settings::ContentSettingConstraints& constraints,
    const NotificationChannel& pending_channel) {
  switch (content_setting) {
    case CONTENT_SETTING_ALLOW:
      CreateChannelIfRequired(primary_pattern, secondary_pattern,
                              NotificationChannelStatus::ENABLED);
      break;
    case CONTENT_SETTING_BLOCK:
      CreateChannelIfRequired(primary_pattern, secondary_pattern,
                              NotificationChannelStatus::BLOCKED);
      break;
    case CONTENT_SETTING_DEFAULT: {
      auto channel_to_delete = cached_channels_->find(pending_channel.origin);
      if (channel_to_delete != cached_channels_->end()) {
        bridge_->DeleteChannel(channel_to_delete->second.id);
        cached_channels_->erase(channel_to_delete);
        // The observers should have already been notified when creating the
        // pending channel, thus there is no need to notify them here.
      }
      break;
    }
    default:
      // We rely on notification settings being one of ALLOW/BLOCK/DEFAULT.
      NOTREACHED();
  }
  // If this is the last pending update for this origin, we no longer need to
  // keep it in `pending_channels_` and can move it to `cached_channels_`.
  auto iter = pending_channels_.find(pending_channel.origin);
  DCHECK(iter != pending_channels_.end());
  if (iter->second == pending_channel &&
      iter->second.timestamp == pending_channel.timestamp) {
    pending_channels_.erase(pending_channel.origin);
    // The notification channel in `cached_channels_` may have a wrong status
    // since the actual android notification channel is not recreated.
    // Therefore, it needs to be updated with the status from the
    // pending channel.
    auto channel_iter = cached_channels_->find(pending_channel.origin);
    if (channel_iter != cached_channels_->end()) {
      channel_iter->second.status = pending_channel.status;
    }
  }
}

void NotificationChannelsProviderAndroid::ClearAllContentSettingsRules(
    ContentSettingsType content_type,
    const content_settings::PartitionKey& partition_key) {
  if (content_type != ContentSettingsType::NOTIFICATIONS) {
    return;
  }

  // Create default pending channels for all existing rules.
  bool rule_changed = false;
  std::set<std::string> origins;
  for (const auto& iter : pending_channels_) {
    origins.insert(iter.first);
  }
  if (cached_channels_) {
    for (const auto& iter : *cached_channels_) {
      origins.insert(iter.first);
    }
  }

  base::Time timestamp = clock_->Now();
  for (const std::string& origin : origins) {
    NotificationChannel channel =
        CreatePendingChannel(origin, CONTENT_SETTING_DEFAULT, timestamp);
    rule_changed = rule_changed || !IsSameAsCachedRule(channel);
    pending_channels_.insert_or_assign(origin, channel);
  }

  if (rule_changed) {
    NotifyObservers(ContentSettingsPattern::Wildcard(),
                    ContentSettingsPattern::Wildcard(), content_type,
                    /*partition_key=*/nullptr);
  }

  ScheduleGetChannels(
      /*skip_get_if_cached_channels_are_available=*/false,
      base::BindOnce(&NotificationChannelsProviderAndroid::ClearAllChannelsImpl,
                     weak_factory_.GetWeakPtr(), content_type, timestamp));
}

void NotificationChannelsProviderAndroid::ClearAllChannelsImpl(
    ContentSettingsType content_type,
    base::Time channel_timestamp_at_invocation,
    const std::vector<NotificationChannel>& channels) {
  for (auto channel : channels) {
    bridge_->DeleteChannel(channel.id);
  }

  if (cached_channels_) {
    cached_channels_->clear();
  }

  // Clean up pending channels that hasn't changed since the
  // ClearAllContentSettingsRules() calls.
  for (auto iter = pending_channels_.begin();
       iter != pending_channels_.end();) {
    if (iter->second.status == NotificationChannelStatus::UNAVAILABLE &&
        iter->second.timestamp == channel_timestamp_at_invocation) {
      iter = pending_channels_.erase(iter);
    } else {
      ++iter;
    }
  }
}

void NotificationChannelsProviderAndroid::ShutdownOnUIThread() {
  pref_service_ = nullptr;
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

// GetCachedChannelsIfNecessary() must be called prior to calling this method.
void NotificationChannelsProviderAndroid::CreateChannelIfRequired(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    NotificationChannelStatus new_channel_status) {
  const std::string origin_string = GetOriginStringFromPattern(primary_pattern);
  auto channel_entry = cached_channels_->find(origin_string);
  if (channel_entry == cached_channels_->end()) {
    base::Time timestamp = clock_->Now();

    NotificationChannel channel = bridge_->CreateChannel(
        origin_string, timestamp,
        new_channel_status == NotificationChannelStatus::ENABLED);
    cached_channels_->emplace(origin_string, std::move(channel));

    // The observers should have been notified when the pending channel was
    // created, so there is no need to notify them.
  }
}

// GetCachedChannelsIfNecessary() must be called prior to calling this method.
void NotificationChannelsProviderAndroid::CreateChannelForRule(
    const content_settings::Rule& rule) {
  ContentSetting content_setting =
      content_settings::ValueToContentSetting(rule.value);
  switch (content_setting) {
    case CONTENT_SETTING_ALLOW:
      CreateChannelIfRequired(rule.primary_pattern, rule.secondary_pattern,
                              NotificationChannelStatus::ENABLED);
      break;
    case CONTENT_SETTING_BLOCK:
      CreateChannelIfRequired(rule.primary_pattern, rule.secondary_pattern,
                              NotificationChannelStatus::BLOCKED);
      break;
    default:
      // We assume notification preferences are either ALLOW/BLOCK.
      NOTREACHED();
  }
}

// This method must be called prior to accessing |cached_channels_|.
void NotificationChannelsProviderAndroid::GetCachedChannelsIfNecessary(
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
    if (!cached_channels_ || updated_channels_map != cached_channels_.value()) {
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
