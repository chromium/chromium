// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/channel_indicator/channel_indicator.h"

#include "ash/public/cpp/shelf_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/channel_indicator/channel_indicator_utils.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "components/session_manager/session_manager_types.h"
#include "components/version_info/channel.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {

// Constants for determining whether a view is "squished" i.e. one of its
// dimensions is very small and one dimension is much larger than the other.
constexpr int kSquishedMinDimension = 2;
constexpr int kSquishedMaxDifferenceBetweenDimensions = 2;

}  // namespace

class ChannelIndicatorViewTest
    : public AshTestBase,
      public testing::WithParamInterface<version_info::Channel> {
 public:
  ChannelIndicatorViewTest() = default;
  ChannelIndicatorViewTest(const ChannelIndicatorViewTest&) = delete;
  ChannelIndicatorViewTest& operator=(const ChannelIndicatorViewTest&) = delete;
  ~ChannelIndicatorViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    // Instantiate a `TestShellDelegate` with the channel set to our param.
    std::unique_ptr<TestShellDelegate> shell_delegate =
        std::make_unique<TestShellDelegate>();
    shell_delegate->set_channel(static_cast<version_info::Channel>(GetParam()));
    AshTestBase::SetUp(std::move(shell_delegate));
  }

  void SetSessionState(session_manager::SessionState state) {
    SessionInfo info;
    info.state = state;
    Shell::Get()->session_controller()->SetSessionInfo(info);
  }

  bool IsViewSquished(const views::View* view) {
    DCHECK(view);

    // A view is considered "squished" if:
    // (1) Either dimension of its bounds is very small and
    // (2) One dimension is much larger than the other
    gfx::Rect bounds = view->GetLocalBounds();
    bool is_squished = (bounds.width() <= kSquishedMinDimension ||
                        bounds.height() <= kSquishedMinDimension) &&
                       std::abs(bounds.width() - bounds.height()) >=
                           kSquishedMaxDifferenceBetweenDimensions;
    if (is_squished) {
      LOG(ERROR) << __FUNCTION__ << " view (w: " << bounds.width()
                 << " h: " << bounds.height() << ") is squished ";
    }
    return is_squished;
  }
};

// Run the `Visible` test below for each value of version_info::Channel.
INSTANTIATE_TEST_SUITE_P(ChannelValues,
                         ChannelIndicatorViewTest,
                         ::testing::Values(version_info::Channel::UNKNOWN,
                                           version_info::Channel::STABLE,
                                           version_info::Channel::BETA,
                                           version_info::Channel::DEV,
                                           version_info::Channel::CANARY));

TEST_P(ChannelIndicatorViewTest, Visible) {
  // Local ref.
  ShellDelegate* shell_delegate = Shell::Get()->shell_delegate();
  DCHECK(shell_delegate);

  // Local ref.
  UnifiedSystemTray* tray =
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()->unified_system_tray();
  ChannelIndicatorView* channel_indicator_view = tray->channel_indicator_view();

  // If the channel is not displayable, there should be no view and the test is
  // complete.
  if (!channel_indicator_utils::IsDisplayableChannel(
          shell_delegate->GetChannel())) {
    EXPECT_FALSE(channel_indicator_view);
    EXPECT_FALSE(tray->ShouldChannelIndicatorBeShown());
    return;
  }

  // Otherwise the view exists, should be shown, and is visible.
  EXPECT_TRUE(channel_indicator_view);
  EXPECT_TRUE(tray->ShouldChannelIndicatorBeShown());
  EXPECT_TRUE(channel_indicator_view->GetVisible());

  // User is not logged in, view should display text, no image.
  SetSessionState(session_manager::SessionState::LOGIN_PRIMARY);
  EXPECT_TRUE(channel_indicator_view->IsLabelVisibleForTesting());
  EXPECT_FALSE(channel_indicator_view->IsImageViewVisibleForTesting());

  // User is logged in, view should display image, no text.
  SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_FALSE(channel_indicator_view->IsLabelVisibleForTesting());
  EXPECT_TRUE(channel_indicator_view->IsImageViewVisibleForTesting());

  // Two shelf alignments to test.
  auto shelf_alignments = {ShelfAlignment::kBottom, ShelfAlignment::kRight};

  // Image is the right size in both alignments.
  for (const auto& alignment : shelf_alignments) {
    // Initiates the shelf alignment change.
    GetPrimaryShelf()->SetAlignment(alignment);

    // Perform a synchronous `Layout` of `channel_indicator_view` and its child
    // views.
    channel_indicator_view->GetWidget()->LayoutRootViewIfNecessary();

    // Now test the bounds of the image view.
    EXPECT_FALSE(IsViewSquished(
        GetPrimaryUnifiedSystemTray()->channel_indicator_view()->image_view()));
  }

  // User locks the session, view should display text, no image.
  SetSessionState(session_manager::SessionState::LOCKED);
  EXPECT_TRUE(channel_indicator_view->IsLabelVisibleForTesting());
  EXPECT_FALSE(channel_indicator_view->IsImageViewVisibleForTesting());

  // User is logged in again, view should display image, no text.
  SetSessionState(session_manager::SessionState::ACTIVE);
  EXPECT_FALSE(channel_indicator_view->IsLabelVisibleForTesting());
  EXPECT_TRUE(channel_indicator_view->IsImageViewVisibleForTesting());

  // Image is the right size in both alignments.
  for (const auto& alignment : shelf_alignments) {
    // Initiates the shelf alignment change.
    GetPrimaryShelf()->SetAlignment(alignment);

    // Perform a synchronous `Layout` of `channel_indicator_view` and its child
    // views.
    channel_indicator_view->GetWidget()->LayoutRootViewIfNecessary();

    // Now test the bounds of the image view.
    EXPECT_FALSE(IsViewSquished(
        GetPrimaryUnifiedSystemTray()->channel_indicator_view()->image_view()));
  }
}

TEST_P(ChannelIndicatorViewTest, AccessibleProperties) {
  ShellDelegate* shell_delegate = Shell::Get()->shell_delegate();
  UnifiedSystemTray* tray =
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()->unified_system_tray();
  ChannelIndicatorView* channel_indicator_view = tray->channel_indicator_view();
  if (!channel_indicator_utils::IsDisplayableChannel(
          shell_delegate->GetChannel())) {
    EXPECT_FALSE(channel_indicator_view);
    GTEST_SKIP()
        << "Test is only valid when channel indicator view is not null.";
  }

  ui::AXNodeData data;
  channel_indicator_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kLabelText);
}

}  // namespace ash
