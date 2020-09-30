// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_RULES_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_RULES_MANAGER_H_

#include <map>
#include <memory>
#include <set>

#include "components/prefs/pref_change_registrar.h"
#include "components/url_matcher/url_matcher.h"

class GURL;
class PrefRegistrySimple;

namespace policy {

// The following const strings are used to parse the policy pref value.
namespace dlp {

extern const char kClipboardRestriction[];
extern const char kScreenshotRestriction[];
extern const char kPrintingRestriction[];
extern const char kPrivacyScreenRestriction[];

extern const char kArc[];
extern const char kCrostini[];
extern const char kPluginVm[];

extern const char kAllowLevel[];
extern const char kBlockLevel[];

}  // namespace dlp

// DlpRulesManager parses the rules set by DataLeakPreventionRulesList policy
// and serves as an available service which can be queried anytime about the
// restrictions set by the policy.
class DlpRulesManager {
 public:
  // A restriction that can be set by DataLeakPreventionRulesList policy.
  enum class Restriction {
    kUnknownRestriction = 0,
    kClipboard = 1,      // Restricts sharing the clipboard data.
    kScreenshot = 2,     // Restricts taking screenshots of confidential screen
                         // content.
    kPrinting = 3,       // Restricts printing confidential screen content.
    kPrivacyScreen = 4,  // Enforces the Eprivacy screen when there's
                         // confidential content on the screen.
    kMaxValue = kPrivacyScreen
  };

  // A representation of destinations to which sharing confidential data is
  // restricted by DataLeakPreventionRulesList policy.
  enum class Component {
    kUnknownComponent,
    kArc,       // ARC++ as a Guest OS.
    kCrostini,  // Crostini as a Guest OS.
    kPluginVm,  // Plugin VM (Parallels/Windows) as a Guest OS.
    kMaxValue = kPluginVm
  };

  // The enforcement level of the restriction set by DataLeakPreventionRulesList
  // policy.
  enum class Level {
    kNotSet,  // Restriction level is not set.
    kBlock,   // Sets the restriction level to block the user on every action.
    kAllow,   // Sets the restriction level to allow (no restriction).
    kMaxValue = kAllow
  };

  using RuleId = int;
  using UrlConditionId = url_matcher::URLMatcherConditionSet::ID;

  // Creates a singleton instance of the class.
  static void Init();

  // Returns whether DlpRulesManager was already created after user policy stack
  // is initialized.
  static bool IsInitialized();

  // Returns a pointer to the existing instance of the class.
  static DlpRulesManager* Get();

  // Registers the policy pref.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Returns the enforcement level for `restriction` given that data comes
  // from `source`. ALLOW is returned if no restrictions should be applied.
  // Requires `restriction` to be one of the following: screenshot, printing,
  // privacy screen.
  Level IsRestricted(const GURL& source, Restriction restriction) const;

  // Returns the enforcement level for `restriction` given that data comes
  // from `source` and requested to be shared to `destination`. ALLOW is
  // returned if no restrictions should be applied. Requires `restriction` to be
  // clipboard.
  Level IsRestrictedDestination(const GURL& source,
                                const GURL& destination,
                                Restriction restriction) const;

  // Returns the enforcement level for `restriction` given that data comes
  // from `source` and requested to be shared to `destination`. ALLOW is
  // returned if no restrictions should be applied. Requires `restriction` to be
  // clipboard.
  Level IsRestrictedComponent(const GURL& source,
                              const Component& destination,
                              Restriction restriction) const;

  // Returns the enforcement level for `restriction` given that data comes
  // from `source` and requested to be shared to `destinations`. ALLOW is
  // returned if there is not any restriction should be applied on any of the
  // `destinations`. Requires `restriction` to be clipboard.
  Level IsRestrictedAnyOfComponents(const GURL& source,
                                    const std::vector<Component>& destinations,
                                    Restriction restriction) const;

 private:
  friend class DlpRulesManagerTest;

  DlpRulesManager();
  ~DlpRulesManager();

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

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_RULES_MANAGER_H_
