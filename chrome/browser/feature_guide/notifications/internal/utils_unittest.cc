// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_guide/notifications/internal/utils.h"

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/feature_engagement/public/feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_guide {

TEST(FeatureNotificationUtilsTest, FeatureToCustomData) {
  std::map<std::string, std::string> custom_data;
  FeatureToCustomData(FeatureType::kVoiceSearch, &custom_data);
  EXPECT_EQ(FeatureType::kVoiceSearch, FeatureFromCustomData(custom_data));
}

}  // namespace feature_guide
