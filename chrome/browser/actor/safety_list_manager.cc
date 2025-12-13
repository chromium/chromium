// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/safety_list_manager.h"

#include <optional>
#include <string_view>

#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_util.h"
#include "chrome/browser/actor/safety_list.h"
#include "components/content_settings/core/common/content_settings_pattern.h"

namespace actor {

namespace {

constexpr std::string_view kAllowedFieldName = "navigation_allowed";
constexpr std::string_view kBlockedFieldName = "navigation_blocked";

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

SafetyListManager::SafetyListManager() {
  SafetyList::Patterns patterns;
  MaybeAppendHardcodedPatterns(patterns);
  blocked_ = SafetyList(std::move(patterns));
}
SafetyListManager::~SafetyListManager() = default;

void SafetyListManager::ParseSafetyLists(std::string_view json_string) {
  std::optional<base::Value> json =
      base::JSONReader::Read(json_string, base::JSON_PARSE_RFC);
  if (!json.has_value()) {
    // TODO(crbug.com/454720466): Add metrics for failures and successes.
    return;
  }

  base::Value::Dict* json_dict = json->GetIfDict();
  if (!json_dict) {
    return;
  }

  if (base::Value::List* allowed = json_dict->FindList(kAllowedFieldName)) {
    allowed_ = SafetyList::ParsePatternListFromJson(*allowed);
  }

  if (base::Value::List* blocked = json_dict->FindList(kBlockedFieldName)) {
    SafetyList parsed_blocked = SafetyList::ParsePatternListFromJson(*blocked);
    SafetyList::Patterns patterns = parsed_blocked.patterns();
    MaybeAppendHardcodedPatterns(patterns);
    blocked_ = SafetyList(std::move(patterns));
  }
}

}  // namespace actor
