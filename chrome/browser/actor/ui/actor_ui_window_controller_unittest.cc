// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_window_controller.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/test/gtest_util.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/ui/actor_overlay_web_view.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/views/controls/webview/webview.h"

namespace actor::ui {
namespace {

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

class ActorUiContentsContainerControllerTest
    : public chrome_test_utils::TestingBrowserProcessDeathTestMixin,
      public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    ON_CALL(mock_window_interface_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(user_data_host_));
    ON_CALL(mock_window_interface_, GetProfile())
        .WillByDefault(Return(nullptr));

    contents_web_view_ = std::make_unique<views::WebView>(nullptr);
    overlay_ = std::make_unique<ActorOverlayWebView>(&mock_window_interface_);
    std::vector<std::pair<views::WebView*, ActorOverlayWebView*>> overlays;
    window_controller_ = std::make_unique<ActorUiWindowController>(
        &mock_window_interface_, std::move(overlays));
    controller_ = std::make_unique<ActorUiContentsContainerController>(
        contents_web_view_.get(), overlay_.get(), window_controller_.get());
  }

  void TearDown() override {
    controller_.reset();
    window_controller_.reset();
    overlay_.reset();
    contents_web_view_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  NiceMock<MockBrowserWindowInterface> mock_window_interface_;
  ::ui::UnownedUserDataHost user_data_host_;
  std::unique_ptr<views::WebView> contents_web_view_;
  std::unique_ptr<ActorOverlayWebView> overlay_;
  std::unique_ptr<ActorUiWindowController> window_controller_;
  std::unique_ptr<ActorUiContentsContainerController> controller_;
};

// TODO(crbug.com/41487832): Enable on ChromeOS when test setup in the death
// subprocess is fixed. AshTestHelper causes ICU/Timezone crashes in the forked
// process before the DCHECK can fire.
#if !BUILDFLAG(IS_CHROMEOS)
TEST_F(ActorUiContentsContainerControllerTest,
       OverlayStateMoveAndClickDeathCheck) {
  ActorOverlayState invalid_state;
  invalid_state.is_active = true;
  invalid_state.mouse_target = gfx::Point(100, 100);
  invalid_state.mouse_down = true;

  base::test::TestFuture<void> future;
  EXPECT_DCHECK_DEATH(controller_->OnOverlayStateChanged(
      /*is_visible=*/true, invalid_state, future.GetCallback()));
}
#endif

}  // namespace
}  // namespace actor::ui
