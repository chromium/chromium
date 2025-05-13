// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/magic_boost/magic_boost_state_ash.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/values.h"
#include "chrome/browser/ash/magic_boost/mock_editor_panel_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using chromeos::HMRConsentStatus;
using chromeos::MagicBoostState;

namespace ash {

namespace {

class TestMagicBoostStateObserver : public MagicBoostState::Observer {
 public:
  TestMagicBoostStateObserver() = default;

  TestMagicBoostStateObserver(const TestMagicBoostStateObserver&) = delete;
  TestMagicBoostStateObserver& operator=(const TestMagicBoostStateObserver&) =
      delete;

  ~TestMagicBoostStateObserver() override = default;

  // MagicBoostStateObserver:
  void OnHMREnabledUpdated(bool enabled) override { hmr_enabled_ = enabled; }

  void OnHMRConsentStatusUpdated(HMRConsentStatus status) override {
    hmr_consent_status_ = status;
  }

  void OnIsDeleting() override {
    // Do nothing as a user of `TestMagicBoostStateObserver` is responsible for
    // managing life cycle, i.e., stop observing before `MagicBoostState`
    // instance gets destructed.
  }

  bool hmr_enabled() const { return hmr_enabled_; }
  HMRConsentStatus hmr_consent_status() const { return hmr_consent_status_; }

 private:
  bool hmr_enabled_ = false;
  HMRConsentStatus hmr_consent_status_ = HMRConsentStatus::kUnset;
};

}  // namespace

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

    magic_boost_state_ = std::make_unique<MagicBoostStateAsh>(
        base::BindRepeating([]() { return static_cast<Profile*>(nullptr); }));
    magic_boost_state_->set_editor_panel_manager_for_test(
        &mock_editor_manager_);

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

  MagicBoostStateAsh* magic_boost_state() { return magic_boost_state_.get(); }

  MockEditorPanelManager& mock_editor_manager() { return mock_editor_manager_; }

 private:
  raw_ptr<TestingPrefServiceSimple> prefs_;
  std::unique_ptr<TestMagicBoostStateObserver> observer_;
  std::unique_ptr<MagicBoostStateAsh> magic_boost_state_;
  testing::NiceMock<MockEditorPanelManager> mock_editor_manager_;
};

TEST_F(MagicBoostStateAshTest, UpdateMagicBoostEnabledState) {
  MagicBoostState::Get()->AddObserver(observer());

  prefs()->SetBoolean(ash::prefs::kMagicBoostEnabled, false);

  // Both HMR and Orca should be disabled when `kMagicBoostEnabled` is false.
  EXPECT_FALSE(MagicBoostState::Get()->hmr_enabled().value());
  EXPECT_FALSE(prefs()->GetBoolean(ash::prefs::kOrcaEnabled));
  EXPECT_FALSE(prefs()->GetBoolean(ash::prefs::kLobsterEnabled));

  // The observer class should get a notification when the pref value
  // changes.
  MagicBoostState::Get()->AsyncWriteHMREnabled(false);
  EXPECT_FALSE(observer()->hmr_enabled());

  prefs()->SetBoolean(ash::prefs::kMagicBoostEnabled, true);

  // Both HMR and Orca should be enabled when `kMagicBoostEnabled` is true.
  EXPECT_TRUE(MagicBoostState::Get()->hmr_enabled().value());
  EXPECT_TRUE(prefs()->GetBoolean(ash::prefs::kOrcaEnabled));
  EXPECT_TRUE(prefs()->GetBoolean(ash::prefs::kLobsterEnabled));

  // The observer class should get a notification when the pref value
  // changes.
  EXPECT_TRUE(MagicBoostState::Get()->hmr_enabled().value());
  EXPECT_TRUE(observer()->hmr_enabled());

  MagicBoostState::Get()->RemoveObserver(observer());
}

