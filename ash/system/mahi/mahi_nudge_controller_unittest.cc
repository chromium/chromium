// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_nudge_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "ash/test/ash_test_base.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "components/prefs/pref_service.h"
#include "ui/lottie/resource.h"

namespace ash {

namespace {

bool IsMahiNudgeShown() {
  return Shell::Get()->anchored_nudge_manager()->IsNudgeShown(
      mahi_constants::kMahiNudgeId);
}

// A class that mocks `MagicBoostState` to use in tests.
class TestMagicBoostState : public chromeos::MagicBoostState {
 public:
  TestMagicBoostState() = default;

  TestMagicBoostState(const TestMagicBoostState&) = delete;
  TestMagicBoostState& operator=(const TestMagicBoostState&) = delete;

  ~TestMagicBoostState() override = default;

  // chromeos::MagicBoostState:
  void AsyncWriteConsentStatus(
      chromeos::HMRConsentStatus consent_status) override {
    UpdateHMRConsentStatus(consent_status);
  }

  void AsyncWriteHMREnabled(bool enabled) override {
    UpdateHMREnabled(enabled);
  }

  bool IsMagicBoostAvailable() override { return true; }
  bool CanShowNoticeBannerForHMR() override { return false; }
  int32_t AsyncIncrementHMRConsentWindowDismissCount() override { return 0; }
  void DisableOrcaFeature() override {}
};

}  // namespace

class MahiNudgeControllerTest : public AshTestBase {
 public:
  MahiNudgeControllerTest() {
    mahi_nudge_controller_ = std::make_unique<MahiNudgeController>();

    // Sets the default functions for the test to create image with the lottie
    // resource id. Otherwise there's no `g_parse_lottie_as_still_image_` set in
    // the `ResourceBundle`.
    ui::ResourceBundle::SetLottieParsingFunctions(
        &lottie::ParseLottieAsStillImage,
        &lottie::ParseLottieAsThemedStillImage);

    test_magic_boost_state_.AsyncWriteConsentStatus(
        chromeos::HMRConsentStatus::kUnset);
  }

  MahiNudgeController* nudge_controller() {
    return mahi_nudge_controller_.get();
  }

  AnchoredNudgeManagerImpl* nudge_manager() {
    return Shell::Get()->anchored_nudge_manager();
  }

  static base::Time GetTestTime() { return test_time_; }
  static void SetTestTime(base::Time test_time) { test_time_ = test_time; }

 protected:
  TestMagicBoostState test_magic_boost_state_;

