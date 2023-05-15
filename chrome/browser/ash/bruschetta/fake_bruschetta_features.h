// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BRUSCHETTA_FAKE_BRUSCHETTA_FEATURES_H_
#define CHROME_BROWSER_ASH_BRUSCHETTA_FAKE_BRUSCHETTA_FEATURES_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/bruschetta/bruschetta_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace bruschetta {

// FakeBruschettaFeatures implements a fake version of BruschettaFeatures which
// can be used for testing.  It captures the current global BruschettaFeatures
// object and replaces it for the scope of this object.  It overrides only the
// features that you set and uses the previous object for other features.
class FakeBruschettaFeatures : public BruschettaFeatures {
 public:
  FakeBruschettaFeatures();
  ~FakeBruschettaFeatures() override;

  // BruschettaFeatures:
  bool IsEnabled() override;

  void SetAll(bool flag);
  void ClearAll();

  void set_enabled(bool enabled) { enabled_ = enabled; }

 private:
  // Original global static when this instance is created. It is captured when
  // FakeBruschettaFeatures is created and replaced at destruction.
  raw_ptr<BruschettaFeatures, ExperimentalAsh> original_features_;

  absl::optional<bool> enabled_;
};

}  // namespace bruschetta

#endif  // CHROME_BROWSER_ASH_BRUSCHETTA_FAKE_BRUSCHETTA_FEATURES_H_
