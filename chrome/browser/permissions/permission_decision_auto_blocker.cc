// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_decision_auto_blocker.h"

#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/variations/variations_associated_data.h"
#include "url/gurl.h"

namespace {

constexpr int kDefaultDismissalsBeforeBlock = 3;
constexpr int kDefaultIgnoresBeforeBlock = 4;
constexpr int kDefaultEmbargoDays = 7;

// The number of times that users may explicitly dismiss a permission prompt
// from an origin before it is automatically blocked.
int g_dismissals_before_block = kDefaultDismissalsBeforeBlock;

// The number of times that users may ignore a permission prompt from an origin
// before it is automatically blocked.
int g_ignores_before_block = kDefaultIgnoresBeforeBlock;

// The number of days that an origin will stay under embargo for a requested
// permission due to repeated dismissals.
int g_dismissal_embargo_days = kDefaultEmbargoDays;

// The number of days that an origin will stay under embargo for a requested
// permission due to repeated ignores.
int g_ignore_embargo_days = kDefaultEmbargoDays;

std::unique_ptr<base::DictionaryValue> GetOriginAutoBlockerData(
    HostContentSettingsMap* settings,
    const GURL& origin_url) {
  std::unique_ptr<base::DictionaryValue> dict =
      base::DictionaryValue::From(settings->GetWebsiteSetting(
          origin_url, GURL(), ContentSettingsType::PERMISSION_AUTOBLOCKER_DATA,
          std::string(), nullptr));
  if (!dict)
    return std::make_unique<base::DictionaryValue>();

  return dict;
}

base::Value* GetOrCreatePermissionDict(base::Value* origin_dict,
                                       const std::string& permission) {
  base::Value* permission_dict =
      origin_dict->FindKeyOfType(permission, base::Value::Type::DICTIONARY);
  if (permission_dict)
    return permission_dict;
  return origin_dict->SetKey(permission,
                             base::Value(base::Value::Type::DICTIONARY));
}

int RecordActionInWebsiteSettings(const GURL& url,
                                  ContentSettingsType permission,
                                  const char* key,
                                  Profile* profile) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  std::unique_ptr<base::DictionaryValue> dict =
      GetOriginAutoBlockerData(map, url);

  base::Value* permission_dict = GetOrCreatePermissionDict(
      dict.get(), PermissionUtil::GetPermissionString(permission));

  base::Value* value =
      permission_dict->FindKeyOfType(key, base::Value::Type::INTEGER);
  int current_count = value ? value->GetInt() : 0;
  permission_dict->SetKey(key, base::Value(++current_count));

  map->SetWebsiteSettingDefaultScope(
      url, GURL(), ContentSettingsType::PERMISSION_AUTOBLOCKER_DATA,
      std::string(), std::move(dict));

  return current_count;
}

int GetActionCount(const GURL& url,
                   ContentSettingsType permission,
                   const char* key,
                   Profile* profile) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  std::unique_ptr<base::DictionaryValue> dict =
      GetOriginAutoBlockerData(map, url);
  base::Value* permission_dict = GetOrCreatePermissionDict(
      dict.get(), PermissionUtil::GetPermissionString(permission));

  base::Value* value =
      permission_dict->FindKeyOfType(key, base::Value::Type::INTEGER);
  return value ? value->GetInt() : 0;
}

bool IsUnderEmbargo(base::Value* permission_dict,
                    const base::Feature& feature,
                    const char* key,
                    base::Time current_time,
                    base::TimeDelta offset) {
  base::Value* found =
      permission_dict->FindKeyOfType(key, base::Value::Type::DOUBLE);
  if (found && base::FeatureList::IsEnabled(feature) &&
      current_time <
          base::Time::FromInternalValue(found->GetDouble()) + offset) {
    return true;
  }

  return false;
}

void UpdateValueFromVariation(const std::string& variation_value,
                              int* value_store,
                              const int default_value) {
  int tmp_value = -1;
  if (base::StringToInt(variation_value, &tmp_value) && tmp_value > 0)
    *value_store = tmp_value;
  else
    *value_store = default_value;
}

}  // namespace

// PermissionDecisionAutoBlocker::Factory --------------------------------------

// static
PermissionDecisionAutoBlocker*
PermissionDecisionAutoBlocker::Factory::GetForProfile(Profile* profile) {
  return static_cast<PermissionDecisionAutoBlocker*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
PermissionDecisionAutoBlocker::Factory*
PermissionDecisionAutoBlocker::Factory::GetInstance() {
  return base::Singleton<PermissionDecisionAutoBlocker::Factory>::get();
}

PermissionDecisionAutoBlocker::Factory::Factory()
    : BrowserContextKeyedServiceFactory(
          "PermissionDecisionAutoBlocker",
          BrowserContextDependencyManager::GetInstance()) {}

PermissionDecisionAutoBlocker::Factory::~Factory() {}

KeyedService* PermissionDecisionAutoBlocker::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  return new PermissionDecisionAutoBlocker(profile);
}

