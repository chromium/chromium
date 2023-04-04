// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_TOKEN_HARDWARE_CHECKER_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_TOKEN_HARDWARE_CHECKER_H_

#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_features_util.h"

namespace borealis {

// Checks the current hardware+token configuration to determine if the user
// should be able to run borealis.
//
// If you are supposed to know the correct token, then you will be able to
// find it ~if you go to the place we all know and love~.
class BorealisTokenHardwareChecker : public TokenHardwareChecker {
 public:
  static BorealisFeatures::AllowStatus BuildAndCheck(Data data);

  explicit BorealisTokenHardwareChecker(Data data);

  ~BorealisTokenHardwareChecker();

  BorealisFeatures::AllowStatus Check() const;

 private:
  // Returns the allow status for a standard released board.
  BorealisFeatures::AllowStatus ReleasedBoardChecks(
      const std::string& cpu_regex) const;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_TOKEN_HARDWARE_CHECKER_H_
