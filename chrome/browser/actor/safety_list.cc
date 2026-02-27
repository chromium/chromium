// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/safety_list.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "url/gurl.h"

namespace actor {

SafetyList::SafetyList(std::vector<SafetyListEntry> entries)
    : entries_(std::move(entries)) {}

SafetyList::~SafetyList() = default;

SafetyList::SafetyList(const SafetyList&) = default;
SafetyList& SafetyList::operator=(const SafetyList&) = default;

// static
base::expected<SafetyList, SafetyListParseResult>
SafetyList::ParseEntriesFromJson(const base::ListValue& list_data) {
  std::vector<SafetyListEntry> entries;
  entries.reserve(list_data.size());
  for (const auto& navigation : list_data) {
    const base::DictValue* navigation_dict = navigation.GetIfDict();
    if (!navigation_dict) {
      return base::unexpected(
          SafetyListParseResult::kJsonListValueNotADictionary);
    }

    // Only parse entry if both fields exist.
    const std::string* from = navigation_dict->FindString("from");
    if (!from) {
      return base::unexpected(SafetyListParseResult::kInvalidFromField);
    }
    ContentSettingsPattern source = ContentSettingsPattern::FromString(*from);
    if (!source.IsValid()) {
      return base::unexpected(SafetyListParseResult::kInvalidFromUrlPattern);
    }

    const std::string* to = navigation_dict->FindString("to");
    if (!to) {
      return base::unexpected(SafetyListParseResult::kInvalidToField);
    }
    ContentSettingsPattern destination =
        ContentSettingsPattern::FromString(*to);
    if (!destination.IsValid()) {
      return base::unexpected(SafetyListParseResult::kInvalidToUrlPattern);
    }
    entries.push_back(
        SafetyListEntry{std::move(source), std::move(destination)});
  }
  return SafetyList(std::move(entries));
}

}  // namespace actor
