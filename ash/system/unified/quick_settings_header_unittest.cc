// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/quick_settings_header.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

class QuickSettingsHeaderTest : public AshTestBase {
 public:
  QuickSettingsHeaderTest() {
    feature_list_.InitAndEnableFeature(features::kQsRevamp);
  }

  base::test::ScopedFeatureList feature_list_;
};

TEST_F(QuickSettingsHeaderTest, Construction) {
  QuickSettingsHeader header;
  // No crash.
}

}  // namespace ash
