// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_RULES_MANAGER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_RULES_MANAGER_IMPL_H_

#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"

#include <map>
#include <memory>
#include <set>

#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/url_matcher/url_matcher.h"

class GURL;
class PrefRegistrySimple;

namespace data_controls {
class DlpReportingManager;
}  // namespace data_controls

namespace policy {

class DlpFilesController;

class DlpRulesManagerImpl : public DlpRulesManager,
                            public chromeos::DlpClient::Observer {
 public:
  using RuleId = int;
  using UrlConditionId = base::MatcherStringPattern::ID;

  explicit DlpRulesManagerImpl(PrefService* local_state, Profile* profile);
  ~DlpRulesManagerImpl() override;

  // Registers the policy pref.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // DlpRulesManager:
  Level IsRestrictedComponent(const GURL& source,
                              const data_controls::Component& destination,
                              Restriction restriction,
                              std::string* out_source_pattern,
                              RuleMetadata* out_rule_metadata) const override;
  AggregatedComponents GetAggregatedComponents(
      const GURL& source,
      Restriction restriction) const override;
  bool IsReportingEnabled() const override;
  data_controls::DlpReportingManager* GetReportingManager() const override;
  DlpFilesController* GetDlpFilesController() const override;

  size_t GetClipboardCheckSizeLimitInBytes() const override;
  bool IsFilesPolicyEnabled() const override;

  // chromeos::DlpClient::Observer overrides:
  void DlpDaemonRestarted() override;

  // KeyedService overrides:
  void Shutdown() override;

 protected:
  friend class DlpRulesManagerFactory;

 private:
  void OnDataLeakPreventionRulesUpdate() override;

  // Used to track kDlpRulesList local state pref.
  PrefChangeRegistrar pref_change_registrar_;

  // Map from the components to their configured rules IDs.
  std::map<data_controls::Component, std::set<RuleId>> components_rules_;

  // Vector of source urls conditions.
  url_matcher::URLMatcherConditionSet::Vector src_conditions_;

  // Vector of destination urls conditions.
  url_matcher::URLMatcherConditionSet::Vector dst_conditions_;

  // System-wide singleton instantiated when required by rules configuration.
  std::unique_ptr<data_controls::DlpReportingManager> reporting_manager_;

  // System-wide singleton instantiated when there are rules involving files.
  std::unique_ptr<DlpFilesController> files_controller_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Observe to re-notify DLP daemon in case of restart.
  base::ScopedObservation<chromeos::DlpClient, chromeos::DlpClient::Observer>
      dlp_client_observation_{this};
#endif
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_RULES_MANAGER_IMPL_H_
