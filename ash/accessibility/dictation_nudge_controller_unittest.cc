// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/dictation_nudge.h"
#include "ash/accessibility/dictation_nudge_controller.h"
#include "ash/shell.h"
#include "ash/system/tray/system_nudge.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

using ::testing::HasSubstr;

namespace ash {
namespace {

class NudgeWigetObserver : public views::WidgetObserver {
 public:
  NudgeWigetObserver(views::Widget* widget) : widget_(widget) {
    if (!widget_)
      return;

    widget_->AddObserver(this);
  }

  ~NudgeWigetObserver() override { CleanupWidget(); }

  void WaitForClose() {
    if (!widget_)
      return;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override {
    CleanupWidget();
    if (run_loop_)
      run_loop_->Quit();
  }

  void CleanupWidget() {
    if (!widget_)
      return;
    widget_->RemoveObserver(this);
    widget_ = nullptr;
  }

 private:
  views::Widget* widget_;
  std::unique_ptr<base::RunLoop> run_loop_;
};
}  // namespace

// Tests for showing the Dictation Nudge from AccessibilityControllerImpl.
class DictationNudgeControllerTest : public AshTestBase {
 public:
  DictationNudgeControllerTest() = default;
  DictationNudgeControllerTest(const DictationNudgeControllerTest&) = delete;
  DictationNudgeControllerTest& operator=(const DictationNudgeControllerTest&) =
      delete;
  ~DictationNudgeControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    Shell::Get()->accessibility_controller()->dictation().SetEnabled(true);
  }

  void ShowDictationLanguageUpgradedNudge(std::string dictation_locale,
                                          std::string application_locale) {
    Shell::Get()
        ->accessibility_controller()
        ->ShowDictationLanguageUpgradedNudge(dictation_locale,
                                             application_locale);
  }

  DictationNudgeController* GetDictationNudgeController() {
    return Shell::Get()
        ->accessibility_controller()
        ->GetDictationNudgeControllerForTest();
  }

  std::unique_ptr<views::View> GetDictationNudgeLabel(DictationNudge* nudge) {
    return nudge->CreateLabelView();
  }

  void WaitForWidgetClose(DictationNudgeController* controller,
                          SystemNudge* nudge) {
    views::Widget* nudge_widget = nudge->widget();
    ASSERT_TRUE(nudge_widget);
    EXPECT_FALSE(nudge_widget->IsClosed());

    // Slow down the duration of the nudge.
    ui::ScopedAnimationDurationScaleMode test_duration_mode(
        ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);

    // Pretend the hide nudge timer has elapsed.
    NudgeWigetObserver widget_close_observer(nudge_widget);
    controller->FireHideNudgeTimerForTesting();

    EXPECT_TRUE(nudge_widget->GetLayer()->GetAnimator()->is_animating());

    widget_close_observer.WaitForClose();
  }
};

TEST_F(DictationNudgeControllerTest, ShowsAndHidesNudge) {
  EXPECT_FALSE(GetDictationNudgeController());

  ShowDictationLanguageUpgradedNudge("en-US", "en-US");

  DictationNudgeController* controller = GetDictationNudgeController();
  ASSERT_TRUE(controller);

  SystemNudge* nudge = controller->GetSystemNudgeForTesting();
  ASSERT_TRUE(nudge);

  WaitForWidgetClose(controller, nudge);
}

TEST_F(DictationNudgeControllerTest, SetsLabelBasedOnApplicationLocale) {
  struct {
    std::string locale;
    std::string application_locale;
    std::string label;
  } kTestCases[] = {
      {"en-US", "en-US", "English"},
      {"es-ES", "en-US", "Spanish"},
      {"en-US", "es-ES", "inglés"},
      {"es-ES", "es-ES", "español"},
  };
  for (const auto& testcase : kTestCases) {
    ShowDictationLanguageUpgradedNudge(testcase.locale,
                                       testcase.application_locale);

    DictationNudgeController* controller = GetDictationNudgeController();
    ASSERT_TRUE(controller);

    DictationNudge* nudge =
        static_cast<DictationNudge*>(controller->GetSystemNudgeForTesting());
    ASSERT_TRUE(nudge);

    std::unique_ptr<views::View> label = GetDictationNudgeLabel(nudge);
    std::string text =
        base::UTF16ToUTF8(static_cast<views::Label*>(label.get())->GetText());
    EXPECT_THAT(text, HasSubstr(testcase.label));

    WaitForWidgetClose(controller, nudge);
  }
}

}  // namespace ash
