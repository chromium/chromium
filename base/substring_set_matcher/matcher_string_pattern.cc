// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/substring_set_matcher/matcher_string_pattern.h"

#include <tuple>
#include <utility>

#include "base/check_op.h"

namespace base {

MatcherStringPattern::MatcherStringPattern(std::string pattern,
                                           MatcherStringPattern::ID id)
    : pattern_(std::move(pattern)), id_(id) {
  DCHECK_NE(kInvalidId, id_);
}

MatcherStringPattern::~MatcherStringPattern() = default;

MatcherStringPattern::MatcherStringPattern(MatcherStringPattern&&) = default;
MatcherStringPattern& MatcherStringPattern::operator=(MatcherStringPattern&&) =
    default;

bool MatcherStringPattern::operator<(const MatcherStringPattern& rhs) const {
  return std::tie(id_, pattern_) < std::tie(rhs.id_, rhs.pattern_);
}

}  // namespace base
