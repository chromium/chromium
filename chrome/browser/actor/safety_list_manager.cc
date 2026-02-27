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

struct SafetyListEntry {
  ContentSettingsPattern source;
  ContentSettingsPattern destination;
};

// Returns a span of elements from an expected vector. If the `expected` is an
// error, then the result span is empty.
//
// This returns a span that references `elts`, so the span is only valid as long
// as `elts` is valid.
base::span<const SafetyListEntry> SpanOverExpected(
    const base::expected<std::vector<SafetyListEntry>,
                         SafetyListManager::ParseResult>& elts) {
  return elts.has_value() ? *elts : base::span<const SafetyListEntry>();
}

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

// Parses a list of entries from a JSON list. Returns the parsed vector on
// success, or a ParseResult on failure. If the result is a ParseResult, the
// enum value is guaranteed to not be `kSuccess`.
base::expected<std::vector<SafetyListEntry>, SafetyListManager::ParseResult>
ParseEntriesFromJson(const base::ListValue& list_data) {
  std::vector<SafetyListEntry> entries;
  entries.reserve(list_data.size());
  for (const auto& navigation : list_data) {
    const base::DictValue* navigation_dict = navigation.GetIfDict();
    if (!navigation_dict) {
      return base::unexpected(
          SafetyListManager::ParseResult::kJsonListValueNotADictionary);
    }

    // Only parse entry if both fields exist.
    const std::string* from = navigation_dict->FindString("from");
    if (!from) {
      return base::unexpected(
          SafetyListManager::ParseResult::kInvalidFromField);
    }
    ContentSettingsPattern source = ContentSettingsPattern::FromString(*from);
    if (!source.IsValid()) {
      return base::unexpected(
          SafetyListManager::ParseResult::kInvalidFromUrlPattern);
    }

    const std::string* to = navigation_dict->FindString("to");
    if (!to) {
      return base::unexpected(SafetyListManager::ParseResult::kInvalidToField);
    }
    ContentSettingsPattern destination =
        ContentSettingsPattern::FromString(*to);
    if (!destination.IsValid()) {
      return base::unexpected(
          SafetyListManager::ParseResult::kInvalidToUrlPattern);
    }
    entries.push_back(
        SafetyListEntry{std::move(source), std::move(destination)});
  }
  return entries;
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
    return {ParseResult::kInvalidJson, ParseResult::kInvalidJson};
  }

  base::DictValue* json_dict = json->GetIfDict();
  if (!json_dict) {
    return {ParseResult::kInvalidJson, ParseResult::kInvalidJson};
  }

  auto parse_one_list = [&json_dict](std::string_view field_name)
      -> base::expected<std::vector<SafetyListEntry>,
                        SafetyListManager::ParseResult> {
    if (const base::Value* value = json_dict->Find(field_name)) {
      if (const base::ListValue* list = value->GetIfList()) {
        return ParseEntriesFromJson(*list);
      }
      return base::unexpected(ParseResult::kJsonKeyValueNotAList);
    }
    return std::vector<SafetyListEntry>();
  };

  base::expected<std::vector<SafetyListEntry>, ParseResult> allowed_result =
      parse_one_list(kAllowedFieldName);
  base::expected<std::vector<SafetyListEntry>, ParseResult> blocked_result =
      parse_one_list(kBlockedFieldName);
  if (allowed_result.has_value() || blocked_result.has_value()) {
    host_indexed_content_settings_.Clear();
    SetAll(SpanOverExpected(allowed_result),
           ContentSetting::CONTENT_SETTING_ALLOW,
           host_indexed_content_settings_);
    SetAll(SpanOverExpected(blocked_result),
           ContentSetting::CONTENT_SETTING_BLOCK,
           host_indexed_content_settings_);
    MaybeSetHardcodedEntries(host_indexed_content_settings_);
  }

  return {allowed_result.error_or(ParseResult::kSuccess),
          blocked_result.error_or(ParseResult::kSuccess)};
}

void SafetyListManager::ParseSafetyLists(std::string_view json_string) {
  ParseStatus status = ParseSafetyListsInternal(json_string);

  base::UmaHistogramEnumeration(kAllowedHistogramName, status.allowed_result);
  base::UmaHistogramEnumeration(kBlockedHistogramName, status.blocked_result);
}

}  // namespace actor
