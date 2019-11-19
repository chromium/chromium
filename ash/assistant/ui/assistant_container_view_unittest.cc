// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_container_view.h"

#include <utility>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/services/assistant/test_support/mock_assistant.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace ash {

namespace {

constexpr int kMarginBottomDip = 8;

class AssistantContainerViewTest : public AshTestBase {
 public:
  AssistantContainerViewTest() = default;
  ~AssistantContainerViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    // Cache controller.
    controller_ = Shell::Get()->assistant_controller();
    DCHECK(controller_);

    // Cache UI controller.
    ui_controller_ = controller_->ui_controller();
    DCHECK(ui_controller_);

    // Enable Assistant in settings.
    Shell::Get()->session_controller()->GetPrimaryUserPrefService()->SetBoolean(
        chromeos::assistant::prefs::kAssistantEnabled, true);

    // After mocks are set up our Assistant service is ready for use. Indicate
    // this by changing status from NOT_READY to STOPPED.
    AssistantState::Get()->NotifyStatusChanged(mojom::AssistantState::READY);
  }

  AssistantUiController* ui_controller() { return ui_controller_; }

 private:
  AssistantController* controller_ = nullptr;
  AssistantUiController* ui_controller_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AssistantContainerViewTest);
};

}  // namespace

TEST_F(AssistantContainerViewTest, InitialAnchoring) {
  // Guarantee short but non-zero duration for animations.
  ui::ScopedAnimationDurationScaleMode scoped_animation_duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Show Assistant UI and grab a reference to our view under test.
  ui_controller()->ShowUi(AssistantEntryPoint::kUnspecified);
  AssistantContainerView* view = ui_controller()->GetViewForTest();

  // We expect the view to appear in the work area where new windows will open.
  gfx::Rect expected_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();

  // We expect the view to be horizontally centered and bottom aligned.
  gfx::Rect expected_bounds = gfx::Rect(expected_work_area);
  expected_bounds.ClampToCenteredSize(view->size());
  expected_bounds.set_y(expected_work_area.bottom() - view->height() -
                        kMarginBottomDip);

  ASSERT_EQ(expected_bounds, view->GetBoundsInScreen());
}

}  // namespace ash
