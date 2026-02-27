// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_SAFETY_LIST_MANAGER_H_
#define CHROME_BROWSER_ACTOR_SAFETY_LIST_MANAGER_H_

#include <string_view>

#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/host_indexed_content_settings.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace actor {

class SafetyListManager {
 public:
  // LINT.IfChange(ParseResult)
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ParseResult {
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

  // Verdicts that are supported by the safety lists.
  enum class Decision {
    // No decision was made by the safety lists.
    kNone,
    // The action is allowed by the safety lists.
    kAllow,
    // The action is blocked by the safety lists.
    kBlock,
  };

  ~SafetyListManager();

  SafetyListManager(const SafetyListManager&) = delete;
  SafetyListManager& operator=(const SafetyListManager&) = delete;
  SafetyListManager(SafetyListManager&&) = default;
  SafetyListManager& operator=(SafetyListManager&&) = default;

  static SafetyListManager* GetInstance();
  static SafetyListManager CreateForTesting();

  // Looks up the most specific rule applying to `source` and `destination`. If
  // no such rule exists, returns `Decision::kNone`.
  Decision Find(const GURL& source, const GURL& destination) const;

  void ParseSafetyLists(std::string_view json);

 private:
  // For singleton pattern.
  friend class base::NoDestructor<SafetyListManager>;
  SafetyListManager();

  struct ParseStatus {
    ParseResult allowed_result;
    ParseResult blocked_result;
  };

  ParseStatus ParseSafetyListsInternal(std::string_view json_string);

  // TODO(crbug.com/453660392): Add hashmap with JSON key -> SafetyList pairing.
  content_settings::HostIndexedContentSettings host_indexed_content_settings_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_SAFETY_LIST_MANAGER_H_
