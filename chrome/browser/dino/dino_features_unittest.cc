// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dino/dino_features.h"

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dino {

TEST(DinoFeaturesTest, DinoDelightDisabledByDefault) {
  EXPECT_FALSE(base::FeatureList::IsEnabled(kDinoDelight));
}

TEST(DinoFeaturesTest, DinoDelightCanBeEnabled) {
  base::test::ScopedFeatureList scoped_feature_list(kDinoDelight);
  EXPECT_TRUE(base::FeatureList::IsEnabled(kDinoDelight));
}

}  // namespace dino
