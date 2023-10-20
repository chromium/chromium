// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/common/nearby_share_resource_getter.h"

#include <string>

#include "chrome/grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

class NearbyShareResourceGetterTest : public ::testing::Test {
 public:
  NearbyShareResourceGetterTest() {}

  ~NearbyShareResourceGetterTest() override = default;
};

TEST_F(NearbyShareResourceGetterTest,
       GetStringWithFeatureNameWorksWithPlaceholder) {
  // Expect the feature name to be inserted into the string.
  // TODO(b/304349796): Replace this string with a more straightforward example.
  EXPECT_EQ(NearbyShareResourceGetter::GetInstance()->GetStringWithFeatureName(
                IDS_NEARBY_HIGH_VISIBILITY_SUB_TITLE_SECONDS),
            u"Nearby Share sec");
}
