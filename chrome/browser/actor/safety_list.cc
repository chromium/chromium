// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/safety_list.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "url/gurl.h"

namespace actor {

bool SafetyList::ContainsUrlPair(const GURL& source,
                                 const GURL& destination) const {
  return std::ranges::any_of(patterns_, [&](const SafetyListPatterns& pattern) {
    return pattern.source.Matches(source) &&
           pattern.destination.Matches(destination);
  });
}

bool SafetyList::ContainsUrlPairWithWildcardSource(
    const GURL& source,
    const GURL& destination) const {
  return std::ranges::any_of(patterns_, [&](const SafetyListPatterns& pattern) {
    // A full wildcard [*] is not included in `HasDomainWildcard()`, so we check
    // for that as well.
    bool has_domain_wildcard =
        pattern.source.GetScope() ==
            ContentSettingsPattern::Scope::kFullWildcard ||
        pattern.source.HasDomainWildcard();
    return has_domain_wildcard && pattern.source.Matches(source) &&
           pattern.destination.Matches(destination);
  });
}

SafetyList::SafetyList(Patterns patterns) : patterns_(std::move(patterns)) {}

SafetyList::~SafetyList() = default;

SafetyList::SafetyList(const SafetyList&) = default;
SafetyList& SafetyList::operator=(const SafetyList&) = default;

// static
SafetyList SafetyList::ParsePatternListFromJson(
    const base::Value::List& list_data) {
  Patterns patterns;
  for (const auto& navigation : list_data) {
    const base::Value::Dict* navigation_dict = navigation.GetIfDict();
    if (!navigation_dict) {
      return SafetyList();
    }

    // Only parse pattern pair if both fields exist.
    const std::string* to = navigation_dict->FindString("to");
    if (!to) {
      return SafetyList();
    }
    const std::string* from = navigation_dict->FindString("from");
    if (!from) {
      return SafetyList();
    }
    ContentSettingsPattern source = ContentSettingsPattern::FromString(*from);
    ContentSettingsPattern destination =
        ContentSettingsPattern::FromString(*to);
    if (!source.IsValid() || !destination.IsValid()) {
      return SafetyList();
    }
    patterns.push_back(
        SafetyListPatterns{std::move(source), std::move(destination)});
  }
  return SafetyList(std::move(patterns));
}

}  // namespace actor
