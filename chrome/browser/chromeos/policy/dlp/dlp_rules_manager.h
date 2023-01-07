// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_RULES_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_RULES_MANAGER_H_

#include <map>
#include <set>
#include <string>

#include "build/chromeos_buildflags.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

class GURL;

namespace policy {

class DlpReportingManager;

#if BUILDFLAG(IS_CHROMEOS_ASH)
class DlpFilesController;
#endif

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

  // A representation of destinations to which sharing confidential data is
  // restricted by DataLeakPreventionRulesList policy.
  // When adding new values, make sure to update the `components` below as well.
  enum class Component {
    kUnknownComponent,
    kArc,       // ARC++ as a Guest OS.
    kCrostini,  // Crostini as a Guest OS.
    kPluginVm,  // Plugin VM (Parallels/Windows) as a Guest OS.
    kUsb,       // Removable disk.
    kDrive,     // Google drive for file storage.
    kMaxValue = kDrive
  };

  // List of all possible component values, used to simplify iterating over all
  // the options.
  constexpr static const std::array<Component, 5> components = {
      Component::kArc, Component::kCrostini, Component::kPluginVm,
      Component::kUsb, Component::kDrive};

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

  // Mapping from a level to the set of destination URLs for which that level is
  // enforced.
  using AggregatedDestinations = std::map<Level, std::set<std::string>>;
  // Mapping from a level to the set of components for which that level is
  // enforced.
  using AggregatedComponents = std::map<Level, std::set<Component>>;

  ~DlpRulesManager() override = default;

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
  // random matching rule URL pattern.
  virtual Level IsRestrictedByAnyRule(
      const GURL& source,
      Restriction restriction,
      std::string* out_source_pattern) const = 0;

  // Returns the enforcement level for `restriction` given that data comes
  // from `source` and requested to be shared to `destination`. ALLOW is
  // returned if there is no matching rule. Requires `restriction` to be
  // clipboard or files.
  // If there's a rule matching, `out_source_pattern` and
  // `out_destination_pattern` will be changed to the original rule URL
  // patterns.
  virtual Level IsRestrictedDestination(
      const GURL& source,
      const GURL& destination,
      Restriction restriction,
      std::string* out_source_pattern,
      std::string* out_destination_pattern) const = 0;

  // Returns the enforcement level for `restriction` given that data comes
  // from `source` and requested to be shared to `destination`. ALLOW is
  // returned if there is no matching rule. Requires `restriction` to be
  // clipboard or files.
  // If there's a rule matching, `out_source_pattern` will be changed to the
  // original rule URL patterns.
  virtual Level IsRestrictedComponent(
      const GURL& source,
      const Component& destination,
      Restriction restriction,
      std::string* out_source_pattern) const = 0;

  // Returns a mapping from the level to a set of destination URLs for which
  // that level is enforced for `source`. Each destination URL it is mapped to
  // the highest level, if there are multiple applicable rules. Requires
  // `restriction` to be clipboard or files.
  virtual AggregatedDestinations GetAggregatedDestinations(
      const GURL& source,
      Restriction restriction) const = 0;

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
  virtual DlpReportingManager* GetReportingManager() const = 0;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Returns the files controller that is used to perform DLP checks on files.
  // Should always return a nullptr if there are no file restrictions (and thus
  // the DLP daemon is not active).
  virtual DlpFilesController* GetDlpFilesController() const = 0;
#endif

  // Returns the URL pattern that `source_url` is matched against. The returned
  // URL pattern should be configured in a policy rule with the same
  // `restriction` and `level`.
  virtual std::string GetSourceUrlPattern(const GURL& source_url,
                                          Restriction restriction,
                                          Level level) const = 0;

  // Returns the admin-configured limit for the minimal size of data in the
  // clipboard to be checked against DLP rules.
  virtual size_t GetClipboardCheckSizeLimitInBytes() const = 0;

  // Returns true if there is any files policy set, and the daemon is
  // successfully initiated.
  virtual bool IsFilesPolicyEnabled() const = 0;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_RULES_MANAGER_H_
