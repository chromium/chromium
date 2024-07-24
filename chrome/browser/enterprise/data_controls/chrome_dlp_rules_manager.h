// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_CHROME_DLP_RULES_MANAGER_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_CHROME_DLP_RULES_MANAGER_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/substring_set_matcher/matcher_string_pattern.h"
#include "components/enterprise/data_controls/core/browser/action_context.h"
#include "components/enterprise/data_controls/core/browser/dlp_rules_manager_base.h"
#include "components/enterprise/data_controls/core/browser/rule.h"
#include "components/enterprise/data_controls/core/browser/verdict.h"
#include "components/url_matcher/url_matcher.h"

class Profile;

namespace data_controls {

class ChromeDlpRulesManagerTest;
class RulesService;

// Implementation of DlpRulesManagerBase common to all desktop platforms.
class ChromeDlpRulesManager : public DlpRulesManagerBase {
 public:
  using RuleId = int;
  using UrlConditionId = base::MatcherStringPattern::ID;
  using RulesConditionsMap = std::map<RuleId, UrlConditionId>;

  ~ChromeDlpRulesManager() override;

  // DlpRulesManagerBase:
  Level IsRestricted(const GURL& source,
                     Restriction restriction) const override;
  Level IsRestrictedByAnyRule(const GURL& source,
                              Restriction restriction,
                              std::string* out_source_pattern,
                              RuleMetadata* out_rule_metadata) const override;
  Level IsRestrictedDestination(const GURL& source,
                                const GURL& destination,
                                Restriction restriction,
                                std::string* out_source_pattern,
                                std::string* out_destination_pattern,
                                RuleMetadata* out_rule_metadata) const override;
  AggregatedDestinations GetAggregatedDestinations(
      const GURL& source,
      Restriction restriction) const override;
  std::string GetSourceUrlPattern(
      const GURL& source_url,
      Restriction restriction,
      Level level,
      RuleMetadata* out_rule_metadata) const override;

 protected:
  friend class data_controls::ChromeDlpRulesManagerTest;
  friend class data_controls::RulesService;

  explicit ChromeDlpRulesManager(Profile* profile);

  template <typename T>
  struct MatchedRuleInfo {
    MatchedRuleInfo(Level level,
                    std::optional<RuleId> rule_id,
                    std::optional<T> url_condition)
        : level(level), rule_id(rule_id), url_condition(url_condition) {}
    MatchedRuleInfo(const MatchedRuleInfo&) = default;
    MatchedRuleInfo() = default;
    MatchedRuleInfo& operator=(const MatchedRuleInfo&) = default;
    ~MatchedRuleInfo() = default;

    Level level;
    std::optional<RuleId> rule_id;
    std::optional<T> url_condition;
  };

  // Matches `url` against `url_matcher` patterns and returns the rules IDs
  // configured with the matched patterns.
  static RulesConditionsMap MatchUrlAndGetRulesMapping(
      const GURL& url,
      const url_matcher::URLMatcher* url_matcher,
      const std::map<UrlConditionId, RuleId>& rules_map);

  // Determines the maximum level of the rules of given
  // `restriction` joined with the `selected_rules`, and returns MatchedRuleInfo
  // of the matched rule.
  template <typename T>
  MatchedRuleInfo<T> GetMaxJoinRestrictionLevelAndRuleId(
      const Restriction restriction,
      const std::map<RuleId, T>& selected_rules,
      const std::map<Restriction, std::map<RuleId, Level>>& restrictions_map,
      const bool ignore_allow = false) const;

  // Parse the "DataLeakPrevention*" policies and populate corresponding class
  // data members. Virtual to be overridden in the CrOS implementation of this
  // class.
  virtual void OnDataLeakPreventionRulesUpdate();

  // The profile with which we are associated. Not owned. For CrOS, it's
  // currently always the main/primary profile.
  const raw_ptr<Profile> profile_ = nullptr;

  // Map from the restrictions to their configured rules IDs and levels.
  std::map<Restriction, std::map<RuleId, Level>> restrictions_map_;

  // Used to match the URLs of the sources.
  std::unique_ptr<url_matcher::URLMatcher> src_url_matcher_;

  // Used to match the URLs of the destinations.
  std::unique_ptr<url_matcher::URLMatcher> dst_url_matcher_;

  // Map from the URL matching conditions IDs of the sources to their configured
  // rules IDs.
  std::map<UrlConditionId, RuleId> src_url_rules_mapping_;

  // Map from the URL matching conditions IDs of the destinations to their
  // configured rules IDs.
  std::map<UrlConditionId, RuleId> dst_url_rules_mapping_;

  // Map from the URL matching conditions IDs of the sources to their string
  // patterns.
  std::map<UrlConditionId, std::string> src_patterns_mapping_;

  // Map from the URL matching conditions IDs of the destinations to their
  // string patterns.
  std::map<UrlConditionId, std::string> dst_patterns_mapping_;

  // Map from RuleIds to the rule metadata.
  std::map<RuleId, RuleMetadata> rules_id_metadata_mapping_;
};

}  // namespace data_controls

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_CHROME_DLP_RULES_MANAGER_H_