TEST_F(MagicBoostStateAshTest, UpdateHMREnabledState) {
  MagicBoostState::Get()->AddObserver(observer());

  // The observer class should get an notification when the pref value
  // changes.
  MagicBoostState::Get()->AsyncWriteHMREnabled(false);
  EXPECT_FALSE(MagicBoostState::Get()->hmr_enabled().value());
  EXPECT_FALSE(observer()->hmr_enabled());

  // The observer class should get an notification when the pref value
  // changes.
  MagicBoostState::Get()->AsyncWriteHMREnabled(true);
  EXPECT_TRUE(MagicBoostState::Get()->hmr_enabled().value());
  EXPECT_TRUE(observer()->hmr_enabled());

  MagicBoostState::Get()->RemoveObserver(observer());
}

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

TEST_F(MagicBoostStateAshTest, UpdateHMRConsentStatusWhenEnableStateChanged) {
  MagicBoostState::Get()->AsyncWriteHMREnabled(false);
  MagicBoostState::Get()->AsyncWriteConsentStatus(HMRConsentStatus::kDeclined);

  // When consent status is `kDeclined` and enable state flip from false to
  // true (this can happen when flipping the toggle in Settings app), consent
  // status should be flip to `kPending` so that disclaimer UI can be shown when
  // accessing the feature.
  MagicBoostState::Get()->AsyncWriteHMREnabled(true);

  EXPECT_EQ(MagicBoostState::Get()->hmr_consent_status(),
            HMRConsentStatus::kPendingDisclaimer);

  // Flipping back enable state from true to false should retain the consent
  // status value.
  MagicBoostState::Get()->AsyncWriteHMREnabled(false);

  EXPECT_EQ(MagicBoostState::Get()->hmr_consent_status(),
            HMRConsentStatus::kPendingDisclaimer);

  MagicBoostState::Get()->AsyncWriteConsentStatus(HMRConsentStatus::kUnset);

  // When consent status is `kUnset` and enable state flip from false to
  // true (this can happen when flipping the toggle in Settings app), consent
  // status should be flip to `kPendingDisclaimer` so that disclaimer UI can be
  // shown when accessing the feature.
  MagicBoostState::Get()->AsyncWriteHMREnabled(true);

  EXPECT_EQ(MagicBoostState::Get()->hmr_consent_status(),
            HMRConsentStatus::kPendingDisclaimer);

  // When consent status is `kApproved`, flipping enable state should not change
  // consent status state.
  MagicBoostState::Get()->AsyncWriteConsentStatus(HMRConsentStatus::kApproved);

  MagicBoostState::Get()->AsyncWriteHMREnabled(false);
  EXPECT_EQ(MagicBoostState::Get()->hmr_consent_status(),
            HMRConsentStatus::kApproved);

  MagicBoostState::Get()->AsyncWriteHMREnabled(true);
  EXPECT_EQ(MagicBoostState::Get()->hmr_consent_status(),
            HMRConsentStatus::kApproved);
}

TEST_F(MagicBoostStateAshTest, UpdateHMRConsentWindowDismissCount) {
  EXPECT_EQ(MagicBoostState::Get()->hmr_consent_window_dismiss_count(), 0);

  prefs()->SetInteger(ash::prefs::kHMRConsentWindowDismissCount, 1);
  EXPECT_EQ(MagicBoostState::Get()->hmr_consent_window_dismiss_count(), 1);

  prefs()->SetInteger(ash::prefs::kHMRConsentWindowDismissCount, 2);
  EXPECT_EQ(MagicBoostState::Get()->hmr_consent_window_dismiss_count(), 2);
}

TEST_F(MagicBoostStateAshTest, ShouldIncludeOrcaInOptInFunctionCall) {
  // `ShouldIncludeOrcaInOptIn` should fetch panel context from
  // `EditorPanelManagerImpl` to see if opt-in is needed for Orca.
  EXPECT_CALL(mock_editor_manager(), GetEditorPanelContext);
  magic_boost_state()->ShouldIncludeOrcaInOptIn(
      base::BindOnce([](bool result) {}));
  testing::Mock::VerifyAndClearExpectations(&mock_editor_manager());
}

