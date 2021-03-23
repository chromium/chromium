// Copyright 2020 The Chromium Authors. All rights reserved.
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

class DlpRulesManagerImpl : public DlpRulesManager {
 public:
  using RuleId = int;
  using UrlConditionId = url_matcher::URLMatcherConditionSet::ID;

  ~DlpRulesManagerImpl() override;

  // Registers the policy pref.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // DlpRulesManager:
  Level IsRestricted(const GURL& source,
                     Restriction restriction) const override;
  Level IsRestrictedDestination(const GURL& source,
                                const GURL& destination,
                                Restriction restriction) const override;
  Level IsRestrictedComponent(const GURL& source,
                              const Component& destination,
                              Restriction restriction) const override;

 protected:
  friend class DlpRulesManagerFactory;

  explicit DlpRulesManagerImpl(PrefService* local_state);

 private:
  void OnPolicyUpdate();

  // Returns the maximum level of the rules of given `restriction` joined with
  // the `selected_rules`.
  Level GetMaxJoinRestrictionLevel(
      const Restriction restriction,
      const std::set<RuleId>& selected_rules) const;

  // Returns the maximum level of the rules of given `restriction` joined with
  // the `source_rules` and `destination_rules`.
  Level GetMaxJoinRestrictionLevel(
      const Restriction restriction,
      const std::set<RuleId>& source_rules,
      const std::set<RuleId>& destination_rules) const;

  // Used to track kDlpRulesList local state pref.
  PrefChangeRegistrar pref_change_registrar_;

  // Used to match the URLs of the sources.
  std::unique_ptr<url_matcher::URLMatcher> src_url_matcher_;

  // Used to match the URLs of the destinations.
  std::unique_ptr<url_matcher::URLMatcher> dst_url_matcher_;

  // Map from the components to their configured rules IDs.
  std::map<Component, std::set<RuleId>> components_rules_;

  // Map from the restrictions to their configured rules IDs and levels.
  std::map<Restriction, std::map<RuleId, Level>> restrictions_map_;

  // Map from the URL matching conditions IDs of the sources to their configured
  // rules IDs.
  std::map<UrlConditionId, RuleId> src_url_rules_mapping_;

  // Map from the URL matching conditions IDs of the destinations to their
  // configured rules IDs.
  std::map<UrlConditionId, RuleId> dst_url_rules_mapping_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_RULES_MANAGER_IMPL_H_
