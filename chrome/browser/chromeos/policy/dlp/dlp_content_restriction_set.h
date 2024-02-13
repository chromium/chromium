// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_RESTRICTION_SET_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_RESTRICTION_SET_H_

#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"

#include <array>

#include "url/gurl.h"

namespace policy {

// Enum representing the possible restrictions applied to on-screen content.
// These values are used in bitmask in DlpContentRestrictionSet and should
// correspond to the type in which the mask is stored.
enum class DlpContentRestriction : int {
  // Do not allow any screenshots or video capture of the corresponding content.
  kScreenshot = 0,
  // Enforce ePrivacy screen when content is visible.
  kPrivacyScreen = 1,
  // Do not allow printing.
  kPrint = 2,
  // Do not allow screen share.
  kScreenShare = 3,
  // Should be equal to the last restriction.
  kMaxValue = kScreenShare,
};

// Represents result of evaluating restriction - both the level at which it
// should be enforced and the url that caused it.
struct RestrictionLevelAndUrl {
  RestrictionLevelAndUrl() = default;
  RestrictionLevelAndUrl(DlpRulesManager::Level level, GURL url)
      : level(level), url(url) {}
  RestrictionLevelAndUrl(const RestrictionLevelAndUrl&) = default;
  RestrictionLevelAndUrl& operator=(const RestrictionLevelAndUrl&) = default;
  ~RestrictionLevelAndUrl() = default;

  // Restrictions with the same level, but different URLs are considered the
  // same as they don't affect the current restriction enforcement.
  bool operator==(const RestrictionLevelAndUrl& other) const {
    return level == other.level;
  }

  DlpRulesManager::Level level = DlpRulesManager::Level::kNotSet;
  GURL url;
};

// Represents set of levels of all restrictions applied to on-screen content.
// Allowed to be copied and assigned.
class DlpContentRestrictionSet {
 public:
  DlpContentRestrictionSet();
  // TODO(b/324549895): Remove this constructor. Content restrictions shouldn't
  // be set without a url since it's used for reporting.
  DlpContentRestrictionSet(DlpContentRestriction restriction,
                           DlpRulesManager::Level level);

  DlpContentRestrictionSet(const DlpContentRestrictionSet& restriction_set);
  DlpContentRestrictionSet& operator=(const DlpContentRestrictionSet&);

  ~DlpContentRestrictionSet();

  bool operator==(const DlpContentRestrictionSet& other) const;
  bool operator!=(const DlpContentRestrictionSet& other) const;

  // Sets the |restriction| to the |level| if not set to a higher one yet and
  // remembers the |url| in this case.
  void SetRestriction(DlpContentRestriction restriction,
                      DlpRulesManager::Level level,
                      const GURL& gurl);

  // Returns the level for the |restriction|.
  DlpRulesManager::Level GetRestrictionLevel(
      DlpContentRestriction restriction) const;

  // Returns the url for most restrictive level for the |restriction|.
  const GURL& GetRestrictionUrl(DlpContentRestriction restriction) const;

  // Returns the level and url for the |restriction|.
  RestrictionLevelAndUrl GetRestrictionLevelAndUrl(
      DlpContentRestriction restriction) const;

  // Returns whether no restrictions should be applied.
  bool IsEmpty() const;

  // Sets all the restrictions to the highest level from |other| and this.
  void UnionWith(const DlpContentRestrictionSet& other);

  // Returns a new set that contains restrictions that exist in this, but not in
  // |other|.
  DlpContentRestrictionSet DifferenceWith(
      const DlpContentRestrictionSet& other) const;

  // Returns which content restrictions are being applied to the |url| according
  // to the policies.
  static DlpContentRestrictionSet GetForURL(const GURL& url);

  static void SetRestrictionsForURLForTesting(
      const GURL& url,
      const DlpContentRestrictionSet& restrictions);

 private:
  friend class DlpContentManagerTestHelper;

  // The current level and url of each of the restrictions.
  std::array<RestrictionLevelAndUrl,
             static_cast<int>(DlpContentRestriction::kMaxValue) + 1>
      restrictions_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_RESTRICTION_SET_H_
