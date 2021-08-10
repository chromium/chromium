// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/vlog.h"

#include <stddef.h>

#include <ostream>
#include <utility>

#include "base/cxx17_backports.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace logging {

const int VlogInfo::kDefaultVlogLevel = 0;

struct VlogInfo::VmodulePattern {
  enum MatchTarget { MATCH_MODULE, MATCH_FILE };

  explicit VmodulePattern(const std::string& pattern);

  VmodulePattern();

  std::string pattern;
  int vlog_level;
  MatchTarget match_target;
};

VlogInfo::VmodulePattern::VmodulePattern(const std::string& pattern)
    : pattern(pattern),
      vlog_level(VlogInfo::kDefaultVlogLevel),
      match_target(MATCH_MODULE) {
  // If the pattern contains a {forward,back} slash, we assume that
  // it's meant to be tested against the entire __FILE__ string.
  std::string::size_type first_slash = pattern.find_first_of("\\/");
  if (first_slash != std::string::npos)
    match_target = MATCH_FILE;
}

VlogInfo::VmodulePattern::VmodulePattern()
    : vlog_level(VlogInfo::kDefaultVlogLevel),
      match_target(MATCH_MODULE) {}

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
base::StringPiece GetModule(const base::StringPiece& file) {
  base::StringPiece module(file);
  base::StringPiece::size_type last_slash_pos =
      module.find_last_of("\\/");
  if (last_slash_pos != base::StringPiece::npos)
    module.remove_prefix(last_slash_pos + 1);
  base::StringPiece::size_type extension_start = module.rfind('.');
  module = module.substr(0, extension_start);
  static const char kInlSuffix[] = "-inl";
  static const int kInlSuffixLen = base::size(kInlSuffix) - 1;
  if (base::EndsWith(module, kInlSuffix))
    module.remove_suffix(kInlSuffixLen);
  return module;
}

}  // namespace

int VlogInfo::GetVlogLevel(const base::StringPiece& file) const {
  if (!vmodule_levels_.empty()) {
    base::StringPiece module(GetModule(file));
    for (const auto& it : vmodule_levels_) {
      base::StringPiece target(
          (it.match_target == VmodulePattern::MATCH_FILE) ? file : module);
      if (MatchVlogPattern(target, it.pattern))
        return it.vlog_level;
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

bool MatchVlogPattern(const base::StringPiece& string,
                      const base::StringPiece& vlog_pattern) {
  base::StringPiece pat(vlog_pattern);
  base::StringPiece str(string);

  // The code implements the glob matching using a greedy approach described in
  // https://research.swtch.com/glob.
  size_t s = 0, nexts = 0;
  size_t p = 0, nextp = 0;
  size_t slen = str.size(), plen = pat.size();
  while (s < slen || p < plen) {
    if (p < plen) {
      switch (pat[p]) {
        // A slash (forward or back) must match a slash (forward or back).
        case '/':
        case '\\':
          if (s < slen && (str[s] == '/' || str[s] == '\\')) {
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
          if (s < slen && str[s] == pat[p]) {
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
