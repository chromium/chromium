// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/arc_ghost_window_handler.h"
#include <memory>
#include <optional>

#include "chromeos/ui/base/window_state_type.h"
#include "components/exo/wm_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::full_restore {

class ArcGhostWindowHandlerTest : public testing::Test {
 public:
  ArcGhostWindowHandlerTest() {
    wm_helper_ = std::make_unique<exo::WMHelper>();
    ghost_window_handler_ = std::make_unique<ArcGhostWindowHandler>();
  }
  ArcGhostWindowHandlerTest(const ArcGhostWindowHandlerTest&) = delete;
  ArcGhostWindowHandlerTest& operator=(const ArcGhostWindowHandlerTest&) =
      delete;
  ~ArcGhostWindowHandlerTest() override = default;

  ArcGhostWindowHandler* handler() { return ghost_window_handler_.get(); }

 private:
  std::unique_ptr<exo::WMHelper> wm_helper_;
  std::unique_ptr<ArcGhostWindowHandler> ghost_window_handler_;
};

TEST_F(ArcGhostWindowHandlerTest, Initialize) {}

TEST_F(ArcGhostWindowHandlerTest, UpdateOverrideBoundsIfGeneralState) {
  constexpr int SESSION_ID = 10001;
  constexpr gfx::Rect bounds(100, 150, 200, 300);
  handler()->OnWindowInfoUpdated(
      SESSION_ID, static_cast<int32_t>(chromeos::WindowStateType::kNormal),
      /*display_id=*/0, bounds);

  EXPECT_TRUE(handler()->session_id_to_pending_window_info_.count(SESSION_ID));
  EXPECT_EQ(handler()->session_id_to_pending_window_info_[SESSION_ID]->state,
            static_cast<int32_t>(chromeos::WindowStateType::kNormal));
  EXPECT_EQ(handler()->session_id_to_pending_window_info_[SESSION_ID]->bounds,
            bounds);
}

TEST_F(ArcGhostWindowHandlerTest, NotUpdateOverrideBoundsIfStateIsDefault) {
  constexpr int SESSION_ID = 10002;
  constexpr gfx::Rect bounds(100, 150, 200, 300);
  handler()->OnWindowInfoUpdated(
      SESSION_ID, static_cast<int32_t>(chromeos::WindowStateType::kDefault),
      /*display_id=*/0, bounds);

  EXPECT_TRUE(handler()->session_id_to_pending_window_info_.count(SESSION_ID));
  EXPECT_EQ(handler()->session_id_to_pending_window_info_[SESSION_ID]->state,
            static_cast<int32_t>(chromeos::WindowStateType::kDefault));
  EXPECT_EQ(handler()->session_id_to_pending_window_info_[SESSION_ID]->bounds,
            std::nullopt);
}

}  // namespace ash::full_restore
