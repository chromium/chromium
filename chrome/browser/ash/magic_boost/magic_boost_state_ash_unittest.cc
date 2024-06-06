// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/magic_boost/magic_boost_state_ash.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/values.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"

using chromeos::HMRConsentStatus;
using chromeos::MagicBoostState;

namespace ash {

class TestMagicBoostStateObserver : public MagicBoostState::Observer {
 public:
  TestMagicBoostStateObserver() = default;

  TestMagicBoostStateObserver(const TestMagicBoostStateObserver&) = delete;
  TestMagicBoostStateObserver& operator=(const TestMagicBoostStateObserver&) =
      delete;

  ~TestMagicBoostStateObserver() override = default;

  // MagicBoostStateObserver:
  void OnHMRConsentStatusUpdated(HMRConsentStatus status) override {
    hmr_consent_status_ = status;
  }

  HMRConsentStatus hmr_consent_status() const { return hmr_consent_status_; }

 private:
  HMRConsentStatus hmr_consent_status_ = HMRConsentStatus::kUnset;
};

class MagicBoostStateAshTest : public AshTestBase {
 protected:
  MagicBoostStateAshTest() = default;
  MagicBoostStateAshTest(const MagicBoostStateAshTest&) = delete;
  MagicBoostStateAshTest& operator=(const MagicBoostStateAshTest&) = delete;
  ~MagicBoostStateAshTest() override = default;

  // ChromeAshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    prefs_ = static_cast<TestingPrefServiceSimple*>(
        ash::Shell::Get()->session_controller()->GetPrimaryUserPrefService());

    magic_boost_state_ = std::make_unique<MagicBoostStateAsh>();

    observer_ = std::make_unique<TestMagicBoostStateObserver>();
  }

  void TearDown() override {
    observer_.reset();
    magic_boost_state_.reset();
    prefs_ = nullptr;
    AshTestBase::TearDown();
  }

  TestingPrefServiceSimple* prefs() { return prefs_; }

  TestMagicBoostStateObserver* observer() { return observer_.get(); }

 private:
  raw_ptr<TestingPrefServiceSimple> prefs_;
  std::unique_ptr<TestMagicBoostStateObserver> observer_;
  std::unique_ptr<MagicBoostStateAsh> magic_boost_state_;
};

TEST_F(MagicBoostStateAshTest, UpdateHMRConsentStatus) {
  MagicBoostState::Get()->AddObserver(observer());

  EXPECT_EQ(MagicBoostState::Get()->hmr_consent_status(),
            HMRConsentStatus::kUnset);
  EXPECT_EQ(observer()->hmr_consent_status(), HMRConsentStatus::kUnset);

  // The observer class should get an notification when the pref value
  // changes.
  prefs()->SetInteger(ash::prefs::kHMRConsentStatus,
                      base::to_underlying(HMRConsentStatus::kDeclined));

  EXPECT_EQ(MagicBoostState::Get()->hmr_consent_status(),
            HMRConsentStatus::kDeclined);
  EXPECT_EQ(observer()->hmr_consent_status(), HMRConsentStatus::kDeclined);

  prefs()->SetInteger(ash::prefs::kHMRConsentStatus,
                      base::to_underlying(HMRConsentStatus::kApproved));
  EXPECT_EQ(MagicBoostState::Get()->hmr_consent_status(),
            HMRConsentStatus::kApproved);
  EXPECT_EQ(observer()->hmr_consent_status(), HMRConsentStatus::kApproved);

  MagicBoostState::Get()->RemoveObserver(observer());
}

TEST_F(MagicBoostStateAshTest, UpdateHMRConsentWindowDismissCount) {
  EXPECT_EQ(MagicBoostState::Get()->hmr_consent_window_dismiss_count(), 0);

  prefs()->SetInteger(ash::prefs::kHMRConsentWindowDismissCount, 1);
  EXPECT_EQ(MagicBoostState::Get()->hmr_consent_window_dismiss_count(), 1);

  prefs()->SetInteger(ash::prefs::kHMRConsentWindowDismissCount, 2);
  EXPECT_EQ(MagicBoostState::Get()->hmr_consent_window_dismiss_count(), 2);
}

}  // namespace ash
