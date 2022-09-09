// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_FEATURES_H_
#define CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_FEATURES_H_

namespace bruschetta {

// BruschettaFeatures provides an interface for querying which parts of
// bruschetta are enabled or allowed.
class BruschettaFeatures {
 public:
  static BruschettaFeatures* Get();

  BruschettaFeatures(const BruschettaFeatures&) = delete;
  BruschettaFeatures& operator=(const BruschettaFeatures&) = delete;

  // Returns whether Bruschetta has been enabled.
  virtual bool IsEnabled();

 protected:
  static void SetForTesting(BruschettaFeatures* features);

  BruschettaFeatures();
  virtual ~BruschettaFeatures();
};

}  // namespace bruschetta

#endif  // CHROME_BROWSER_ASH_BRUSCHETTA_BRUSCHETTA_FEATURES_H_
