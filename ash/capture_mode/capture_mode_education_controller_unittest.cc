// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_education_controller.h"

#include "ash/accelerators/accelerator_tracker.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/shell.h"
#include "ash/style/keyboard_shortcut_view.h"
#include "ash/system/toast/anchored_nudge.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

namespace {

constexpr char kCaptureModeNudgeId[] = "kCaptureModeNudge";

constexpr char kNudgeTimeToActionWithin1m[] =
    "Ash.NotifierFramework.Nudge.TimeToAction.Within1m";
constexpr char kNudgeTimeToActionWithin1h[] =
    "Ash.NotifierFramework.Nudge.TimeToAction.Within1h";
constexpr char kNudgeTimeToActionWithinSession[] =
    "Ash.NotifierFramework.Nudge.TimeToAction.WithinSession";

PrefService* GetPrefService() {
  return Shell::Get()->session_controller()->GetActivePrefService();
}

void CancelNudge(const std::string& id) {
  Shell::Get()->anchored_nudge_manager()->Cancel(id);
}

}  // namespace

class CaptureModeEducationControllerTest : public AshTestBase {
 public:
  CaptureModeEducationControllerTest(const std::string& arm_name = "")
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    // `kSystemNudgeV2` must be initialized before the test starts as otherwise
    // `AnchoredNudgeManagerImpl` will not be created by the shell.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kSystemNudgeV2, {}},
         {features::kCaptureModeEducation,
          {{"CaptureModeEducationParam", arm_name}}}},
        /*disabled_features=*/{});
  }
  CaptureModeEducationControllerTest(
      const CaptureModeEducationControllerTest&) = delete;
  CaptureModeEducationControllerTest& operator=(
      const CaptureModeEducationControllerTest&) = delete;
  ~CaptureModeEducationControllerTest() override = default;

  static void SetOverrideClock(base::Clock* test_clock) {
    CaptureModeEducationController::SetOverrideClockForTesting(test_clock);
  }

  void ActivateNudgeAndCheckVisibility() {
    // Attempt to use the Windows Snipping Tool (capture bar) shortcut.
    PressAndReleaseKey(ui::VKEY_S, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);

    // Get the list of visible nudges from the nudge manager and make sure our
    // education nudge is in the list and visible.
    const AnchoredNudge* nudge =
        Shell::Get()->anchored_nudge_manager()->GetNudgeIfShown(
            kCaptureModeNudgeId);
    ASSERT_TRUE(nudge);
    EXPECT_TRUE(nudge->GetVisible());
  }

  // Skip the 3 times/24 hours show limit for testing.
  void SkipNudgePrefs() {
    CaptureModeController::Get()->education_controller()->skip_prefs_for_test_ =
        true;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(CaptureModeEducationControllerTest, Exists) {
  // Check that the default arm is enabled.
  ASSERT_EQ(features::kCaptureModeEducationParam.Get(),
            features::CaptureModeEducationParam::kShortcutNudge);
  ASSERT_TRUE(CaptureModeController::Get()->education_controller());
}

TEST_F(CaptureModeEducationControllerTest, NudgeAppearsOnAcceleratorPressed) {
  base::SimpleTestClock test_clock;
  CaptureModeEducationControllerTest::SetOverrideClock(&test_clock);

  // We will need to show the nudge more than three times, so ignore pref
  // limits.
  SkipNudgePrefs();

  for (auto [tracker_data, metadata] : kAcceleratorTrackerList) {
    // We only want to test capture mode related misinputs.
    if (metadata.type != TrackerType::kCaptureMode) {
      continue;
    }

    ActivateNudgeAndCheckVisibility();

    // Close nudge to get ready for the next input.
    CancelNudge(kCaptureModeNudgeId);
  }
}

// Test fixture to verify the behaviour of Arm 1, the shortcut nudge.
class CaptureModeEducationShortcutNudgeTest
    : public CaptureModeEducationControllerTest {
 public:
  CaptureModeEducationShortcutNudgeTest()
      : CaptureModeEducationControllerTest("ShortcutNudge") {}
  CaptureModeEducationShortcutNudgeTest(
      const CaptureModeEducationShortcutNudgeTest&) = delete;
  CaptureModeEducationShortcutNudgeTest& operator=(
      const CaptureModeEducationShortcutNudgeTest&) = delete;
  ~CaptureModeEducationShortcutNudgeTest() override = default;
};

TEST_F(CaptureModeEducationShortcutNudgeTest, CorrectStyling) {
  base::SimpleTestClock test_clock;
  CaptureModeEducationControllerTest::SetOverrideClock(&test_clock);

  // Advance clock so we aren't at zero time.
  test_clock.Advance(base::Hours(25));

  // Attempt to use the Windows Snipping Tool (capture bar) shortcut.
  PressAndReleaseKey(ui::VKEY_S, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);

  // Get the list of visible nudges from the nudge manager and make sure our
  // education nudge is in the list and visible.
  AnchoredNudge* nudge =
      Shell::Get()->anchored_nudge_manager()->GetNudgeIfShown(
          kCaptureModeNudgeId);
  ASSERT_TRUE(nudge);
  EXPECT_TRUE(nudge->GetVisible());

  // The keyboard shortcut view should be visible.
  auto* shortcut_view = views::AsViewClass<KeyboardShortcutView>(
      nudge->GetContentsView()->GetViewByID(
          VIEW_ID_SYSTEM_NUDGE_SHORTCUT_VIEW));
  ASSERT_TRUE(shortcut_view);
  EXPECT_TRUE(shortcut_view->GetVisible());

  CaptureModeEducationControllerTest::SetOverrideClock(nullptr);
}

TEST_F(CaptureModeEducationShortcutNudgeTest, EducationPreferencesShowLimit) {
  base::SimpleTestClock test_clock;
  CaptureModeEducationControllerTest::SetOverrideClock(&test_clock);

  // Advance clock so we aren't at zero time.
  test_clock.Advance(base::Hours(25));

  AnchoredNudgeManagerImpl* nudge_manager =
      Shell::Get()->anchored_nudge_manager();
  for (int i = 0; i < 3; i++) {
    ActivateNudgeAndCheckVisibility();

    // Showing the nudge should also update the preferences.
    EXPECT_EQ(
        GetPrefService()->GetInteger(prefs::kCaptureModeEducationShownCount),
        i + 1);

    // Close the nudge.
    CancelNudge(kCaptureModeNudgeId);

    // Advance the clock so we can show the nudge again.
    test_clock.Advance(base::Hours(25));
  }

  // Activate the nudge once more.
  PressAndReleaseKey(ui::VKEY_S, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);

  // The nudge should not be visible.
  const AnchoredNudge* nudge =
      nudge_manager->GetNudgeIfShown(kCaptureModeNudgeId);
  ASSERT_FALSE(nudge);

  // The nudge count should not increment as the nudge was not shown.
  EXPECT_EQ(
      GetPrefService()->GetInteger(prefs::kCaptureModeEducationShownCount), 3);

  CaptureModeEducationControllerTest::SetOverrideClock(nullptr);
}

TEST_F(CaptureModeEducationShortcutNudgeTest, EducationPreferencesTimeLimit) {
  base::SimpleTestClock test_clock;
  CaptureModeEducationControllerTest::SetOverrideClock(&test_clock);

  // Advance clock so we aren't at zero time.
  test_clock.Advance(base::Hours(25));

  ActivateNudgeAndCheckVisibility();

  // Showing the nudge should also update the preferences.
  EXPECT_EQ(
      GetPrefService()->GetInteger(prefs::kCaptureModeEducationShownCount), 1);

  // Close the nudge.
  CancelNudge(kCaptureModeNudgeId);

  // Advance the clock, but not more than 24 hours.
  test_clock.Advance(base::Hours(20));

  // Activate the nudge once more.
  PressAndReleaseKey(ui::VKEY_S, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);

  // The nudge should not be visible.
  const AnchoredNudge* null_nudge =
      Shell::Get()->anchored_nudge_manager()->GetNudgeIfShown(
          kCaptureModeNudgeId);
  ASSERT_FALSE(null_nudge);

  // The nudge count should not increment as the nudge was not shown.
  EXPECT_EQ(
      GetPrefService()->GetInteger(prefs::kCaptureModeEducationShownCount), 1);

  CaptureModeEducationControllerTest::SetOverrideClock(nullptr);
}

// Tests that metrics relating to the shortcut nudge (Arm 1) are properly
// recorded.
TEST_F(CaptureModeEducationShortcutNudgeTest, ShortcutNudgeMetrics) {
  base::HistogramTester histogram_tester;

  // For this test, we do not care about pref limits.
  SkipNudgePrefs();

  // The nudge has not been activated, so all related buckets should be at 0.
  Shell::Get()->anchored_nudge_manager()->ResetNudgeRegistryForTesting();
  histogram_tester.ExpectBucketCount(
      kNudgeTimeToActionWithin1m,
      NudgeCatalogName::kCaptureModeEducationShortcutNudge, 0);
  histogram_tester.ExpectBucketCount(
      kNudgeTimeToActionWithin1h,
      NudgeCatalogName::kCaptureModeEducationShortcutNudge, 0);
  histogram_tester.ExpectBucketCount(
      kNudgeTimeToActionWithinSession,
      NudgeCatalogName::kCaptureModeEducationShortcutNudge, 0);

  ActivateNudgeAndCheckVisibility();

  // Attempt to use the screenshot shortcut as soon as possible.
  PressAndReleaseKey(ui::VKEY_MEDIA_LAUNCH_APP1,
                     ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);

  // The buckets do not cascade, so a nudge that has been activated within 1m
  // will not show up in the `kNudgeTimeToActionWithin1h` bucket or session
  // bucket.
  histogram_tester.ExpectBucketCount(
      kNudgeTimeToActionWithin1m,
      NudgeCatalogName::kCaptureModeEducationShortcutNudge, 1);
  histogram_tester.ExpectBucketCount(
      kNudgeTimeToActionWithin1h,
      NudgeCatalogName::kCaptureModeEducationShortcutNudge, 0);
  histogram_tester.ExpectBucketCount(
      kNudgeTimeToActionWithinSession,
      NudgeCatalogName::kCaptureModeEducationShortcutNudge, 0);

  // Close the capture session and nudge, and advance the clock so we can show
  // the nudge again.
  CaptureModeController::Get()->Stop();
  CancelNudge(kCaptureModeNudgeId);
  ActivateNudgeAndCheckVisibility();

  // Attempt to use the screenshot shortcut after more than 1 minute, but less
  // than 1 hour.
  task_environment()->FastForwardBy(base::Minutes(30));
  PressAndReleaseKey(ui::VKEY_MEDIA_LAUNCH_APP1,
                     ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
  histogram_tester.ExpectBucketCount(
      kNudgeTimeToActionWithin1m,
      NudgeCatalogName::kCaptureModeEducationShortcutNudge, 1);
  histogram_tester.ExpectBucketCount(
      kNudgeTimeToActionWithin1h,
      NudgeCatalogName::kCaptureModeEducationShortcutNudge, 1);
  histogram_tester.ExpectBucketCount(
      kNudgeTimeToActionWithinSession,
      NudgeCatalogName::kCaptureModeEducationShortcutNudge, 0);

  // Close the capture session and nudge, and advance the clock so we can show
  // the nudge again.
  CaptureModeController::Get()->Stop();
  CancelNudge(kCaptureModeNudgeId);
  ActivateNudgeAndCheckVisibility();

  // Attempt to use the screenshot shortcut after more than 1 hour.
  task_environment()->FastForwardBy(base::Hours(2));
  PressAndReleaseKey(ui::VKEY_MEDIA_LAUNCH_APP1,
                     ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN);
  histogram_tester.ExpectBucketCount(
      kNudgeTimeToActionWithin1m,
      NudgeCatalogName::kCaptureModeEducationShortcutNudge, 1);
  histogram_tester.ExpectBucketCount(
      kNudgeTimeToActionWithin1h,
      NudgeCatalogName::kCaptureModeEducationShortcutNudge, 1);
  histogram_tester.ExpectBucketCount(
      kNudgeTimeToActionWithinSession,
      NudgeCatalogName::kCaptureModeEducationShortcutNudge, 1);
}

}  // namespace ash
