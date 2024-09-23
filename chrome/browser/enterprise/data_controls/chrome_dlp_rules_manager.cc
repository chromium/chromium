// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/data_controls/chrome_dlp_rules_manager.h"

#include "base/feature_list.h"
#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/data_controls/core/browser/features.h"
#include "components/enterprise/data_controls/core/browser/prefs.h"
#include "components/prefs/pref_service.h"
#include "url/origin.h"

namespace data_controls {
namespace {

using Level = DlpRulesManagerBase::Level;
using RuleId = ChromeDlpRulesManager::RuleId;
using UrlConditionId = ChromeDlpRulesManager::UrlConditionId;
using RulesConditionsMap = std::map<RuleId, UrlConditionId>;

constexpr char kWildCardMatching[] = "*";

}  // namespace

ChromeDlpRulesManager::ChromeDlpRulesManager(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
}

ChromeDlpRulesManager::~ChromeDlpRulesManager() = default;

Level ChromeDlpRulesManager::IsRestricted(const GURL& source,
                                          Restriction restriction) const {
  DCHECK(src_url_matcher_);
  DCHECK(restriction == Restriction::kPrinting ||
         restriction == Restriction::kPrivacyScreen ||
         restriction == Restriction::kScreenshot ||
         restriction == Restriction::kScreenShare);

  const RulesConditionsMap src_rules_map = MatchUrlAndGetRulesMapping(
      source, src_url_matcher_.get(), src_url_rules_mapping_);

  return GetMaxJoinRestrictionLevelAndRuleId(restriction, src_rules_map,
                                             restrictions_map_)
      .level;
}

Level ChromeDlpRulesManager::IsRestrictedByAnyRule(
    const GURL& source,
    Restriction restriction,
    std::string* out_source_pattern,
    RuleMetadata* out_rule_metadata) const {
  DCHECK(src_url_matcher_);

  const RulesConditionsMap src_rules_map = MatchUrlAndGetRulesMapping(
      source, src_url_matcher_.get(), src_url_rules_mapping_);

  const MatchedRuleInfo rule_info = GetMaxJoinRestrictionLevelAndRuleId(
      restriction, src_rules_map, restrictions_map_,
      /*ignore_allow=*/true);

  if (rule_info.url_condition.has_value() && out_source_pattern) {
    UrlConditionId src_condition_id = rule_info.url_condition.value();
    *out_source_pattern = src_patterns_mapping_.at(src_condition_id);
  }
  if (rule_info.rule_id.has_value() && out_rule_metadata) {
    auto rule_metadata_itr =
        rules_id_metadata_mapping_.find(rule_info.rule_id.value());
    if (rule_metadata_itr != rules_id_metadata_mapping_.end()) {
      *out_rule_metadata = rule_metadata_itr->second;
    }
  }

  return rule_info.level;
}

Level ChromeDlpRulesManager::IsRestrictedDestination(
    const GURL& source,
    const GURL& destination,
    Restriction restriction,
    std::string* out_source_pattern,
    std::string* out_destination_pattern,
    RuleMetadata* out_rule_metadata) const {
  DCHECK(src_url_matcher_);
  DCHECK(dst_url_matcher_);
  DCHECK(restriction == Restriction::kClipboard ||
         restriction == Restriction::kFiles);

  // Allow copy/paste within the same document.
  if (url::IsSameOriginWith(source, destination)) {
    return Level::kAllow;
  }

  const RulesConditionsMap src_rules_map = MatchUrlAndGetRulesMapping(
      source, src_url_matcher_.get(), src_url_rules_mapping_);

  const RulesConditionsMap dst_rules_map = MatchUrlAndGetRulesMapping(
      destination, dst_url_matcher_.get(), dst_url_rules_mapping_);

  std::map<RuleId, std::pair<UrlConditionId, UrlConditionId>>
      intersection_rules;
  auto src_map_itr = src_rules_map.begin();
  auto dst_map_itr = dst_rules_map.begin();
  while (src_map_itr != src_rules_map.end() &&
         dst_map_itr != dst_rules_map.end()) {
    if (src_map_itr->first < dst_map_itr->first) {
      ++src_map_itr;
    } else if (dst_map_itr->first < src_map_itr->first) {
      ++dst_map_itr;
    } else {
      intersection_rules.insert(std::make_pair(
          src_map_itr->first,
          std::make_pair(src_map_itr->second, dst_map_itr->second)));
      ++src_map_itr;
      ++dst_map_itr;
    }
  }

  const MatchedRuleInfo rule_info = GetMaxJoinRestrictionLevelAndRuleId(
      restriction, intersection_rules, restrictions_map_);
  if (rule_info.url_condition.has_value() && out_source_pattern) {
    UrlConditionId src_condition_id = rule_info.url_condition.value().first;
    UrlConditionId dst_condition_id = rule_info.url_condition.value().second;
    if (out_source_pattern) {
      *out_source_pattern = src_patterns_mapping_.at(src_condition_id);
    }
    if (out_destination_pattern) {
      *out_destination_pattern = dst_patterns_mapping_.at(dst_condition_id);
    }
    if (rule_info.rule_id.has_value() && out_rule_metadata) {
      auto rule_metadata_itr =
          rules_id_metadata_mapping_.find(rule_info.rule_id.value());
      if (rule_metadata_itr != rules_id_metadata_mapping_.end()) {
        *out_rule_metadata = rule_metadata_itr->second;
      }
    }
  }
  return rule_info.level;
}

DlpRulesManagerBase::AggregatedDestinations
ChromeDlpRulesManager::GetAggregatedDestinations(
    const GURL& source,
    Restriction restriction) const {
  DCHECK(src_url_matcher_);
  DCHECK(dst_url_matcher_);
  DCHECK(restriction == Restriction::kClipboard ||
         restriction == Restriction::kFiles);

  auto restriction_it = restrictions_map_.find(restriction);
  if (restriction_it == restrictions_map_.end()) {
    return std::map<Level, std::set<std::string>>();
  }
  const std::map<RuleId, Level>& restriction_rules = restriction_it->second;

  const RulesConditionsMap src_rules_map = MatchUrlAndGetRulesMapping(
      source, src_url_matcher_.get(), src_url_rules_mapping_);
  // We need to check all possible destinations for rules that apply to it and
  // to the `source`. There can be many matching rules, but we want to keep only
  // the highest enforced level for each destination.
  std::map<std::string, Level> destination_level_map;
  // If there's a wildcard for a level, we should ignore all destinations for
  // lower levels.
  Level wildcard_level = Level::kNotSet;
  for (auto dst_map_itr : dst_url_rules_mapping_) {
    auto src_map_itr = src_rules_map.find(dst_map_itr.second);
    if (src_map_itr == src_rules_map.end()) {
      continue;
    }
    const auto& restriction_rule_itr =
        restriction_rules.find(src_map_itr->first);
    if (restriction_rule_itr == restriction_rules.end()) {
      continue;
    }
    UrlConditionId dst_condition_id = dst_map_itr.first;
    std::string destination_pattern =
        dst_patterns_mapping_.at(dst_condition_id);
    Level level = restriction_rule_itr->second;
    auto it = destination_level_map.find(destination_pattern);
    if (it == destination_level_map.end() || level > it->second) {
      destination_level_map[destination_pattern] = restriction_rule_itr->second;
    }
    if (destination_pattern == kWildCardMatching && level > wildcard_level) {
      wildcard_level = level;
    }
  }

  std::map<Level, std::set<std::string>> result;
  for (auto it : destination_level_map) {
    if (it.first == kWildCardMatching) {
      result[it.second] = {it.first};
    } else if (it.second >= wildcard_level &&
               result[it.second].find(kWildCardMatching) ==
                   result[it.second].end()) {
      result[it.second].insert(it.first);
    }
  }

  return result;
}

std::string ChromeDlpRulesManager::GetSourceUrlPattern(
    const GURL& source_url,
    Restriction restriction,
    Level level,
    RuleMetadata* out_rule_metadata) const {
  const std::set<UrlConditionId> url_conditions_ids =
      src_url_matcher_->MatchURL(source_url);

  std::map<RuleId, UrlConditionId> rules_conditions_map;
  for (const auto& condition_id : url_conditions_ids) {
    rules_conditions_map.insert(
        std::make_pair(src_url_rules_mapping_.at(condition_id), condition_id));
  }
  auto restriction_itr = restrictions_map_.find(restriction);
  if (restriction_itr == restrictions_map_.end()) {
    return std::string();
  }

  const auto rules_levels_map = restriction_itr->second;
  for (const auto& rule_level_entry : rules_levels_map) {
    auto rule_id = rule_level_entry.first;
    auto lvl = rule_level_entry.second;
    auto rule_condition_itr = rules_conditions_map.find(rule_id);
    if (lvl == level && rule_condition_itr != rules_conditions_map.end()) {
      auto condition_id = rule_condition_itr->second;
      auto condition_pattern_itr = src_patterns_mapping_.find(condition_id);
      if (condition_pattern_itr != src_patterns_mapping_.end()) {
        if (out_rule_metadata) {
          auto rule_metadata_itr = rules_id_metadata_mapping_.find(rule_id);
          if (rule_metadata_itr != rules_id_metadata_mapping_.end()) {
            *out_rule_metadata = rule_metadata_itr->second;
          }
        }
        return condition_pattern_itr->second;
      }
    }
  }
  return std::string();
}

// static
RulesConditionsMap ChromeDlpRulesManager::MatchUrlAndGetRulesMapping(
    const GURL& url,
    const url_matcher::URLMatcher* url_matcher,
    const std::map<UrlConditionId, RuleId>& rules_map) {
  DCHECK(url_matcher);
  const std::set<UrlConditionId> url_conditions_ids =
      url_matcher->MatchURL(url);

  RulesConditionsMap rules_conditions_map;
  for (const auto& id : url_conditions_ids) {
    rules_conditions_map[rules_map.at(id)] = id;
  }
  return rules_conditions_map;
}

template <typename T>
ChromeDlpRulesManager::MatchedRuleInfo<T>
ChromeDlpRulesManager::GetMaxJoinRestrictionLevelAndRuleId(
    const Restriction restriction,
    const std::map<RuleId, T>& selected_rules,
    const std::map<Restriction, std::map<RuleId, Level>>& restrictions_map,
    const bool ignore_allow) const {
  auto restriction_it = restrictions_map.find(restriction);
  if (restriction_it == restrictions_map.end()) {
    return MatchedRuleInfo<T>(Level::kAllow, std::nullopt, std::nullopt);
  }

  const std::map<RuleId, Level>& restriction_rules = restriction_it->second;

  Level max_level = Level::kNotSet;
  std::optional<T> url_condition = std::nullopt;
  std::optional<RuleId> matched_rule_id = std::nullopt;

  for (const auto& rule_pair : selected_rules) {
    const auto& restriction_rule_itr = restriction_rules.find(rule_pair.first);
    if (restriction_rule_itr == restriction_rules.end()) {
      continue;
    }
    if (ignore_allow && restriction_rule_itr->second == Level::kAllow) {
      continue;
    }
    if (restriction_rule_itr->second > max_level) {
      max_level = restriction_rule_itr->second;
      url_condition = rule_pair.second;
      matched_rule_id = rule_pair.first;
    }
  }

  if (max_level == Level::kNotSet) {
    return MatchedRuleInfo<T>(Level::kAllow, std::nullopt, std::nullopt);
  }

  return MatchedRuleInfo(max_level, matched_rule_id, url_condition);
}

void ChromeDlpRulesManager::OnDataLeakPreventionRulesUpdate() {
  // Not supported on non-CrOS platforms, see
  // `DlpRulesManagerImpl::OnDataLeakPreventionRulesUpdate()` for the CrOS
  // implementation.
  NOTREACHED();
}

}  // namespace data_controls