 private:
  std::unique_ptr<MahiNudgeController> mahi_nudge_controller_;
  static inline base::Time test_time_;
};

// Tests that the nudge can show when the user is not opted into Mahi.
TEST_F(MahiNudgeControllerTest, NudgeShows_WhenUserPrefNotEnabled) {
  test_magic_boost_state_.AsyncWriteHMREnabled(false);

  EXPECT_FALSE(IsMahiNudgeShown());
  nudge_controller()->MaybeShowNudge();
  EXPECT_TRUE(IsMahiNudgeShown());
}

// Tests that the nudge won't show when the user has opted into Mahi.
TEST_F(MahiNudgeControllerTest, NudgeDoesNotShow_WhenUserPrefEnabled) {
  test_magic_boost_state_.AsyncWriteHMREnabled(true);

  EXPECT_FALSE(IsMahiNudgeShown());
  nudge_controller()->MaybeShowNudge();
  EXPECT_FALSE(IsMahiNudgeShown());
}

// Tests that the nudge can show when the consent status is unset.
TEST_F(MahiNudgeControllerTest, NudgeShows_WhenConsentStatusIsUnset) {
  test_magic_boost_state_.AsyncWriteHMREnabled(false);
  test_magic_boost_state_.AsyncWriteConsentStatus(
      chromeos::HMRConsentStatus::kUnset);

  EXPECT_FALSE(IsMahiNudgeShown());
  nudge_controller()->MaybeShowNudge();
  EXPECT_TRUE(IsMahiNudgeShown());
}

// Tests that the nudge won't show when the consent status is set.
TEST_F(MahiNudgeControllerTest, NudgeDoesNotShow_WhenConsentStatusSet) {
  test_magic_boost_state_.AsyncWriteHMREnabled(false);

  test_magic_boost_state_.AsyncWriteConsentStatus(
      chromeos::HMRConsentStatus::kPendingDisclaimer);

  EXPECT_FALSE(IsMahiNudgeShown());
  nudge_controller()->MaybeShowNudge();
  EXPECT_FALSE(IsMahiNudgeShown());

  test_magic_boost_state_.AsyncWriteConsentStatus(
      chromeos::HMRConsentStatus::kApproved);

  EXPECT_FALSE(IsMahiNudgeShown());
  nudge_controller()->MaybeShowNudge();
  EXPECT_FALSE(IsMahiNudgeShown());

  test_magic_boost_state_.AsyncWriteConsentStatus(
      chromeos::HMRConsentStatus::kDeclined);

  EXPECT_FALSE(IsMahiNudgeShown());
  nudge_controller()->MaybeShowNudge();
  EXPECT_FALSE(IsMahiNudgeShown());
}

// Tests that the nudge won't show if the time between shown threshold hasn't
// passed since it was last shown.
TEST_F(MahiNudgeControllerTest, NudgeDoesNotShow_IfRecentlyShown) {
  test_magic_boost_state_.AsyncWriteHMREnabled(false);

  SetTestTime(base::Time::Now());
  base::subtle::ScopedTimeClockOverrides clock_override(
      /*time_override=*/&MahiNudgeControllerTest::GetTestTime,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr);

  // Show the nudge once and close it.
  EXPECT_FALSE(IsMahiNudgeShown());
  nudge_controller()->MaybeShowNudge();
  EXPECT_TRUE(IsMahiNudgeShown());
  nudge_manager()->Cancel(mahi_constants::kMahiNudgeId);

  // Attempt showing the nudge again immediately. It should not show.
  nudge_controller()->MaybeShowNudge();
  EXPECT_FALSE(IsMahiNudgeShown());

  // Attempt showing the nudge after some time but before its threshold time has
  // fully passed. It should not show.
  SetTestTime(GetTestTime() + mahi_constants::kNudgeTimeBetweenShown -
              base::Hours(1));
  nudge_controller()->MaybeShowNudge();
  EXPECT_FALSE(IsMahiNudgeShown());

  // Attempt showing the nudge after its "time between shown" threshold has
  // passed. It should show.
  SetTestTime(GetTestTime() + mahi_constants::kNudgeTimeBetweenShown +
              base::Hours(1));
  nudge_controller()->MaybeShowNudge();
  EXPECT_TRUE(IsMahiNudgeShown());
}

// Tests that the nudge won't show if it has been shown its max number of times.
TEST_F(MahiNudgeControllerTest, NudgeDoesNotShow_IfMaxTimesShown) {
  test_magic_boost_state_.AsyncWriteHMREnabled(false);

  SetTestTime(base::Time::Now());
  base::subtle::ScopedTimeClockOverrides clock_override(
      /*time_override=*/&MahiNudgeControllerTest::GetTestTime,
      /*time_ticks_override=*/nullptr, /*thread_ticks_override=*/nullptr);

  // Show the nudge its max number of times.
  for (int i = 0; i < mahi_constants::kNudgeMaxShownCount; i++) {
    nudge_controller()->MaybeShowNudge();
    EXPECT_TRUE(IsMahiNudgeShown());
    nudge_manager()->Cancel(mahi_constants::kMahiNudgeId);
    SetTestTime(GetTestTime() + mahi_constants::kNudgeTimeBetweenShown +
                base::Hours(1));
  }

  // Attempt showing the nudge once more. It should not show.
  nudge_controller()->MaybeShowNudge();
  EXPECT_FALSE(IsMahiNudgeShown());
}

}  // namespace ash
