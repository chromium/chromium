// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>
#include <stdint.h>

#include <set>
#include <string>
#include <vector>

#include "base/substring_set_matcher/matcher_string_pattern.h"
#include "base/substring_set_matcher/substring_set_matcher.h"

namespace base {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);

  std::vector<MatcherStringPattern> patterns;
  std::set<std::string> pattern_set;
  for (;;) {
    std::string pattern = provider.ConsumeRandomLengthString();
    if (pattern.empty() || pattern_set.count(pattern))
      break;
    patterns.emplace_back(pattern, patterns.size());
    pattern_set.insert(pattern);
  }

  SubstringSetMatcher matcher;
  if (matcher.Build(patterns)) {
    std::set<MatcherStringPattern::ID> matches;
    matcher.Match(provider.ConsumeRandomLengthString(), &matches);
  }

  return 0;
}

}  // namespace base