TEST_F(MagicBoostStateAshTest, ShouldIncludeOrcaInOptInBlocked) {
  ON_CALL(mock_editor_manager(), GetEditorPanelContext)
      .WillByDefault(
          [](base::OnceCallback<void(
                 const chromeos::editor_menu::EditorContext&)> callback) {
            std::move(callback).Run(chromeos::editor_menu::EditorContext(
                chromeos::editor_menu::EditorMode::kHardBlocked,
                chromeos::editor_menu::EditorTextSelectionMode::kNoSelection,
                /*consent_status_settled=*/false, {}));
          });

  magic_boost_state()->ShouldIncludeOrcaInOptIn(base::BindOnce([](bool result) {
    // If `EditorPanelMode` is `kHardBlocked`, Orca should not be included in
    // opt-in flow.
    EXPECT_FALSE(result);
  }));
  testing::Mock::VerifyAndClearExpectations(&mock_editor_manager());
}

TEST_F(MagicBoostStateAshTest, ShouldIncludeOrcaInOptInConsentStatusSettled) {
  ON_CALL(mock_editor_manager(), GetEditorPanelContext)
      .WillByDefault(
          [](base::OnceCallback<void(
                 const chromeos::editor_menu::EditorContext&)> callback) {
            std::move(callback).Run(chromeos::editor_menu::EditorContext(
                chromeos::editor_menu::EditorMode::kWrite,
                chromeos::editor_menu::EditorTextSelectionMode::kNoSelection,
                /*consent_status_settled=*/true, {}));
          });

  magic_boost_state()->ShouldIncludeOrcaInOptIn(base::BindOnce([](bool result) {
    // If `consent_status_settled`, Orca should not be included in opt-in flow.
    EXPECT_FALSE(result);
  }));
  testing::Mock::VerifyAndClearExpectations(&mock_editor_manager());
}

TEST_F(MagicBoostStateAshTest,
       ShouldIncludeOrcaInOptInConsentStatusNotSettled) {
  ON_CALL(mock_editor_manager(), GetEditorPanelContext)
      .WillByDefault(
          [](base::OnceCallback<void(
                 const chromeos::editor_menu::EditorContext&)> callback) {
            std::move(callback).Run(chromeos::editor_menu::EditorContext(
                chromeos::editor_menu::EditorMode::kWrite,
                chromeos::editor_menu::EditorTextSelectionMode::kNoSelection,
                /*consent_status_settled=*/false, {}));
          });

  magic_boost_state()->ShouldIncludeOrcaInOptIn(base::BindOnce([](bool result) {
    // If `consent_status_settled` is false, Orca should be included in opt-in
    // flow.
    EXPECT_TRUE(result);
  }));
  testing::Mock::VerifyAndClearExpectations(&mock_editor_manager());
}

TEST_F(MagicBoostStateAshTest, DisableOrcaFeature) {
  // `DisableOrcaFeature` should trigger the correct functions from
  // `EditorPanelManagerImpl`.
  EXPECT_CALL(mock_editor_manager(), OnMagicBoostPromoCardDeclined);

  magic_boost_state()->DisableOrcaFeature();
  testing::Mock::VerifyAndClearExpectations(&mock_editor_manager());
}

TEST_F(MagicBoostStateAshTest, EnableOrcaFeature) {
  // `EnableOrcaFeature` should trigger the correct functions from
  // `EditorPanelManagerImpl`.
  EXPECT_CALL(mock_editor_manager(), OnConsentApproved);

  magic_boost_state()->EnableOrcaFeature();
  testing::Mock::VerifyAndClearExpectations(&mock_editor_manager());
}

TEST_F(MagicBoostStateAshTest, DisableLobsterSettings) {
  ASSERT_TRUE(prefs()->GetBoolean(ash::prefs::kLobsterEnabled));

  magic_boost_state()->DisableLobsterSettings();

  EXPECT_FALSE(prefs()->GetBoolean(ash::prefs::kLobsterEnabled));
}

