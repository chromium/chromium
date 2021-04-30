// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_RESTRICTION_SET_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_RESTRICTION_SET_H_

#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"

#include <array>

namespace policy {

// Enum representing the possible restrictions applied to on-screen content.
// These values are used in bitmask in DlpContentRestrictionSet and should
// correspond to the type in which the mask is stored.
enum DlpContentRestriction {
  // Do not allow any screenshots of the corresponding content.
  kScreenshot = 0,
  // Enforce ePrivacy screen when content is visible.
  kPrivacyScreen = 1,
  // Do not allow printing.
  kPrint = 2,
  // Do not allow video capturing of the content.
  kVideoCapture = 3,
  // Do not allow screen share.
  kScreenShare = 4,
  // Should be equal to the last restriction.
  kMaxValue = kScreenShare,
};

// Represents set of levels of all restrictions applied to on-screen content.
// Allowed to be copied and assigned.
class DlpContentRestrictionSet {
 public:
  DlpContentRestrictionSet();
  DlpContentRestrictionSet(DlpContentRestriction restriction,
                           DlpRulesManager::Level level);

  DlpContentRestrictionSet(const DlpContentRestrictionSet& restriction_set);
  DlpContentRestrictionSet& operator=(const DlpContentRestrictionSet&);

  ~DlpContentRestrictionSet();

  bool operator==(const DlpContentRestrictionSet& other) const;
  bool operator!=(const DlpContentRestrictionSet& other) const;

  // Sets the |restriction| to the |level| if not set to a higher one yet.
  void SetRestriction(DlpContentRestriction restriction,
                      DlpRulesManager::Level level);

  // Returns the level for the |restriction|.
  DlpRulesManager::Level GetRestriction(
      DlpContentRestriction restriction) const;

  // Returns whether no restrictions should be applied.
  bool IsEmpty() const;

  // Sets all the restrictions to the highest level from |other| and this.
  void UnionWith(const DlpContentRestrictionSet& other);

  // Returns a new set that contains restrictions that exist in this, but not in
  // |other|.
  DlpContentRestrictionSet DifferenceWith(
      const DlpContentRestrictionSet& other) const;

 private:
  // The current level of each of the restrictions.
  std::array<DlpRulesManager::Level, DlpContentRestriction::kMaxValue + 1>
      restrictions_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_RESTRICTION_SET_H_
