// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_SAFETY_LIST_H_
#define CHROME_BROWSER_ACTOR_SAFETY_LIST_H_

#include <cstddef>
#include <vector>

#include "base/types/expected.h"
#include "base/values.h"
#include "components/content_settings/core/common/content_settings_pattern.h"

class GURL;

namespace actor {

// LINT.IfChange(SafetyListParseResult)
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SafetyListParseResult {
  // The Safety List was successfully parsed.
  kSuccess = 0,
  // The provided string was not valid JSON.
  kInvalidJson = 1,
  // The value associated with the key was not a list.
  kJsonKeyValueNotAList = 2,
  // A value in the list was not a dictionary.
  kJsonListValueNotADictionary = 3,
  // The `to` field was missing or not a string.
  kInvalidToField = 4,
  // The `to` field was not a valid URL pattern.
  kInvalidToUrlPattern = 5,
  // The `from` field was missing or not a string.
  kInvalidFromField = 6,
  // The `from` field was not a valid URL pattern.
  kInvalidFromUrlPattern = 7,
  kMaxValue = kInvalidFromUrlPattern,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:SafetyListParseResult)

struct SafetyListPatterns {
  ContentSettingsPattern source;
  ContentSettingsPattern destination;

  friend bool operator==(const SafetyListPatterns&,
                         const SafetyListPatterns&) = default;
};

class SafetyList {
 public:
  using Patterns = std::vector<SafetyListPatterns>;

  explicit SafetyList(Patterns patterns = {});

  ~SafetyList();

  SafetyList(const SafetyList&);
  SafetyList& operator=(const SafetyList&);

  const Patterns& patterns() const { return patterns_; }

  size_t size() const { return patterns_.size(); }

  bool ContainsUrlPairWithWildcardSource(const GURL& source,
                                         const GURL& destination) const;

  bool ContainsUrlPair(const GURL& source, const GURL& destination) const;

  // SafetyList may contain rules intended to block actor from visiting a
  // blocked site. These rules have a {source, destination} pair where `source`
  // is always wildcard. If a rule forbids self-navigation, the origin is
  // assumed to be off limits altogether.
  //
  // Just in case, to prevent rules which do not have wildcard in their
  // `source` from erroneously applying, we still pass `url` as the `source`
  // argument for ContainsUrlPair.
  bool ContainsPatternMatchingSelfNavigation(const GURL& url) const {
    return ContainsUrlPair(url, url);
  }

  // Parses a list of patterns from a JSON list. Returns the parsed SafetyList
  // on success, or a SafetyListParseResult on failure. If the result is a
  // SafetyListParseResult, the enum value is guaranteed to not be `kSuccess`.
  static base::expected<SafetyList, SafetyListParseResult>
  ParsePatternListFromJson(const base::ListValue& list_data);

  friend bool operator==(const SafetyList&, const SafetyList&) = default;

 private:
  Patterns patterns_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_SAFETY_LIST_H_