struct MagicBoostHmrCardShowConditionTestCase {
  std::string test_name;
  bool magic_boost_revamp_enabled;
  HMRConsentStatus hmr_consent_status;
  bool expected_hmr_card_shown;
};

class MagicBoostHmrCardShowConditionTest
    : public MagicBoostStateAshTest,
      public testing::WithParamInterface<
          MagicBoostHmrCardShowConditionTestCase> {};

TEST_P(MagicBoostHmrCardShowConditionTest, ShouldShowHmrCard) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatureState(chromeos::features::kMagicBoostRevamp,
                                    GetParam().magic_boost_revamp_enabled);

  MagicBoostState::Get()->AsyncWriteConsentStatus(
      GetParam().hmr_consent_status);

  EXPECT_EQ(MagicBoostState::Get()->ShouldShowHmrCard(),
            GetParam().expected_hmr_card_shown);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    MagicBoostHmrCardShowConditionTest,
    testing::ValuesIn<MagicBoostHmrCardShowConditionTestCase>(
        {// magic_boost_revamp_enabled = false
         MagicBoostHmrCardShowConditionTestCase{
             /*test_name=*/"NoMagicBoostRevamp_ConsentStatusUnset",
             /*magic_boost_revamp_enabled=*/false,
             /*hmr_consent_status=*/HMRConsentStatus::kUnset,
             /*expected_hmr_card_shown=*/false},
         MagicBoostHmrCardShowConditionTestCase{
             /*test_name=*/"NoMagicBoostRevamp_ConsentStatusDeclined",
             /*magic_boost_revamp_enabled=*/false,
             /*hmr_consent_status=*/HMRConsentStatus::kDeclined,
             /*expected_hmr_card_shown=*/false},
         MagicBoostHmrCardShowConditionTestCase{
             /*test_name=*/"NoMagicBoostRevamp_ConsentStatusApproved",
             /*magic_boost_revamp_enabled=*/false,
             /*hmr_consent_status=*/HMRConsentStatus::kApproved,
             /*expected_hmr_card_shown=*/true},
         MagicBoostHmrCardShowConditionTestCase{
             /*test_name=*/"NoMagicBoostRevamp_"
                           "ConsentStatusPendingDisclaimer",
             /*magic_boost_revamp_enabled=*/false,
             /*hmr_consent_status=*/HMRConsentStatus::kPendingDisclaimer,
             /*expected_hmr_card_shown=*/true},

         // magic_boost_revamp_enabled = true
         MagicBoostHmrCardShowConditionTestCase{
             /*test_name=*/"MagicBoostRevamp_ConsentStatusUnset",
             /*magic_boost_revamp_enabled=*/true,
             /*hmr_consent_status=*/HMRConsentStatus::kUnset,
             /*expected_hmr_card_shown=*/true},
         MagicBoostHmrCardShowConditionTestCase{
             /*test_name=*/"MagicBoostRevamp_ConsentStatusDeclined",
             /*magic_boost_revamp_enabled=*/true,
             /*hmr_consent_status=*/HMRConsentStatus::kDeclined,
             /*expected_hmr_card_shown=*/false},
         MagicBoostHmrCardShowConditionTestCase{
             /*test_name=*/"MagicBoostRevamp_ConsentStatusApproved",
             /*magic_boost_revamp_enabled=*/true,
             /*hmr_consent_status=*/HMRConsentStatus::kApproved,
             /*expected_hmr_card_shown=*/true},
         MagicBoostHmrCardShowConditionTestCase{
             /*test_name=*/"MagicBoostRevamp_"
                           "ConsentStatusPendingDisclaimer",
             /*magic_boost_revamp_enabled=*/true,
             /*hmr_consent_status=*/HMRConsentStatus::kPendingDisclaimer,
             /*expected_hmr_card_shown=*/true}}),
    [](const testing::TestParamInfo<
        MagicBoostHmrCardShowConditionTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace ash
