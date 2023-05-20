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
// For the most part borealis is allowed/denied based on hardware configuration
// (model, CPU, RAM) but we provide developers with the ability to override
// those checks using "tokens".
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
  // Helper method that performs different checks based on the user's board.
  BorealisFeatures::AllowStatus BoardSpecificChecks() const;

  // Returns the true if the board's CPU matches the given |cpu_regex| and RAM
  // is more than 7G.
  bool HasSufficientHardware(const std::string& cpu_regex) const;

  // Performs a token check for the |expected| token with the given |salt|. If
  // the token matches we log a WARNING, identifying the token by |name|, since
  // we only expect tokens to be used on unreleased/underpowered devices by
  // internal developers.
  bool HasNamedToken(const char* name,
                     const char* salt,
                     const char* expected) const;

  // Determines if this hardware has the correct segmentation parameters (see
  // b/274537000 for details).
  bool InTargetSegment() const;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_TOKEN_HARDWARE_CHECKER_H_
