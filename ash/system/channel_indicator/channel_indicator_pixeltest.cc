// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/channel_indicator/channel_indicator_utils.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_helper.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "ash/test_shell_delegate.h"
#include "components/session_manager/session_manager_types.h"
#include "components/version_info/channel.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

// Pixel tests for the quick settings `UnifiedSliderView`.
class ChannelIndicatorPixelTest
    : public AshTestBase,
      public testing::WithParamInterface<
          std::tuple<version_info::Channel,
                     /*IsHorizontal()=*/bool,
                     /*IsLoggedIn()=*/bool,
                     /*IsSystemBlurEnabled()=*/bool>> {
 public:
  ChannelIndicatorPixelTest() = default;

  // AshTestBase:
  void SetUp() override {
    // Instantiate a `TestShellDelegate` with the channel set to our param.
    std::unique_ptr<TestShellDelegate> shell_delegate =
        std::make_unique<TestShellDelegate>();
    shell_delegate->set_channel(GetChannel());
    set_shell_delegate(std::move(shell_delegate));
    AshTestBase::SetUp();
    GetSessionControllerClient()->SetSessionState(
        IsLoggedIn() ? session_manager::SessionState::ACTIVE
                     : session_manager::SessionState::LOGIN_PRIMARY);
    GetPrimaryShelf()->SetAlignment(IsHorizontal() ? ShelfAlignment::kBottom
                                                   : ShelfAlignment::kRight);
  }

  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    pixel_test::InitParams init_params;
    init_params.system_blur_enabled = IsSystemBlurEnabled();
    return init_params;
  }

  version_info::Channel GetChannel() const { return std::get<0>(GetParam()); }
  bool IsHorizontal() const { return std::get<1>(GetParam()); }
  bool IsLoggedIn() const { return std::get<2>(GetParam()); }
  bool IsSystemBlurEnabled() const { return std::get<3>(GetParam()); }

  std::string GenerateScreenshotName(const std::string& title) override {
    return pixel_test_helper()->GenerateScreenshotName(
        title + (IsHorizontal() ? "_Horizontal" : "_Vertical") +
        (IsLoggedIn() ? "_LoggedIn" : "_LoggedOut"));
  }
};

const version_info::Channel kVersions[] = {version_info::Channel::BETA,
                                           version_info::Channel::DEV,
                                           version_info::Channel::CANARY};

// Run the `Visible` test below for each combination of Channel, shelf alignment
// and login state.
INSTANTIATE_TEST_SUITE_P(
    All,
    ChannelIndicatorPixelTest,
    testing::Combine(testing::ValuesIn(kVersions),
                     /*IsHorizontal()=*/testing::Bool(),
                     /*IsLoggedIn()=*/testing::Bool(),
                     /*IsSystemBlurEnabled()=*/testing::Bool()));

TEST_P(ChannelIndicatorPixelTest, ChannelIndicatorArea) {
  // The shelf can not be vertical on the login screen so skip this case.
  if (!IsHorizontal() && !IsLoggedIn()) {
    GTEST_SKIP();
  }

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GenerateScreenshotName(l10n_util::GetStringUTF8(
          channel_indicator_utils::GetChannelNameStringResourceID(GetChannel(),
                                                                  false))),
      /*revision_number=*/pixel_test_helper()->IsSystemBlurEnabled() ? 2 : 0,
      GetPrimaryShelf()->GetStatusAreaWidget()));
}

}  // namespace ash
