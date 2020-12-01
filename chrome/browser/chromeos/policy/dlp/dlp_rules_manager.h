// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_RULES_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_RULES_MANAGER_H_

#include <map>
#include <memory>
#include <set>

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
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
extern const char kScreenShareRestriction[];

extern const char kArc[];
extern const char kCrostini[];
extern const char kPluginVm[];

extern const char kAllowLevel[];
extern const char kBlockLevel[];

}  // namespace dlp

// DlpRulesManager parses the rules set by DataLeakPreventionRulesList policy
// and serves as an available service which can be queried anytime about the
// restrictions set by the policy.
class DlpRulesManager : public KeyedService {
 public:
  // A restriction that can be set by DataLeakPreventionRulesList policy.
  enum class Restriction {
    kUnknownRestriction = 0,
    kClipboard = 1,      // Restricts sharing the data via clipboard and
                         // drag-n-drop.
    kScreenshot = 2,     // Restricts taking screenshots of confidential screen
                         // content.
                         // TODO(crbug/1145100): Update to include video capture
    kPrinting = 3,       // Restricts printing confidential screen content.
    kPrivacyScreen = 4,  // Enforces the Eprivacy screen when there's
                         // confidential content on the screen.
    kScreenShare = 5,    // Restricts screen sharing of confidential content
                         // through 3P extensions/websites.
    kMaxValue = kScreenShare
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

  ~DlpRulesManager() override;

  // Registers the policy pref.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Returns the enforcement level for `restriction` given that data comes
  // from `source`. ALLOW is returned if no restrictions should be applied.
  // Requires `restriction` to be one of the following: screenshot, printing,
  // privacy screen, screenshare.
  virtual Level IsRestricted(const GURL& source, Restriction restriction) const;

  // Returns the enforcement level for `restriction` given that data comes
  // from `source` and requested to be shared to `destination`. ALLOW is
  // returned if no restrictions should be applied. Requires `restriction` to be
  // clipboard.
  virtual Level IsRestrictedDestination(const GURL& source,
                                        const GURL& destination,
                                        Restriction restriction) const;

  // Returns the enforcement level for `restriction` given that data comes
  // from `source` and requested to be shared to `destination`. ALLOW is
  // returned if no restrictions should be applied. Requires `restriction` to be
  // clipboard.
  virtual Level IsRestrictedComponent(const GURL& source,
                                      const Component& destination,
                                      Restriction restriction) const;

  // Returns the enforcement level for `restriction` given that data comes
  // from `source` and requested to be shared to `destinations`. ALLOW is
  // returned if there is not any restriction should be applied on any of the
  // `destinations`. Requires `restriction` to be clipboard.
  virtual Level IsRestrictedAnyOfComponents(
      const GURL& source,
      const std::vector<Component>& destinations,
      Restriction restriction) const;

 protected:
  friend class DlpRulesManagerFactory;
  friend class DlpRulesManagerTest;

  explicit DlpRulesManager(PrefService* local_state);

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

// Initializes an instance of DlpRulesManager when a primary managed profile is
// being created, e.g. when managed user sign in.
class DlpRulesManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static DlpRulesManagerFactory* GetInstance();
  // Returns nullptr if there is no primary profile, e.g. the session is not
  // started.
  static DlpRulesManager* GetForPrimaryProfile();

  // TODO(crbug/1153146): Use TestingFactory instead.
  static void OverrideManagerForTesting(DlpRulesManager* testing_manager);

 private:
  friend class base::NoDestructor<DlpRulesManagerFactory>;

  DlpRulesManagerFactory();
  ~DlpRulesManagerFactory() override = default;

  // BrowserStateKeyedServiceFactory overrides:
  bool ServiceIsCreatedWithBrowserContext() const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_RULES_MANAGER_H_
