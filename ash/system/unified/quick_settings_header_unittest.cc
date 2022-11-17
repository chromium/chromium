// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/quick_settings_header.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "base/test/scoped_feature_list.h"
#include "components/version_info/channel.h"

namespace ash {

class QuickSettingsHeaderTest : public NoSessionAshTestBase {
 public:
  QuickSettingsHeaderTest() {
    feature_list_.InitWithFeatures(
        {features::kQsRevamp, features::kQsRevampWip}, {});
  }

  // AshTestBase:
  void SetUp() override {
    // Install a test delegate to allow overriding channel version.
    auto delegate = std::make_unique<TestShellDelegate>();
    test_shell_delegate_ = delegate.get();
    AshTestBase::SetUp(std::move(delegate));
  }

  base::test::ScopedFeatureList feature_list_;
  TestShellDelegate* test_shell_delegate_ = nullptr;
};

TEST_F(QuickSettingsHeaderTest, HiddenByDefaultBeforeLogin) {
  QuickSettingsHeader header;

  // By default, channel view is not created.
  EXPECT_FALSE(header.channel_view_for_test());

  // Since no views are created, the header is hidden.
  EXPECT_FALSE(header.GetVisible());
}

TEST_F(QuickSettingsHeaderTest, DoesNotShowChannelViewBeforeLogin) {
  test_shell_delegate_->set_channel(version_info::Channel::BETA);

  QuickSettingsHeader header;

  EXPECT_FALSE(header.channel_view_for_test());
  EXPECT_FALSE(header.GetVisible());
}

TEST_F(QuickSettingsHeaderTest, ShowsChannelViewAfterLogin) {
  test_shell_delegate_->set_channel(version_info::Channel::BETA);
  SimulateUserLogin("user@gmail.com");

  QuickSettingsHeader header;

  // Channel view is created.
  EXPECT_TRUE(header.channel_view_for_test());

  // Header is shown.
  EXPECT_TRUE(header.GetVisible());
}

}  // namespace ash
