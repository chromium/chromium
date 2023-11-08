// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/dictation_nudge.h"
#include "ash/accessibility/dictation_nudge_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ash/system/tray/system_nudge_label.h"
#include "ash/test/ash_test_base.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

using ::testing::HasSubstr;

namespace ash {

namespace {

constexpr char kDictationLanguageUpgradedNudgeId[] =
    "dictation_language_upgraded.nudge_id";

}  // namespace

// Tests for showing the Dictation Nudge from AccessibilityControllerImpl.
class DictationNudgeControllerTest : public AshTestBase,
                                     public testing::WithParamInterface<bool> {
 public:
  DictationNudgeControllerTest() = default;
  DictationNudgeControllerTest(const DictationNudgeControllerTest&) = delete;
  DictationNudgeControllerTest& operator=(const DictationNudgeControllerTest&) =
      delete;
  ~DictationNudgeControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(features::kSystemNudgeMigration,
                                              IsSystemNudgeMigrationEnabled());
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

  std::unique_ptr<SystemNudgeLabel> GetDictationNudgeLabel(
      DictationNudge* nudge) {
    return nudge->CreateLabelView();
  }

  void WaitForWidgetDestruction(DictationNudgeController* controller,
                                SystemNudge* nudge) {
    views::Widget* nudge_widget = nudge->widget();
    ASSERT_TRUE(nudge_widget);
    EXPECT_FALSE(nudge_widget->IsClosed());

    // Slow down the duration of the nudge.
    ui::ScopedAnimationDurationScaleMode test_duration_mode(
        ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);

    // Pretend the hide nudge timer has elapsed.
    views::test::WidgetDestroyedWaiter widget_destroyed_waiter(nudge_widget);
    controller->FireHideNudgeTimerForTesting();

    EXPECT_TRUE(nudge_widget->GetLayer()->GetAnimator()->is_animating());

    widget_destroyed_waiter.Wait();
  }

  bool IsSystemNudgeMigrationEnabled() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         DictationNudgeControllerTest,
                         /*enable_system_nudge_migration=*/testing::Bool());

TEST_P(DictationNudgeControllerTest, ShowsAndHidesNudge) {
  if (IsSystemNudgeMigrationEnabled()) {
    EXPECT_FALSE(AnchoredNudgeManager::Get()->IsNudgeShown(
        kDictationLanguageUpgradedNudgeId));
    ShowDictationLanguageUpgradedNudge("en-US", "en-US");
    EXPECT_TRUE(AnchoredNudgeManager::Get()->IsNudgeShown(
        kDictationLanguageUpgradedNudgeId));

    // Manager handled nudge's destruction is tested in
    // anchored_nudge_manager_impl_unittest.cc.
    return;
  }

  EXPECT_FALSE(GetDictationNudgeController());

  ShowDictationLanguageUpgradedNudge("en-US", "en-US");

  DictationNudgeController* controller = GetDictationNudgeController();
  ASSERT_TRUE(controller);

  SystemNudge* nudge = controller->GetSystemNudgeForTesting();
  ASSERT_TRUE(nudge);

  WaitForWidgetDestruction(controller, nudge);
}

TEST_P(DictationNudgeControllerTest, SetsLabelBasedOnApplicationLocale) {
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
    if (IsSystemNudgeMigrationEnabled()) {
      ASSERT_TRUE(AnchoredNudgeManager::Get()->IsNudgeShown(
          kDictationLanguageUpgradedNudgeId));

      const std::string body_text = base::UTF16ToUTF8(
          Shell::Get()->anchored_nudge_manager()->GetNudgeBodyTextForTest(
              kDictationLanguageUpgradedNudgeId));
      EXPECT_THAT(body_text, HasSubstr(testcase.label));
    } else {
      DictationNudgeController* controller = GetDictationNudgeController();
      ASSERT_TRUE(controller);

      DictationNudge* nudge =
          static_cast<DictationNudge*>(controller->GetSystemNudgeForTesting());
      ASSERT_TRUE(nudge);

      std::unique_ptr<SystemNudgeLabel> label = GetDictationNudgeLabel(nudge);
      std::string text = base::UTF16ToUTF8(label->GetText());
      EXPECT_THAT(text, HasSubstr(testcase.label));

      WaitForWidgetDestruction(controller, nudge);
    }
  }
}

}  // namespace ash
