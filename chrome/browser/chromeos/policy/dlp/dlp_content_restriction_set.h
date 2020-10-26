// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_RESTRICTION_SET_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_RESTRICTION_SET_H_

#include <stdint.h>

namespace policy {

// Enum representing the possible restrictions applied to on-screen content.
// These values are used in bitmask in DlpContentRestrictionSet and should
// correspond to the type in which the mask is stored.
enum DlpContentRestriction {
  // Do not allow any screenshots of the corresponding content.
  kScreenshot = 1 << 0,
  // Enforce ePrivacy screen when content is visible.
  kPrivacyScreen = 1 << 1,
  // Do not allow printing.
  kPrint = 1 << 2,
  // Do not allow video capturing of the content.
  kVideoCapture = 1 << 3,
  // Do not allow screen share.
  kScreenShare = 1 << 4,
};

// Represents set of restrictions applied to on-screen content.
// Internally stores it in a single integer bitmask.
// Allowed to be copied and assigned.
class DlpContentRestrictionSet {
 public:
  DlpContentRestrictionSet();
  explicit DlpContentRestrictionSet(DlpContentRestriction restriction);

  DlpContentRestrictionSet(const DlpContentRestrictionSet& restriction_set);
  DlpContentRestrictionSet& operator=(const DlpContentRestrictionSet&);

  ~DlpContentRestrictionSet();

  bool operator==(const DlpContentRestrictionSet& other) const;
  bool operator!=(const DlpContentRestrictionSet& other) const;

  // Adds the restriction to the set if not yet.
  void SetRestriction(DlpContentRestriction restriction);

  // Returns whether the restriction is present in the set.
  bool HasRestriction(DlpContentRestriction restriction) const;

  // Returns whether no restrictions should be applied.
  bool IsEmpty() const { return restriction_mask_ == 0; }

  // Adds all the restrictions from |other| to this.
  void UnionWith(const DlpContentRestrictionSet& other);

  // Returns a new set that contains restrictions that exist in this, but not in
  // |other|.
  DlpContentRestrictionSet DifferenceWith(
      const DlpContentRestrictionSet& other) const;

 private:
  explicit DlpContentRestrictionSet(uint8_t mask);

  // Bitmask of the restrictions.
  uint8_t restriction_mask_ = 0;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_CONTENT_RESTRICTION_SET_H_
