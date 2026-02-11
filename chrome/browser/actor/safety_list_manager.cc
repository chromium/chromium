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
#include "components/content_settings/core/common/content_settings_pattern.h"

namespace actor {

namespace {

constexpr std::string_view kAllowedFieldName = "navigation_allowed";
constexpr std::string_view kBlockedFieldName = "navigation_blocked";

constexpr std::string_view kAllowedHistogramName =
    "Actor.SafetyListParseResult.NavigationAllowed";
constexpr std::string_view kBlockedHistogramName =
    "Actor.SafetyListParseResult.NavigationBlocked";

void MaybeAppendHardcodedPatterns(SafetyList::Patterns& patterns) {
  if (IsNavigationGatingEnabled() &&
      kGlicIncludeHardcodedBlockListEntries.Get()) {
    patterns.emplace_back(
        ContentSettingsPattern::FromString("*"),
        ContentSettingsPattern::FromString("[*.]googleplex.com"));
    patterns.emplace_back(
        ContentSettingsPattern::FromString("*"),
        ContentSettingsPattern::FromString("[*.]corp.google.com"));
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

SafetyListManager::SafetyListManager() {
  SafetyList::Patterns patterns;
  MaybeAppendHardcodedPatterns(patterns);
  blocked_ = SafetyList(std::move(patterns));
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
        return SafetyList::ParsePatternListFromJson(*list);
      }
      return base::unexpected(SafetyListParseResult::kJsonKeyValueNotAList);
    }
    return SafetyList();
  };

  base::expected<SafetyList, SafetyListParseResult> allowed_result =
      parse_one_list(kAllowedFieldName);
  if (allowed_result.has_value()) {
    allowed_ = std::move(allowed_result.value());
  }

  base::expected<SafetyList, SafetyListParseResult> blocked_result =
      parse_one_list(kBlockedFieldName);
  if (blocked_result.has_value()) {
    SafetyList::Patterns patterns = blocked_result->patterns();
    MaybeAppendHardcodedPatterns(patterns);
    blocked_ = SafetyList(std::move(patterns));
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
