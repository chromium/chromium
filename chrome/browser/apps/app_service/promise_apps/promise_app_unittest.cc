// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/promise_apps/promise_app.h"

#include "components/services/app_service/public/cpp/package_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class PromiseAppsTest : public testing::Test {};

TEST_F(PromiseAppsTest, PromiseStatusEnumToString) {
  EXPECT_EQ(EnumToString(PromiseStatus::kPending), "PromiseStatus::kPending");
  EXPECT_EQ(EnumToString(PromiseStatus::kInstalling),
            "PromiseStatus::kInstalling");
  EXPECT_EQ(EnumToString(PromiseStatus::kUnknown), "PromiseStatus::kUnknown");
}

}  // namespace apps
