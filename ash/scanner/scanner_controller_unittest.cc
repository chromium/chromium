// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_controller.h"

#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/scanner/scanner_action.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/auto_reset.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

using ::testing::IsEmpty;
using ::testing::Not;

class ScannerControllerTest : public AshTestBase {
 public:
  ScannerControllerTest() = default;
  ScannerControllerTest(const ScannerControllerTest&) = delete;
  ScannerControllerTest& operator=(const ScannerControllerTest&) = delete;
  ~ScannerControllerTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{features::kScannerUpdate};
  base::AutoReset<bool> ignore_scanner_update_secret_key_ =
      switches::SetIgnoreScannerUpdateSecretKeyForTest();
};

TEST_F(ScannerControllerTest, FetchesActionsDuringActiveSession) {
  base::test::TestFuture<std::vector<ScannerAction>> actions_future;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  EXPECT_TRUE(scanner_controller->StartNewSession());

  scanner_controller->FetchActionsForImage(/*jpeg_bytes=*/nullptr,
                                           actions_future.GetCallback());

  EXPECT_THAT(actions_future.Take(), Not(IsEmpty()));
}

TEST_F(ScannerControllerTest, NoActionsFetchedWhenNoActiveSession) {
  base::test::TestFuture<std::vector<ScannerAction>> actions_future;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);

  scanner_controller->FetchActionsForImage(/*jpeg_bytes=*/nullptr,
                                           actions_future.GetCallback());

  EXPECT_THAT(actions_future.Take(), IsEmpty());
}

}  // namespace

}  // namespace ash
