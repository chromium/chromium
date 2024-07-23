// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_RULES_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_RULES_MANAGER_H_

#include <map>
#include <set>
#include <string>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/data_controls/chrome_dlp_rules_manager.h"
#include "components/enterprise/data_controls/core/browser/component.h"
#include "components/enterprise/data_controls/core/browser/dlp_rules_manager_base.h"
#include "url/gurl.h"

class Profile;

namespace data_controls {
class DlpReportingManager;
}  // namespace data_controls

namespace policy {

class DlpFilesController;

// DlpRulesManager is the CrOS-specific parser for the rules set by the
// DataLeakPreventionRulesList policy and serves as an available service which
// can be queried anytime about the restrictions set by the policy.
class DlpRulesManager : public data_controls::ChromeDlpRulesManager {
 public:
  // List of all possible component values, used to simplify iterating over all
  // the options.
  constexpr static const std::array<data_controls::Component, 6> components = {
      data_controls::Component::kArc,      data_controls::Component::kCrostini,
      data_controls::Component::kPluginVm, data_controls::Component::kUsb,
      data_controls::Component::kDrive,    data_controls::Component::kOneDrive};

  // Represents file metadata.
  struct FileMetadata {
    FileMetadata(uint64_t inode, const GURL& source)
        : inode(inode), source(source) {}
    FileMetadata(uint64_t inode, const std::string& source)
        : inode(inode), source(source) {}
    FileMetadata(const FileMetadata&) = default;
    FileMetadata& operator=(const FileMetadata&) = default;
    ~FileMetadata() = default;

    uint64_t inode;  // File inode number.
    GURL source;     // File source URL.
  };

  // Mapping from a level to the set of components for which that level is
  // enforced.
  using AggregatedComponents =
      std::map<Level, std::set<data_controls::Component>>;

  explicit DlpRulesManager(Profile* profile);
  ~DlpRulesManager() override = default;

  // Returns the enforcement level for `restriction` given that data comes
  // from `source` and requested to be shared to `destination`. ALLOW is
  // returned if there is no matching rule. Requires `restriction` to be
  // clipboard or files.
  // If there's a rule matching, `out_source_pattern` will be changed to the
  // original rule URL patterns and `out_rule_metadata` will be changed to the
  // matched rule metadata.
  virtual Level IsRestrictedComponent(
      const GURL& source,
      const data_controls::Component& destination,
      Restriction restriction,
      std::string* out_source_pattern,
      RuleMetadata* out_rule_metadata) const = 0;

  // Returns a mapping from the level to the set of components for which that
  // level is enforced for `source`. Components that do not have a matching rule
  // set are mapped to the ALLOW level. Requires `restriction` to be clipboard
  // or files.
  virtual AggregatedComponents GetAggregatedComponents(
      const GURL& source,
      Restriction restriction) const = 0;

  // Returns true if the general dlp reporting policy is enabled otherwise
  // false.
  virtual bool IsReportingEnabled() const = 0;

  // Returns the reporting manager that is used to report DLPPolicyEvents to the
  // serverside. Should always return a nullptr if reporting is disabled (see
  // IsReportingEnabled).
  virtual data_controls::DlpReportingManager* GetReportingManager() const = 0;

  // Returns the files controller that is used to perform DLP checks on files.
  // Should always return a nullptr if there are no file restrictions (and thus
  // the DLP daemon is not active).
  virtual DlpFilesController* GetDlpFilesController() const = 0;

  // Returns the admin-configured limit for the minimal size of data in the
  // clipboard to be checked against DLP rules.
  virtual size_t GetClipboardCheckSizeLimitInBytes() const = 0;

  // Returns true if there is any files policy set, and the daemon is
  // successfully initiated.
  virtual bool IsFilesPolicyEnabled() const = 0;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_RULES_MANAGER_H_
