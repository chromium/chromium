// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_RULES_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_RULES_MANAGER_H_

#include "components/keyed_service/core/keyed_service.h"

class GURL;

namespace policy {

// DlpRulesManager parses the rules set by DataLeakPreventionRulesList policy
// and serves as an available service which can be queried anytime about the
// restrictions set by the policy.
class DlpRulesManager : public KeyedService {
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
    kScreenshot = 2,     // Restricts taking screenshots of confidential screen
                         // content.
                         // TODO(crbug/1145100): Update to include video capture
    kPrinting = 3,       // Restricts printing confidential screen content.
    kPrivacyScreen = 4,  // Enforces the Eprivacy screen when there's
                         // confidential content on the screen.
    kScreenShare = 5,    // Restricts screen sharing of confidential content
                         // through 3P extensions/websites.
    kFiles = 6,          // Restricts file operations, like copying, uploading
                         // or opening in an app.
    kMaxValue = kFiles
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
    kWarn,    // Sets the restriction level to warn the user on every action.
    kMaxValue = kWarn
  };

  ~DlpRulesManager() override = default;

  // Returns the enforcement level for `restriction` given that data comes
  // from `source`. ALLOW is returned if no restrictions should be applied.
  // Requires `restriction` to be one of the following: screenshot, printing,
  // privacy screen, screenshare.
  virtual Level IsRestricted(const GURL& source,
                             Restriction restriction) const = 0;

  // Returns the enforcement level for `restriction` given that data comes
  // from `source` and requested to be shared to `destination`. ALLOW is
  // returned if no restrictions should be applied. Requires `restriction` to be
  // clipboard or files.
  virtual Level IsRestrictedDestination(const GURL& source,
                                        const GURL& destination,
                                        Restriction restriction) const = 0;

  // Returns the enforcement level for `restriction` given that data comes
  // from `source` and requested to be shared to `destination`. ALLOW is
  // returned if no restrictions should be applied. Requires `restriction` to be
  // clipboard.
  virtual Level IsRestrictedComponent(const GURL& source,
                                      const Component& destination,
                                      Restriction restriction) const = 0;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_RULES_MANAGER_H_
