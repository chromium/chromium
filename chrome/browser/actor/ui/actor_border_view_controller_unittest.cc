// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_border_view_controller.h"

#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace {

using ::testing::MockFunction;
using ::testing::ReturnRef;

class ActorBorderViewControllerTest : public testing::Test {
 public:
  ActorBorderViewControllerTest() {
    ON_CALL(mock_browser_window_interface_, GetUnownedUserDataHost)
        .WillByDefault(ReturnRef(user_data_host_));
    controller_ = std::make_unique<ActorBorderViewController>(
        &mock_browser_window_interface_);
  }

  ActorBorderViewController* actor_border_view_controller() {
    return ActorBorderViewController::From(&mock_browser_window_interface_);
  }

 protected:
  ::ui::UnownedUserDataHost user_data_host_;
  MockBrowserWindowInterface mock_browser_window_interface_;
  std::unique_ptr<ActorBorderViewController> controller_;
  tabs::MockTabInterface mock_tab_;
};

TEST_F(ActorBorderViewControllerTest, NotifiesOnGlowEnabledChanged) {
  MockFunction<void(tabs::TabInterface*, bool)> callback;

  auto subscription =
      actor_border_view_controller()->AddOnActorBorderGlowUpdatedCallback(
          base::BindRepeating(
              &testing::MockFunction<void(tabs::TabInterface*, bool)>::Call,
              base::Unretained(&callback)));

  EXPECT_CALL(callback, Call(&mock_tab_, true));
  actor_border_view_controller()->SetGlowEnabled(&mock_tab_, true);

  EXPECT_CALL(callback, Call(&mock_tab_, false));
  actor_border_view_controller()->SetGlowEnabled(&mock_tab_, false);
}

}  // namespace
