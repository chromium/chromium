// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/safety_list_manager.h"

#include <optional>
#include <string_view>

#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_util.h"
#include "chrome/browser/actor/safety_list.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/host_indexed_content_settings.h"

namespace actor {

namespace {

constexpr std::string_view kAllowedFieldName = "navigation_allowed";
constexpr std::string_view kBlockedFieldName = "navigation_blocked";

constexpr std::string_view kAllowedHistogramName =
    "Actor.SafetyListParseResult.NavigationAllowed";
constexpr std::string_view kBlockedHistogramName =
    "Actor.SafetyListParseResult.NavigationBlocked";

void SetAll(base::span<const SafetyListEntry> entries,
            ContentSetting setting,
            content_settings::HostIndexedContentSettings& indexed_settings) {
  const base::Value setting_value =
      content_settings::ContentSettingToValue(setting);
  for (const auto& entry : entries) {
    indexed_settings.SetValue(entry.source, entry.destination,
                              setting_value.Clone(), {});
  }
}

void MaybeSetHardcodedEntries(
    content_settings::HostIndexedContentSettings& indexed_settings) {
  if (IsNavigationGatingEnabled() &&
      kGlicIncludeHardcodedBlockListEntries.Get()) {
    SetAll(
        {
            {
                ContentSettingsPattern::FromString("*"),
                ContentSettingsPattern::FromString("[*.]googleplex.com"),
            },
            {
                ContentSettingsPattern::FromString("*"),
                ContentSettingsPattern::FromString("[*.]corp.google.com"),
            },
        },
        ContentSetting::CONTENT_SETTING_BLOCK, indexed_settings);
  }
}

}  // namespace

// static
SafetyListManager* SafetyListManager::GetInstance() {
  static base::NoDestructor<SafetyListManager> instance;
  return instance.get();
}

// static
SafetyListManager SafetyListManager::CreateForTesting() {
  return SafetyListManager();
}

SafetyListManager::Decision SafetyListManager::Find(
    const GURL& source,
    const GURL& destination) const {
  const content_settings::RuleEntry* rule_entry =
      host_indexed_content_settings_.Find(source, destination);

  if (!rule_entry) {
    return Decision::kNone;
  }
  ContentSetting setting =
      content_settings::ParseContentSettingValue(rule_entry->second.value)
          .value();
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
      return Decision::kAllow;
    case CONTENT_SETTING_BLOCK:
      return Decision::kBlock;
    case CONTENT_SETTING_DEFAULT:
    case CONTENT_SETTING_ASK:
    case CONTENT_SETTING_SESSION_ONLY:
    case CONTENT_SETTING_NUM_SETTINGS:
      NOTREACHED();
  }
  NOTREACHED();
}

SafetyListManager::SafetyListManager() {
  MaybeSetHardcodedEntries(host_indexed_content_settings_);
}
SafetyListManager::~SafetyListManager() = default;

SafetyListManager::ParseStatus SafetyListManager::ParseSafetyListsInternal(
    std::string_view json_string) {
  std::optional<base::Value> json =
      base::JSONReader::Read(json_string, base::JSON_PARSE_RFC);
  if (!json.has_value()) {
    return {SafetyListParseResult::kInvalidJson,
            SafetyListParseResult::kInvalidJson};
  }

  base::DictValue* json_dict = json->GetIfDict();
  if (!json_dict) {
    return {SafetyListParseResult::kInvalidJson,
            SafetyListParseResult::kInvalidJson};
  }

  auto parse_one_list = [&json_dict](std::string_view field_name)
      -> base::expected<SafetyList, SafetyListParseResult> {
    if (const base::Value* value = json_dict->Find(field_name)) {
      if (const base::ListValue* list = value->GetIfList()) {
        return SafetyList::ParseEntriesFromJson(*list);
      }
      return base::unexpected(SafetyListParseResult::kJsonKeyValueNotAList);
    }
    return SafetyList();
  };

  base::expected<SafetyList, SafetyListParseResult> allowed_result =
      parse_one_list(kAllowedFieldName);
  base::expected<SafetyList, SafetyListParseResult> blocked_result =
      parse_one_list(kBlockedFieldName);
  if (allowed_result.has_value() || blocked_result.has_value()) {
    host_indexed_content_settings_.Clear();
    if (allowed_result.has_value()) {
      SetAll(allowed_result->entries(), ContentSetting::CONTENT_SETTING_ALLOW,
             host_indexed_content_settings_);
    }
    if (blocked_result.has_value()) {
      SetAll(blocked_result->entries(), ContentSetting::CONTENT_SETTING_BLOCK,
             host_indexed_content_settings_);
    }
    MaybeSetHardcodedEntries(host_indexed_content_settings_);
  }

  return {allowed_result.error_or(SafetyListParseResult::kSuccess),
          blocked_result.error_or(SafetyListParseResult::kSuccess)};
}

void SafetyListManager::ParseSafetyLists(std::string_view json_string) {
  ParseStatus status = ParseSafetyListsInternal(json_string);

  base::UmaHistogramEnumeration(kAllowedHistogramName, status.allowed_result);
  base::UmaHistogramEnumeration(kBlockedHistogramName, status.blocked_result);
}

}  // namespace actor
