// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/vlog.h"

#include <stddef.h>

#include <ostream>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace logging {

const int VlogInfo::kDefaultVlogLevel = 0;

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
    : vlog_level(VlogInfo::kDefaultVlogLevel), match_target(MATCH_MODULE) {}

// static
std::vector<VlogInfo::VmodulePattern> VlogInfo::ParseVmoduleLevels(
    const std::string& vmodule_switch) {
  std::vector<VmodulePattern> vmodule_levels;
  base::StringPairs kv_pairs;
  if (!base::SplitStringIntoKeyValuePairs(vmodule_switch, '=', ',',
                                          &kv_pairs)) {
    DLOG(WARNING) << "Could not fully parse vmodule switch \"" << vmodule_switch
                  << "\"";
  }
  for (const auto& pair : kv_pairs) {
    VmodulePattern pattern(pair.first);
    if (!base::StringToInt(pair.second, &pattern.vlog_level)) {
      DLOG(WARNING) << "Parsed vlog level for \"" << pair.first << "="
                    << pair.second << "\" as " << pattern.vlog_level;
    }
    vmodule_levels.push_back(pattern);
  }
  return vmodule_levels;
}

VlogInfo::VlogInfo(const std::string& v_switch,
                   const std::string& vmodule_switch,
                   int* min_log_level)
    : vmodule_levels_(ParseVmoduleLevels(vmodule_switch)),
      min_log_level_(min_log_level) {
  DCHECK_NE(min_log_level, nullptr);

  int vlog_level = 0;
  if (!v_switch.empty()) {
    if (base::StringToInt(v_switch, &vlog_level)) {
      SetMaxVlogLevel(vlog_level);
    } else {
      DLOG(WARNING) << "Could not parse v switch \"" << v_switch << "\"";
    }
  }
}

VlogInfo::~VlogInfo() = default;

namespace {

// Given a path, returns the basename with the extension chopped off
// (and any -inl suffix).  We avoid using FilePath to minimize the
// number of dependencies the logging system has.
std::string_view GetModule(std::string_view file) {
  std::string_view module(file);
  size_t last_slash_pos = module.find_last_of("\\/");
  if (last_slash_pos != std::string_view::npos) {
    module.remove_prefix(last_slash_pos + 1);
  }
  size_t extension_start = module.rfind('.');
  module = module.substr(0, extension_start);
  static const char kInlSuffix[] = "-inl";
  static const int kInlSuffixLen = std::size(kInlSuffix) - 1;
  if (base::EndsWith(module, kInlSuffix))
    module.remove_suffix(kInlSuffixLen);
  return module;
}

}  // namespace

int VlogInfo::GetVlogLevel(std::string_view file) const {
  if (!vmodule_levels_.empty()) {
    std::string_view module(GetModule(file));
    for (const auto& it : vmodule_levels_) {
      std::string_view target(
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

VlogInfo::VlogInfo(std::vector<VmodulePattern> vmodule_levels,
                   int* min_log_level)
    : vmodule_levels_(std::move(vmodule_levels)),
      min_log_level_(min_log_level) {}

VlogInfo* VlogInfo::WithSwitches(const std::string& vmodule_switch) const {
  std::vector<VmodulePattern> vmodule_levels = vmodule_levels_;
  std::vector<VmodulePattern> additional_vmodule_levels =
      ParseVmoduleLevels(vmodule_switch);
  vmodule_levels.insert(vmodule_levels.end(), additional_vmodule_levels.begin(),
                        additional_vmodule_levels.end());
  return new VlogInfo(std::move(vmodule_levels), min_log_level_);
}

bool MatchVlogPattern(std::string_view string, std::string_view vlog_pattern) {
  // The code implements the glob matching using a greedy approach described in
  // https://research.swtch.com/glob.
  size_t s = 0, nexts = 0;
  size_t p = 0, nextp = 0;
  size_t slen = string.size(), plen = vlog_pattern.size();
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
