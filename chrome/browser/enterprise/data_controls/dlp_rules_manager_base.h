// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_DLP_RULES_MANAGER_BASE_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_DLP_RULES_MANAGER_BASE_H_

#include <map>
#include <set>
#include <string>

#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

namespace policy {

// DlpRulesManagerBase is the generic interface to parse the rules set in the
// DataLeakPreventionRulesList policy and serves as an available service which
// can be queried anytime about the restrictions set by the policy.
class DlpRulesManagerBase : public KeyedService {
 public:
  // A restriction that can be set by DataLeakPreventionRulesList policy.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // When new entries are added, EnterpriseDlpPolicyRestriction enum in
  // histograms/enums.xml should be updated.
  enum class Restriction {
    kUnknownRestriction = 0,
    kClipboard = 1,      // Restricts sharing the data via clipboard and
                         // drag-n-drop.
    kScreenshot = 2,     // Restricts taking screenshots and video captures of
                         // confidential screen content.
    kPrinting = 3,       // Restricts printing confidential screen content.
    kPrivacyScreen = 4,  // Enforces the Eprivacy screen when there's
                         // confidential content on the screen.
    kScreenShare = 5,    // Restricts screen sharing of confidential content
                         // through 3P extensions/websites.
    kFiles = 6,          // Restricts file operations, like copying, uploading
                         // or opening in an app.
    kMaxValue = kFiles
  };

  // The enforcement level of the restriction set by DataLeakPreventionRulesList
  // policy. Should be listed in the order of increased priority.
  enum class Level {
    kNotSet = 0,  // Restriction level is not set.
    kReport = 1,  // Restriction level to only report on every action.
    kWarn = 2,    // Restriction level to warn the user on every action.
    kBlock = 3,   // Restriction level to block the user on every action.
    kAllow = 4,   // Restriction level to allow (no restriction).
    kMaxValue = kAllow
  };

  // Represents rule metadata that is used for reporting.
  struct RuleMetadata {
    RuleMetadata(const std::string& name, const std::string& obfuscated_id)
        : name(name), obfuscated_id(obfuscated_id) {}
    RuleMetadata(const RuleMetadata&) = default;
    RuleMetadata() = default;
    RuleMetadata& operator=(const RuleMetadata&) = default;
    ~RuleMetadata() = default;

    std::string name;
    std::string obfuscated_id;
  };

  // Mapping from a level to the set of destination URLs for which that level is
  // enforced.
  using AggregatedDestinations = std::map<Level, std::set<std::string>>;

  ~DlpRulesManagerBase() override = default;

  // Returns the enforcement level for `restriction` given that data comes
  // from `source`. ALLOW is returned if there is no matching rule. Requires
  // `restriction` to be one of the following: screenshot, printing,
  // privacy screen, screenshare.
  virtual Level IsRestricted(const GURL& source,
                             Restriction restriction) const = 0;

  // Returns the highest possible restriction enforcement level for
  // 'restriction' given that data comes from 'source' and the destination might
  // be any. ALLOW level rules are ignored.
  // If there's a rule matching, `out_source_pattern` will be changed to any
  // random matching rule URL pattern  and `out_rule_metadata` will be changed
  // to the matched rule metadata.
  virtual Level IsRestrictedByAnyRule(
      const GURL& source,
      Restriction restriction,
      std::string* out_source_pattern,
      RuleMetadata* out_rule_metadata) const = 0;

  // Returns the enforcement level for `restriction` given that data comes
  // from `source` and requested to be shared to `destination`. ALLOW is
  // returned if there is no matching rule. Requires `restriction` to be
  // clipboard or files.
  // If there's a rule matching, `out_source_pattern` and
  // `out_destination_pattern` will be changed to the original rule URL
  // patterns  and `out_rule_metadata` will be changed to the matched rule
  // metadata.
  virtual Level IsRestrictedDestination(
      const GURL& source,
      const GURL& destination,
      Restriction restriction,
      std::string* out_source_pattern,
      std::string* out_destination_pattern,
      RuleMetadata* out_rule_metadata) const = 0;

  // Returns a mapping from the level to a set of destination URLs for which
  // that level is enforced for `source`. Each destination URL it is mapped to
  // the highest level, if there are multiple applicable rules. Requires
  // `restriction` to be clipboard or files.
  virtual AggregatedDestinations GetAggregatedDestinations(
      const GURL& source,
      Restriction restriction) const = 0;

  // Returns the URL pattern that `source_url` is matched against. The returned
  // URL pattern should be configured in a policy rule with the same
  // `restriction` and `level`.
  virtual std::string GetSourceUrlPattern(
      const GURL& source_url,
      Restriction restriction,
      Level level,
      RuleMetadata* out_rule_metadata) const = 0;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_DLP_RULES_MANAGER_BASE_H_