content::BrowserContext*
PermissionDecisionAutoBlocker::Factory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

// PermissionDecisionAutoBlocker -----------------------------------------------

// static
const char PermissionDecisionAutoBlocker::kPromptDismissCountKey[] =
    "dismiss_count";

// static
const char PermissionDecisionAutoBlocker::kPromptIgnoreCountKey[] =
    "ignore_count";

// static
const char PermissionDecisionAutoBlocker::kPermissionDismissalEmbargoKey[] =
    "dismissal_embargo_days";

// static
const char PermissionDecisionAutoBlocker::kPermissionIgnoreEmbargoKey[] =
    "ignore_embargo_days";

// static
PermissionDecisionAutoBlocker* PermissionDecisionAutoBlocker::GetForProfile(
    Profile* profile) {
  return PermissionDecisionAutoBlocker::Factory::GetForProfile(profile);
}

// static
PermissionResult PermissionDecisionAutoBlocker::GetEmbargoResult(
    HostContentSettingsMap* settings_map,
    const GURL& request_origin,
    ContentSettingsType permission,
    base::Time current_time) {
  DCHECK(settings_map);
  std::unique_ptr<base::DictionaryValue> dict =
      GetOriginAutoBlockerData(settings_map, request_origin);
  base::Value* permission_dict = GetOrCreatePermissionDict(
      dict.get(), PermissionUtil::GetPermissionString(permission));

  if (IsUnderEmbargo(permission_dict, features::kBlockPromptsIfDismissedOften,
                     kPermissionDismissalEmbargoKey, current_time,
                     base::TimeDelta::FromDays(g_dismissal_embargo_days))) {
    return PermissionResult(CONTENT_SETTING_BLOCK,
                            PermissionStatusSource::MULTIPLE_DISMISSALS);
  }

  if (IsUnderEmbargo(permission_dict, features::kBlockPromptsIfIgnoredOften,
                     kPermissionIgnoreEmbargoKey, current_time,
                     base::TimeDelta::FromDays(g_ignore_embargo_days))) {
    return PermissionResult(CONTENT_SETTING_BLOCK,
                            PermissionStatusSource::MULTIPLE_IGNORES);
  }

  return PermissionResult(CONTENT_SETTING_ASK,
                          PermissionStatusSource::UNSPECIFIED);
}

// static
void PermissionDecisionAutoBlocker::UpdateFromVariations() {
  std::string dismissals_before_block_value =
      variations::GetVariationParamValueByFeature(
          features::kBlockPromptsIfDismissedOften, kPromptDismissCountKey);
  std::string ignores_before_block_value =
      variations::GetVariationParamValueByFeature(
          features::kBlockPromptsIfIgnoredOften, kPromptIgnoreCountKey);
  std::string dismissal_embargo_days_value =
      variations::GetVariationParamValueByFeature(
          features::kBlockPromptsIfDismissedOften,
          kPermissionDismissalEmbargoKey);
  std::string ignore_embargo_days_value =
      variations::GetVariationParamValueByFeature(
          features::kBlockPromptsIfIgnoredOften, kPermissionIgnoreEmbargoKey);

  // If converting the value fails, revert to the original value.
  UpdateValueFromVariation(dismissals_before_block_value,
                           &g_dismissals_before_block,
                           kDefaultDismissalsBeforeBlock);
  UpdateValueFromVariation(ignores_before_block_value, &g_ignores_before_block,
                           kDefaultIgnoresBeforeBlock);
  UpdateValueFromVariation(dismissal_embargo_days_value,
                           &g_dismissal_embargo_days, kDefaultEmbargoDays);
  UpdateValueFromVariation(ignore_embargo_days_value, &g_ignore_embargo_days,
                           kDefaultEmbargoDays);
}

PermissionResult PermissionDecisionAutoBlocker::GetEmbargoResult(
    const GURL& request_origin,
    ContentSettingsType permission) {
  return GetEmbargoResult(
      HostContentSettingsMapFactory::GetForProfile(profile_), request_origin,
      permission, clock_->Now());
}

int PermissionDecisionAutoBlocker::GetDismissCount(
    const GURL& url,
    ContentSettingsType permission) {
  return GetActionCount(url, permission, kPromptDismissCountKey, profile_);
}

int PermissionDecisionAutoBlocker::GetIgnoreCount(
    const GURL& url,
    ContentSettingsType permission) {
  return GetActionCount(url, permission, kPromptIgnoreCountKey, profile_);
}

