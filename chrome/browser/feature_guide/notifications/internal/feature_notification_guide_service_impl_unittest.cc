// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_guide/notifications/internal/feature_notification_guide_service_impl.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace feature_guide {
namespace {

class FeatureNotificationGuideServiceImplTest : public testing::Test {
 public:
  FeatureNotificationGuideServiceImplTest() = default;
  ~FeatureNotificationGuideServiceImplTest() override = default;
};

TEST_F(FeatureNotificationGuideServiceImplTest, Initialize) {}

}  // namespace

}  // namespace feature_guide
