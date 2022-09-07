// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/channel_indicator/channel_indicator_utils.h"

#include <string>

#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "components/version_info/channel.h"
#include "ui/gfx/color_palette.h"

namespace ash {
namespace {

// OS version that's set, and final button string that's expected.
const char* kTestOsVersion = "123.45.6789.10";
const char16_t* kTestButtonStr = u"Beta 123.45.6789.10";

}  // namespace

class ChannelIndicatorUtilsTest : public AshTestBase {
 public:
  ChannelIndicatorUtilsTest() = default;
  ChannelIndicatorUtilsTest(const ChannelIndicatorUtilsTest&) = delete;
  ChannelIndicatorUtilsTest& operator=(const ChannelIndicatorUtilsTest&) =
      delete;
  ~ChannelIndicatorUtilsTest() override = default;

  // AshTestBase:
  void SetUp() override {
    // Instantiate a `TestShellDelegate` with the version set to something
    // tests can verify.
    std::unique_ptr<TestShellDelegate> shell_delegate =
        std::make_unique<TestShellDelegate>();
    shell_delegate->set_version_string(kTestOsVersion);
    AshTestBase::SetUp(std::move(shell_delegate));
  }
};

TEST_F(ChannelIndicatorUtilsTest, IsDisplayableChannel) {
  EXPECT_FALSE(channel_indicator_utils::IsDisplayableChannel(
      version_info::Channel::UNKNOWN));
  EXPECT_TRUE(channel_indicator_utils::IsDisplayableChannel(
      version_info::Channel::CANARY));
  EXPECT_TRUE(channel_indicator_utils::IsDisplayableChannel(
      version_info::Channel::DEV));
  EXPECT_TRUE(channel_indicator_utils::IsDisplayableChannel(
      version_info::Channel::BETA));
  EXPECT_FALSE(channel_indicator_utils::IsDisplayableChannel(
      version_info::Channel::STABLE));
}

TEST_F(ChannelIndicatorUtilsTest, GetChannelNameStringResourceID) {
  // Non-displayable channel should yield a resource_id of -1.
  EXPECT_EQ(channel_indicator_utils::GetChannelNameStringResourceID(
                version_info::Channel::STABLE, false),
            -1);

  // Same thing if `append_channel` is `true`.
  EXPECT_EQ(channel_indicator_utils::GetChannelNameStringResourceID(
                version_info::Channel::STABLE, true),
            -1);

  // Displayable channel should yield a valid resource_id.
  EXPECT_EQ(channel_indicator_utils::GetChannelNameStringResourceID(
                version_info::Channel::BETA, false),
            IDS_ASH_STATUS_TRAY_CHANNEL_BETA);

  // An equally-valid resource_id if `append_channel` is `true`.
  EXPECT_EQ(channel_indicator_utils::GetChannelNameStringResourceID(
                version_info::Channel::BETA, true),
            IDS_ASH_STATUS_TRAY_CHANNEL_BETA_CHANNEL);
}

TEST_F(ChannelIndicatorUtilsTest, GetColors) {
  // Non-displayable channel should yield fg/bg colors of 0.
  EXPECT_EQ(channel_indicator_utils::GetFgColor(version_info::Channel::STABLE),
            SkColorSetRGB(0x00, 0x00, 0x00));
  EXPECT_EQ(channel_indicator_utils::GetBgColor(version_info::Channel::STABLE),
            SkColorSetRGB(0x00, 0x00, 0x00));

  // Displayable channel should yield valid, nonzero fg/bg colors. Check with
  // dark mode not enabled first.
  DarkLightModeController::Get()->SetDarkModeEnabledForTest(false);
  EXPECT_EQ(channel_indicator_utils::GetFgColor(version_info::Channel::BETA),
            gfx::kGoogleBlue900);
  EXPECT_EQ(channel_indicator_utils::GetBgColor(version_info::Channel::BETA),
            gfx::kGoogleBlue200);

  // Check with dark mode enabled.
  DarkLightModeController::Get()->SetDarkModeEnabledForTest(true);
  EXPECT_EQ(channel_indicator_utils::GetFgColor(version_info::Channel::BETA),
            gfx::kGoogleBlue200);
  EXPECT_EQ(channel_indicator_utils::GetBgColor(version_info::Channel::BETA),
            SkColorSetA(gfx::kGoogleBlue300, 0x55));
}

TEST_F(ChannelIndicatorUtilsTest, GetFullReleaseTrackString) {
  // Channel is not displayable, no string.
  EXPECT_TRUE(channel_indicator_utils::GetFullReleaseTrackString(
                  version_info::Channel::STABLE)
                  .empty());

  // Channel is displayable, string that's expected.
  EXPECT_EQ(channel_indicator_utils::GetFullReleaseTrackString(
                version_info::Channel::BETA),
            kTestButtonStr);
}

}  // namespace ash
