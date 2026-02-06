// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_SAFETY_LIST_H_
#define CHROME_BROWSER_ACTOR_SAFETY_LIST_H_

#include <cstddef>
#include <vector>

#include "base/values.h"
#include "components/content_settings/core/common/content_settings_pattern.h"

class GURL;

namespace actor {

struct SafetyListPatterns {
  ContentSettingsPattern source;
  ContentSettingsPattern destination;
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

  static SafetyList ParsePatternListFromJson(const base::ListValue& list_data);

 private:
  Patterns patterns_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_SAFETY_LIST_H_
