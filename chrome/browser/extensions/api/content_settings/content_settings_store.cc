// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/content_settings/content_settings_store.h"

#include <algorithm>
#include <memory>
#include <set>
#include <utility>

#include "base/debug/alias.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_api_constants.h"
#include "chrome/browser/extensions/api/content_settings/content_settings_helpers.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_origin_identifier_value_map.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;
using content_settings::ConcatenationIterator;
using content_settings::Rule;
using content_settings::RuleIterator;
using content_settings::OriginIdentifierValueMap;
using content_settings::ResourceIdentifier;

namespace extensions {

struct ContentSettingsStore::ExtensionEntry {
  // Extension id.
  std::string id;
  // Installation time.
  base::Time install_time;
  // Whether extension is enabled in the profile.
  bool enabled;
  // Content settings.
  OriginIdentifierValueMap settings;
  // Persistent incognito content settings.
  OriginIdentifierValueMap incognito_persistent_settings;
  // Session-only incognito content settings.
  OriginIdentifierValueMap incognito_session_only_settings;
};

ContentSettingsStore::ContentSettingsStore() {
  DCHECK(OnCorrectThread());
}

ContentSettingsStore::~ContentSettingsStore() {
}

std::unique_ptr<RuleIterator> ContentSettingsStore::GetRuleIterator(
    ContentSettingsType type,
    const content_settings::ResourceIdentifier& identifier,
    bool incognito) const {
  std::vector<std::unique_ptr<RuleIterator>> iterators;

  // The individual |RuleIterators| shouldn't lock; pass |lock_| to the
  // |ConcatenationIterator| in a locked state.
  std::unique_ptr<base::AutoLock> auto_lock(new base::AutoLock(lock_));

  // Iterate the extensions based on install time (most-recently installed
  // items first).
  for (const auto& entry : entries_) {
    if (!entry->enabled)
      continue;

    std::unique_ptr<RuleIterator> rule_it;
    if (incognito) {
      rule_it = entry->incognito_session_only_settings.GetRuleIterator(
          type, identifier, nullptr);
      if (rule_it)
        iterators.push_back(std::move(rule_it));
      rule_it = entry->incognito_persistent_settings.GetRuleIterator(
          type, identifier, nullptr);
      if (rule_it)
        iterators.push_back(std::move(rule_it));
    } else {
      rule_it = entry->settings.GetRuleIterator(type, identifier, nullptr);
      if (rule_it)
        iterators.push_back(std::move(rule_it));
    }
  }
  if (iterators.empty())
    return nullptr;

  return std::make_unique<ConcatenationIterator>(std::move(iterators),
                                                 auto_lock.release());
}

void ContentSettingsStore::SetExtensionContentSetting(
    const std::string& ext_id,
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType type,
    const content_settings::ResourceIdentifier& identifier,
    ContentSetting setting,
    ExtensionPrefsScope scope) {
  {
    base::AutoLock lock(lock_);
    OriginIdentifierValueMap* map = GetValueMap(ext_id, scope);
    if (setting == CONTENT_SETTING_DEFAULT) {
      map->DeleteValue(primary_pattern, secondary_pattern, type, identifier);
    } else {
      // Do not set a timestamp for extension settings.
      map->SetValue(primary_pattern, secondary_pattern, type, identifier,
                    base::Time(), base::Value(setting));
    }
  }

  // Send notification that content settings changed. (Note: This is responsible
  // for updating the pref store, so cannot be skipped even if the setting would
  // be masked by another extension.)
  NotifyOfContentSettingChanged(ext_id,
                                scope != kExtensionPrefsScopeRegular);
}

void ContentSettingsStore::RegisterExtension(
    const std::string& ext_id,
    const base::Time& install_time,
    bool is_enabled) {
  base::AutoLock lock(lock_);
  auto i = FindIterator(ext_id);
  ExtensionEntry* entry = nullptr;
  if (i != entries_.end()) {
    entry = i->get();
  } else {
    entry = new ExtensionEntry;
    entry->install_time = install_time;

    // Insert in reverse-chronological order to maintain the invariant.
    auto unique_entry = base::WrapUnique(entry);
    auto location =
        std::upper_bound(entries_.begin(), entries_.end(), unique_entry,
                         [](const std::unique_ptr<ExtensionEntry>& a,
                            const std::unique_ptr<ExtensionEntry>& b) {
                           return a->install_time > b->install_time;
                         });
    entries_.insert(location, std::move(unique_entry));
  }

  entry->id = ext_id;
  entry->enabled = is_enabled;
}

void ContentSettingsStore::UnregisterExtension(
    const std::string& ext_id) {
  bool notify = false;
  bool notify_incognito = false;
  {
    base::AutoLock lock(lock_);
    auto i = FindIterator(ext_id);
    if (i == entries_.end())
      return;
    notify = !(*i)->settings.empty();
    notify_incognito = !(*i)->incognito_persistent_settings.empty() ||
                       !(*i)->incognito_session_only_settings.empty();

    entries_.erase(i);
  }
  if (notify)
    NotifyOfContentSettingChanged(ext_id, false);
  if (notify_incognito)
    NotifyOfContentSettingChanged(ext_id, true);
}

void ContentSettingsStore::SetExtensionState(
    const std::string& ext_id, bool is_enabled) {
  bool notify = false;
  bool notify_incognito = false;
  {
    base::AutoLock lock(lock_);
    ExtensionEntry* entry = FindEntry(ext_id);
    if (!entry)
      return;

    notify = !entry->settings.empty();
    notify_incognito = !entry->incognito_persistent_settings.empty() ||
                       !entry->incognito_session_only_settings.empty();

    entry->enabled = is_enabled;
  }
  if (notify)
    NotifyOfContentSettingChanged(ext_id, false);
  if (notify_incognito)
    NotifyOfContentSettingChanged(ext_id, true);
}

OriginIdentifierValueMap* ContentSettingsStore::GetValueMap(
    const std::string& ext_id,
    ExtensionPrefsScope scope) {
  const OriginIdentifierValueMap* result =
      static_cast<const ContentSettingsStore*>(this)->GetValueMap(ext_id,
                                                                  scope);
  return const_cast<OriginIdentifierValueMap*>(result);
}

const OriginIdentifierValueMap* ContentSettingsStore::GetValueMap(
    const std::string& ext_id,
    ExtensionPrefsScope scope) const {
  ExtensionEntry* entry = FindEntry(ext_id);
  if (!entry)
    return nullptr;

  switch (scope) {
    case kExtensionPrefsScopeRegular:
      return &(entry->settings);
    case kExtensionPrefsScopeRegularOnly:
      // TODO(bauerb): Implement regular-only content settings.
      NOTREACHED();
      return nullptr;
    case kExtensionPrefsScopeIncognitoPersistent:
      return &(entry->incognito_persistent_settings);
    case kExtensionPrefsScopeIncognitoSessionOnly:
      return &(entry->incognito_session_only_settings);
  }

  NOTREACHED();
  return nullptr;
}

void ContentSettingsStore::ClearContentSettingsForExtension(
    const std::string& ext_id,
    ExtensionPrefsScope scope) {
  bool notify = false;
  {
    base::AutoLock lock(lock_);
    OriginIdentifierValueMap* map = GetValueMap(ext_id, scope);
    DCHECK(map);
    notify = !map->empty();
    map->clear();
  }
  if (notify) {
    NotifyOfContentSettingChanged(ext_id, scope != kExtensionPrefsScopeRegular);
  }
}

void ContentSettingsStore::ClearContentSettingsForExtensionAndContentType(
    const std::string& ext_id,
    ExtensionPrefsScope scope,
    ContentSettingsType content_type) {
  bool notify = false;
  {
    base::AutoLock lock(lock_);
    OriginIdentifierValueMap* map = GetValueMap(ext_id, scope);
    DCHECK(map);

    // Get all of the resource identifiers for this |content_type|.
    std::set<ResourceIdentifier> resource_identifiers;
    for (const auto& entry : *map) {
      if (entry.first.content_type == content_type)
        resource_identifiers.insert(entry.first.resource_identifier);
    }

    notify = !resource_identifiers.empty();

    for (const ResourceIdentifier& resource_identifier : resource_identifiers)
      map->DeleteValues(content_type, resource_identifier);
  }
  if (notify) {
    NotifyOfContentSettingChanged(ext_id, scope != kExtensionPrefsScopeRegular);
  }
}

std::unique_ptr<base::ListValue> ContentSettingsStore::GetSettingsForExtension(
    const std::string& extension_id,
    ExtensionPrefsScope scope) const {
  base::AutoLock lock(lock_);
  const OriginIdentifierValueMap* map = GetValueMap(extension_id, scope);
  if (!map)
    return nullptr;

  auto settings = std::make_unique<base::ListValue>();
  for (const auto& it : *map) {
    const auto& key = it.first;
    std::unique_ptr<RuleIterator> rule_iterator(
        map->GetRuleIterator(key.content_type, key.resource_identifier,
                             nullptr));  // We already hold the lock.
    if (!rule_iterator)
      continue;

    while (rule_iterator->HasNext()) {
      const Rule& rule = rule_iterator->Next();
      std::unique_ptr<base::DictionaryValue> setting_dict(
          new base::DictionaryValue());
      setting_dict->SetString(
          content_settings_api_constants::kPrimaryPatternKey,
          rule.primary_pattern.ToString());
      setting_dict->SetString(
          content_settings_api_constants::kSecondaryPatternKey,
          rule.secondary_pattern.ToString());
      setting_dict->SetString(
          content_settings_api_constants::kContentSettingsTypeKey,
          content_settings_helpers::ContentSettingsTypeToString(
              key.content_type));
      setting_dict->SetString(
          content_settings_api_constants::kResourceIdentifierKey,
          key.resource_identifier);
      ContentSetting content_setting =
          content_settings::ValueToContentSetting(&rule.value);
      DCHECK_NE(CONTENT_SETTING_DEFAULT, content_setting);

      std::string setting_string =
          content_settings::ContentSettingToString(content_setting);
      DCHECK(!setting_string.empty());

      setting_dict->SetString(
          content_settings_api_constants::kContentSettingKey, setting_string);
      settings->Append(std::move(setting_dict));
    }
  }
  return settings;
}

void ContentSettingsStore::SetExtensionContentSettingFromList(
    const std::string& extension_id,
    const base::ListValue* list,
    ExtensionPrefsScope scope) {
  for (const auto& value : *list) {
    const base::DictionaryValue* dict = nullptr;
    if (!value.GetAsDictionary(&dict)) {
      NOTREACHED();
      continue;
    }
    std::string primary_pattern_str;
    dict->GetString(content_settings_api_constants::kPrimaryPatternKey,
                    &primary_pattern_str);
    ContentSettingsPattern primary_pattern =
        ContentSettingsPattern::FromString(primary_pattern_str);
    DCHECK(primary_pattern.IsValid());

    std::string secondary_pattern_str;
    dict->GetString(content_settings_api_constants::kSecondaryPatternKey,
                    &secondary_pattern_str);
    ContentSettingsPattern secondary_pattern =
        ContentSettingsPattern::FromString(secondary_pattern_str);
    DCHECK(secondary_pattern.IsValid());

    std::string content_settings_type_str;
    dict->GetString(content_settings_api_constants::kContentSettingsTypeKey,
                    &content_settings_type_str);
    ContentSettingsType content_settings_type =
        content_settings_helpers::StringToContentSettingsType(
            content_settings_type_str);
    if (content_settings_type == ContentSettingsType::DEFAULT) {
      // We'll end up with DEFAULT here if the type string isn't recognised.
      // This could be if it's a string from an old settings type that has been
      // deleted. DCHECK to make sure this is the case (not some random string).
      DCHECK(content_settings_type_str == "fullscreen" ||
             content_settings_type_str == "mouselock");

      // In this case, we just skip over that setting, effectively deleting it
      // from the in-memory model. This will implicitly delete these old
      // settings from the pref store when it is written back.
      continue;
    }

    const content_settings::ContentSettingsInfo* info =
        content_settings::ContentSettingsRegistry::GetInstance()->Get(
            content_settings_type);
    if (primary_pattern != secondary_pattern &&
        secondary_pattern != ContentSettingsPattern::Wildcard() &&
        !info->website_settings_info()->SupportsEmbeddedExceptions() &&
        base::FeatureList::IsEnabled(::features::kPermissionDelegation)) {
      // Some types may have had embedded exceptions written even though they
      // aren't supported. This will implicitly delete these old settings from
      // the pref store when it is written back.
      continue;
    }

    std::string resource_identifier;
    dict->GetString(content_settings_api_constants::kResourceIdentifierKey,
                    &resource_identifier);

    std::string content_setting_string;
    dict->GetString(content_settings_api_constants::kContentSettingKey,
                    &content_setting_string);
    ContentSetting setting;
    bool result = content_settings::ContentSettingFromString(
        content_setting_string, &setting);
    DCHECK(result);
    // The content settings extensions API does not support setting any content
    // settings to |CONTENT_SETTING_DEFAULT|.
    DCHECK_NE(CONTENT_SETTING_DEFAULT, setting);

    SetExtensionContentSetting(extension_id,
                               primary_pattern,
                               secondary_pattern,
                               content_settings_type,
                               resource_identifier,
                               setting,
                               scope);
  }
}

void ContentSettingsStore::AddObserver(Observer* observer) {
  DCHECK(OnCorrectThread());
  observers_.AddObserver(observer);
}

void ContentSettingsStore::RemoveObserver(Observer* observer) {
  DCHECK(OnCorrectThread());
  observers_.RemoveObserver(observer);
}

void ContentSettingsStore::NotifyOfContentSettingChanged(
    const std::string& extension_id,
    bool incognito) {
  for (auto& observer : observers_)
    observer.OnContentSettingChanged(extension_id, incognito);
}

bool ContentSettingsStore::OnCorrectThread() {
  // If there is no UI thread, we're most likely in a unit test.
  return !BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI);
}

ContentSettingsStore::ExtensionEntry* ContentSettingsStore::FindEntry(
    const std::string& ext_id) const {
  auto iter =
      std::find_if(entries_.begin(), entries_.end(),
                   [ext_id](const std::unique_ptr<ExtensionEntry>& entry) {
                     return entry->id == ext_id;
                   });
  return iter == entries_.end() ? nullptr : iter->get();
}

ContentSettingsStore::ExtensionEntries::iterator
ContentSettingsStore::FindIterator(const std::string& ext_id) {
  return std::find_if(entries_.begin(), entries_.end(),
                      [ext_id](const std::unique_ptr<ExtensionEntry>& entry) {
                        return entry->id == ext_id;
                      });
}

}  // namespace extensions
