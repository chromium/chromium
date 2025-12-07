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

  static SafetyList ParsePatternListFromJson(
      const base::Value::List& list_data);

 private:
  Patterns patterns_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_SAFETY_LIST_H_
