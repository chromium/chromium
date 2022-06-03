// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/vlog.h"

#include <stddef.h>
#include <algorithm>
#include <limits>
#include <ostream>
#include <utility>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace logging {

const int VlogInfo::kDefaultVlogLevel = 0;

struct VlogInfo::VmodulePattern {
  enum MatchTarget { MATCH_MODULE, MATCH_FILE };

  explicit VmodulePattern(const std::string& pattern);

  VmodulePattern() = default;

  std::string pattern;
  int vlog_level = VlogInfo::kDefaultVlogLevel;
  MatchTarget match_target = MATCH_MODULE;
  size_t score = 0;
};

VlogInfo::VmodulePattern::VmodulePattern(const std::string& pattern)
    : pattern(pattern) {
  // If the pattern contains a {forward,back} slash, we assume that
  // it's meant to be tested against the entire __FILE__ string.
  std::string::size_type first_slash = pattern.find_first_of("\\/");
  if (first_slash != std::string::npos)
    match_target = MATCH_FILE;
}

VlogInfo::VlogInfo(const std::string& v_switch,
                   const std::string& vmodule_switch,
                   int* min_log_level)
    : min_log_level_(min_log_level) {
  DCHECK_NE(min_log_level, nullptr);

  int vlog_level = 0;
  if (!v_switch.empty()) {
    if (base::StringToInt(v_switch, &vlog_level)) {
      SetMaxVlogLevel(vlog_level);
    } else {
      DLOG(WARNING) << "Could not parse v switch \"" << v_switch << "\"";
    }
  }

  base::StringPairs kv_pairs;
  if (!base::SplitStringIntoKeyValuePairs(
          vmodule_switch, '=', ',', &kv_pairs)) {
    DLOG(WARNING) << "Could not fully parse vmodule switch \""
                  << vmodule_switch << "\"";
  }
  for (base::StringPairs::const_iterator it = kv_pairs.begin();
       it != kv_pairs.end(); ++it) {
    VmodulePattern pattern(it->first);
    if (!base::StringToInt(it->second, &pattern.vlog_level)) {
      DLOG(WARNING) << "Parsed vlog level for \""
                    << it->first << "=" << it->second
                    << "\" as " << pattern.vlog_level;
    }
    vmodule_levels_.push_back(pattern);
  }
}

VlogInfo::~VlogInfo() = default;

namespace {

// Given a path, returns the basename with the extension chopped off
// (and any -inl suffix).  We avoid using FilePath to minimize the
// number of dependencies the logging system has.
base::StringPiece GetModule(base::StringPiece file) {
  base::StringPiece module = file;

  // Chop off the file extension.
  base::StringPiece::size_type extension_start = module.rfind('.');
  module = module.substr(0, extension_start);

  // Chop off the -inl suffix.
  static constexpr base::StringPiece kInlSuffix("-inl");
  if (base::EndsWith(module, kInlSuffix))
    module.remove_suffix(kInlSuffix.size());

  // Chop off the path up to the start of the file name. Using single-character
  // overload of `base::StringPiece::find_last_of` for speed; this overload does
  // not build a lookup table.
  base::StringPiece::size_type last_slash_pos = module.find_last_of('/');
  if (last_slash_pos != base::StringPiece::npos) {
    module.remove_prefix(last_slash_pos + 1);
    return module;
  }
  last_slash_pos = module.find_last_of('\\');
  if (last_slash_pos != base::StringPiece::npos)
    module.remove_prefix(last_slash_pos + 1);
  return module;
}

}  // namespace

int VlogInfo::GetVlogLevel(base::StringPiece file) {
  base::AutoLock lock(vmodule_levels_lock_);
  if (!vmodule_levels_.empty()) {
    base::StringPiece module(GetModule(file));
    for (size_t i = 0; i < vmodule_levels_.size(); i++) {
      VmodulePattern& it = vmodule_levels_[i];

      const bool kUseFile = it.match_target == VmodulePattern::MATCH_FILE;
      if (!MatchVlogPattern(kUseFile ? file : module, it.pattern)) {
        continue;
      }
      const int ret = it.vlog_level;

      // Since `it` matched, increase its score because we believe it has a
      // higher probability of winning next time.
      if (it.score == std::numeric_limits<size_t>::max()) {
        for (VmodulePattern& pattern : vmodule_levels_) {
          pattern.score = 0;
        }
      }
      ++it.score;
      if (i > 0 && it.score > vmodule_levels_[i - 1].score)
        std::swap(it, vmodule_levels_[i - 1]);

      return ret;
    }
  }
  return GetMaxVlogLevel();
}

void VlogInfo::SetMaxVlogLevel(int level) {
  // Log severity is the negative verbosity.
  *min_log_level_ = -level;
}

int VlogInfo::GetMaxVlogLevel() const {
  return -*min_log_level_;
}

bool MatchVlogPattern(base::StringPiece string,
                      base::StringPiece vlog_pattern) {
  // The code implements the glob matching using a greedy approach described in
  // https://research.swtch.com/glob.
  size_t s = 0, nexts = 0;
  size_t p = 0, nextp = 0;
  const size_t slen = string.size(), plen = vlog_pattern.size();
  while (s < slen || p < plen) {
    if (p < plen) {
      switch (vlog_pattern[p]) {
        // A slash (forward or back) must match a slash (forward or back).
        case '/':
        case '\\':
          if (s < slen && (string[s] == '/' || string[s] == '\\')) {
            p++, s++;
            continue;
          }
          break;
        // A '?' matches anything.
        case '?':
          if (s < slen) {
            p++, s++;
            continue;
          }
          break;
        case '*':
          nextp = p;
          nexts = s + 1;
          p++;
          continue;
        // Anything else must match literally.
        default:
          if (s < slen && string[s] == vlog_pattern[p]) {
            p++, s++;
            continue;
          }
          break;
      }
    }
    // Mismatch - maybe restart.
    if (0 < nexts && nexts <= slen) {
      p = nextp;
      s = nexts;
      continue;
    }
    return false;
  }
  return true;
}

}  // namespace logging