bool PermissionDecisionAutoBlocker::RecordDismissAndEmbargo(
    const GURL& url,
    ContentSettingsType permission) {
  int current_dismissal_count = RecordActionInWebsiteSettings(
      url, permission, kPromptDismissCountKey, profile_);

  // TODO(dominickn): ideally we would have a method
  // PermissionContextBase::ShouldEmbargoAfterRepeatedDismissals() to specify
  // if a permission is opted in. This is difficult right now because:
  // 1. PermissionQueueController needs to call this method at a point where it
  //    does not have a PermissionContextBase available
  // 2. Not calling RecordDismissAndEmbargo means no repeated dismissal metrics
  //    are recorded
  // For now, only plugins are explicitly opted out. We should think about how
  // to make this nicer once PermissionQueueController is removed.
  if (base::FeatureList::IsEnabled(features::kBlockPromptsIfDismissedOften) &&
      permission != ContentSettingsType::PLUGINS &&
      current_dismissal_count >= g_dismissals_before_block) {
    PlaceUnderEmbargo(url, permission, kPermissionDismissalEmbargoKey);
    return true;
  }
  return false;
}

bool PermissionDecisionAutoBlocker::RecordIgnoreAndEmbargo(
    const GURL& url,
    ContentSettingsType permission) {
  int current_ignore_count = RecordActionInWebsiteSettings(
      url, permission, kPromptIgnoreCountKey, profile_);

  if (base::FeatureList::IsEnabled(features::kBlockPromptsIfIgnoredOften) &&
      permission != ContentSettingsType::PLUGINS &&
      current_ignore_count >= g_ignores_before_block) {
    PlaceUnderEmbargo(url, permission, kPermissionIgnoreEmbargoKey);
    return true;
  }
  return false;
}

void PermissionDecisionAutoBlocker::RemoveEmbargoByUrl(
    const GURL& url,
    ContentSettingsType permission) {
  if (!PermissionUtil::IsPermission(permission))
    return;

  // Don't proceed if |permission| was not under embargo for |url|.
  PermissionResult result = GetEmbargoResult(url, permission);
  if (result.source != PermissionStatusSource::MULTIPLE_DISMISSALS)
    return;

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  std::unique_ptr<base::DictionaryValue> dict =
      GetOriginAutoBlockerData(map, url);
  base::Value* permission_dict = GetOrCreatePermissionDict(
      dict.get(), PermissionUtil::GetPermissionString(permission));

  const bool dismissal_key_deleted =
      permission_dict->RemoveKey(kPermissionDismissalEmbargoKey);
  DCHECK(dismissal_key_deleted);

  map->SetWebsiteSettingDefaultScope(
      url, GURL(), ContentSettingsType::PERMISSION_AUTOBLOCKER_DATA,
      std::string(), std::move(dict));
}

void PermissionDecisionAutoBlocker::RemoveCountsByUrl(
    base::Callback<bool(const GURL& url)> filter) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);

  std::unique_ptr<ContentSettingsForOneType> settings(
      new ContentSettingsForOneType);
  map->GetSettingsForOneType(ContentSettingsType::PERMISSION_AUTOBLOCKER_DATA,
                             std::string(), settings.get());

  for (const auto& site : *settings) {
    GURL origin(site.primary_pattern.ToString());

    if (origin.is_valid() && filter.Run(origin)) {
      map->SetWebsiteSettingDefaultScope(
          origin, GURL(), ContentSettingsType::PERMISSION_AUTOBLOCKER_DATA,
          std::string(), nullptr);
    }
  }
}

PermissionDecisionAutoBlocker::PermissionDecisionAutoBlocker(Profile* profile)
    : profile_(profile), clock_(base::DefaultClock::GetInstance()) {}

PermissionDecisionAutoBlocker::~PermissionDecisionAutoBlocker() {}

void PermissionDecisionAutoBlocker::PlaceUnderEmbargo(
    const GURL& request_origin,
    ContentSettingsType permission,
    const char* key) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  std::unique_ptr<base::DictionaryValue> dict =
      GetOriginAutoBlockerData(map, request_origin);
  base::Value* permission_dict = GetOrCreatePermissionDict(
      dict.get(), PermissionUtil::GetPermissionString(permission));
  permission_dict->SetKey(
      key, base::Value(static_cast<double>(clock_->Now().ToInternalValue())));
  map->SetWebsiteSettingDefaultScope(
      request_origin, GURL(), ContentSettingsType::PERMISSION_AUTOBLOCKER_DATA,
      std::string(), std::move(dict));
}

void PermissionDecisionAutoBlocker::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}
