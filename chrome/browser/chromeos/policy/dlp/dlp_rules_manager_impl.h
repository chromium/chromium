// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_RULES_MANAGER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_RULES_MANAGER_IMPL_H_

#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"

#include <map>
#include <memory>
#include <set>

#include "components/prefs/pref_change_registrar.h"
#include "components/url_matcher/url_matcher.h"

class GURL;
class PrefRegistrySimple;

namespace policy {

class DlpReportingManager;

#if BUILDFLAG(IS_CHROMEOS_ASH)
class DlpFilesController;
#endif

class DlpRulesManagerImpl : public DlpRulesManager {
 public:
  using RuleId = int;
  using UrlConditionId = base::MatcherStringPattern::ID;

  ~DlpRulesManagerImpl() override;

  // Registers the policy pref.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // DlpRulesManager:
  Level IsRestricted(const GURL& source,
                     Restriction restriction) const override;
  Level IsRestrictedByAnyRule(const GURL& source,
                              Restriction restriction,
                              std::string* out_source_pattern) const override;
  Level IsRestrictedDestination(
      const GURL& source,
      const GURL& destination,
      Restriction restriction,
      std::string* out_source_pattern,
      std::string* out_destination_pattern) const override;
  Level IsRestrictedComponent(const GURL& source,
                              const Component& destination,
                              Restriction restriction,
                              std::string* out_source_pattern) const override;
  AggregatedDestinations GetAggregatedDestinations(
      const GURL& source,
      Restriction restriction) const override;
  AggregatedComponents GetAggregatedComponents(
      const GURL& source,
      Restriction restriction) const override;
  bool IsReportingEnabled() const override;
  DlpReportingManager* GetReportingManager() const override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  DlpFilesController* GetDlpFilesController() const override;
#endif

  std::string GetSourceUrlPattern(const GURL& source_url,
                                  Restriction restriction,
                                  Level level) const override;
  size_t GetClipboardCheckSizeLimitInBytes() const override;
  bool IsFilesPolicyEnabled() const override;

 protected:
  friend class DlpRulesManagerFactory;

  explicit DlpRulesManagerImpl(PrefService* local_state);

 private:
  void OnPolicyUpdate();

  // Used to track kDlpRulesList local state pref.
  PrefChangeRegistrar pref_change_registrar_;

  // Map from the components to their configured rules IDs.
  std::map<Component, std::set<RuleId>> components_rules_;

  // Map from the restrictions to their configured rules IDs and levels.
  std::map<Restriction, std::map<RuleId, Level>> restrictions_map_;

  // Vector of source urls conditions.
  url_matcher::URLMatcherConditionSet::Vector src_conditions_;

  // Vector of destination urls conditions.
  url_matcher::URLMatcherConditionSet::Vector dst_conditions_;

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
  std::map<UrlConditionId, std::string> src_pattterns_mapping_;

  // Map from the URL matching conditions IDs of the destinations to their
  // string patterns.
  std::map<UrlConditionId, std::string> dst_pattterns_mapping_;

  // System-wide singleton instantiated when required by rules configuration.
  std::unique_ptr<DlpReportingManager> reporting_manager_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // System-wide singleton instantiated when there are rules involving files.
  std::unique_ptr<DlpFilesController> files_controller_;
#endif
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_RULES_MANAGER_IMPL_H_
