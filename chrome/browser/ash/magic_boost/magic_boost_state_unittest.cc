// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/magic_boost/magic_boost_state.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/debug/stack_trace.h"
#include "base/functional/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/ash/magic_boost/mock_editor_panel_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
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
  void OnUserEligibleForGenAIFeaturesUpdated(bool eligible) override {
    user_eligible_for_genai_features_ = eligible;
    ++user_eligible_updated_count_;
  }

  void OnHMREnabledUpdated(bool enabled) override { hmr_enabled_ = enabled; }

  void OnHMRConsentStatusUpdated(HMRConsentStatus status) override {
    hmr_consent_status_ = status;
  }

  void OnIsDeleting() override {
    // Do nothing as a user of `TestMagicBoostStateObserver` is responsible for
    // managing life cycle, i.e., stop observing before `MagicBoostState`
    // instance gets destructed.
  }

  bool user_eligible_for_genai_features() const {
    return user_eligible_for_genai_features_;
  }
  int user_eligible_updated_count() const {
    return user_eligible_updated_count_;
  }
  bool hmr_enabled() const { return hmr_enabled_; }
  HMRConsentStatus hmr_consent_status() const { return hmr_consent_status_; }

 private:
  bool user_eligible_for_genai_features_ = false;
  int user_eligible_updated_count_ = 0;
  bool hmr_enabled_ = false;
  HMRConsentStatus hmr_consent_status_ = HMRConsentStatus::kUnset;
};

// `MagicBoostState` test double with a configurable GenAI eligibility result.
class ConfigurableEligibilityMagicBoostState : public ash::MagicBoostState {
 public:
  explicit ConfigurableEligibilityMagicBoostState(
      base::expected<bool, MagicBoostState::Error> expected_eligibility)
      : ash::MagicBoostState(base::BindRepeating(
            []() { return static_cast<Profile*>(nullptr); })),
        expected_eligibility_(expected_eligibility) {}

  void set_expected_eligibility(
      base::expected<bool, MagicBoostState::Error> expected_eligibility) {
    expected_eligibility_ = expected_eligibility;
  }

  int eligibility_check_count() const { return eligibility_check_count_; }

 protected:
  base::expected<bool, MagicBoostState::Error>
  IsUserEligibleForGenAIFeaturesExpected() const override {
    ++eligibility_check_count_;
    return expected_eligibility_;
  }

 private:
  base::expected<bool, MagicBoostState::Error> expected_eligibility_;
  mutable int eligibility_check_count_ = 0;
};

}  // namespace

class MagicBoostStateTest : public ChromeAshTestBase {
 protected:
  MagicBoostStateTest() = default;
  MagicBoostStateTest(const MagicBoostStateTest&) = delete;
  MagicBoostStateTest& operator=(const MagicBoostStateTest&) = delete;
  ~MagicBoostStateTest() override = default;

  // ChromeAshTestBase:
  void SetUp() override {
    ChromeAshTestBase::SetUp();

    prefs_ = static_cast<TestingPrefServiceSimple*>(
        ash::Shell::Get()->session_controller()->GetPrimaryUserPrefService());

    magic_boost_state_ = std::make_unique<MagicBoostState>(
        base::BindRepeating([]() { return static_cast<Profile*>(nullptr); }));
    magic_boost_state_->set_editor_panel_manager_for_test(
        &mock_editor_manager_);

    observer_ = std::make_unique<TestMagicBoostStateObserver>();
  }

  void TearDown() override {
    observer_.reset();
    magic_boost_state_.reset();
    prefs_ = nullptr;
    ChromeAshTestBase::TearDown();
  }

  ConfigurableEligibilityMagicBoostState* ReplaceWithConfigurableState(
      base::expected<bool, MagicBoostState::Error> initial_expected) {
    prefs_ = nullptr;
    magic_boost_state_.reset();
    ClearLogin();

    auto state = std::make_unique<ConfigurableEligibilityMagicBoostState>(
        initial_expected);
    auto* state_ptr = state.get();
    state->set_editor_panel_manager_for_test(&mock_editor_manager_);
    magic_boost_state_ = std::move(state);

    SimulateUserLogin(kRegularUserLoginInfo);
    prefs_ = static_cast<TestingPrefServiceSimple*>(
        ash::Shell::Get()->session_controller()->GetPrimaryUserPrefService());
    return state_ptr;
  }

  void ResetMagicBoostState() { magic_boost_state_.reset(); }

  void TriggerOnRefreshTokensReady() {
    magic_boost_state_->OnRefreshTokensReady();
  }

  TestingPrefServiceSimple* prefs() { return prefs_; }

  TestMagicBoostStateObserver* observer() { return observer_.get(); }

  MagicBoostState* magic_boost_state() { return magic_boost_state_.get(); }

  MockEditorPanelManager& mock_editor_manager() { return mock_editor_manager_; }

 private:
  raw_ptr<TestingPrefServiceSimple> prefs_;
  std::unique_ptr<TestMagicBoostStateObserver> observer_;
  std::unique_ptr<MagicBoostState> magic_boost_state_;
  testing::NiceMock<MockEditorPanelManager> mock_editor_manager_;
};

TEST_F(MagicBoostStateTest, UpdateMagicBoostEnabledState) {
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

TEST_F(MagicBoostStateTest, UpdateHMREnabledState) {
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

TEST_F(MagicBoostStateTest, UpdateHMRConsentStatus) {
  MagicBoostState::Get()->AddObserver(observer());

  EXPECT_EQ(MagicBoostState::Get()->hmr_consent_status(),
            HMRConsentStatus::kUnset);
  EXPECT_EQ(observer()->hmr_consent_status(), HMRConsentStatus::kUnset);

  // The observer class should get an notification when the pref value
  // changes.
  prefs()->SetInteger(ash::prefs::kHMRConsentStatus,
                      std::to_underlying(HMRConsentStatus::kDeclined));

  EXPECT_EQ(MagicBoostState::Get()->hmr_consent_status(),
            HMRConsentStatus::kDeclined);
  EXPECT_EQ(observer()->hmr_consent_status(), HMRConsentStatus::kDeclined);

  prefs()->SetInteger(ash::prefs::kHMRConsentStatus,
                      std::to_underlying(HMRConsentStatus::kApproved));
  EXPECT_EQ(MagicBoostState::Get()->hmr_consent_status(),
            HMRConsentStatus::kApproved);
  EXPECT_EQ(observer()->hmr_consent_status(), HMRConsentStatus::kApproved);

  MagicBoostState::Get()->RemoveObserver(observer());
}

TEST_F(MagicBoostStateTest, UpdateHMRConsentStatusWhenEnableStateChanged) {
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

TEST_F(MagicBoostStateTest, UpdateHMRConsentWindowDismissCount) {
  EXPECT_EQ(MagicBoostState::Get()->hmr_consent_window_dismiss_count(), 0);

  prefs()->SetInteger(ash::prefs::kHMRConsentWindowDismissCount, 1);
  EXPECT_EQ(MagicBoostState::Get()->hmr_consent_window_dismiss_count(), 1);

  prefs()->SetInteger(ash::prefs::kHMRConsentWindowDismissCount, 2);
  EXPECT_EQ(MagicBoostState::Get()->hmr_consent_window_dismiss_count(), 2);
}

TEST_F(MagicBoostStateTest, ShouldIncludeOrcaInOptInFunctionCall) {
  // `ShouldIncludeOrcaInOptIn` should fetch panel context from
  // `EditorPanelManagerImpl` to see if opt-in is needed for Orca.
  EXPECT_CALL(mock_editor_manager(), GetEditorPanelContext);
  magic_boost_state()->ShouldIncludeOrcaInOptIn(
      base::BindOnce([](bool result) {}));
  testing::Mock::VerifyAndClearExpectations(&mock_editor_manager());
}

TEST_F(MagicBoostStateTest, ShouldIncludeOrcaInOptInBlocked) {
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

TEST_F(MagicBoostStateTest, ShouldIncludeOrcaInOptInConsentStatusSettled) {
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

TEST_F(MagicBoostStateTest, ShouldIncludeOrcaInOptInConsentStatusNotSettled) {
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

TEST_F(MagicBoostStateTest, DisableOrcaFeature) {
  // `DisableOrcaFeature` should trigger the correct functions from
  // `EditorPanelManagerImpl`.
  EXPECT_CALL(mock_editor_manager(), OnMagicBoostPromoCardDeclined);

  magic_boost_state()->DisableOrcaFeature();
  testing::Mock::VerifyAndClearExpectations(&mock_editor_manager());
}

TEST_F(MagicBoostStateTest, EnableOrcaFeature) {
  // `EnableOrcaFeature` should trigger the correct functions from
  // `EditorPanelManagerImpl`.
  EXPECT_CALL(mock_editor_manager(), OnConsentApproved);

  magic_boost_state()->EnableOrcaFeature();
  testing::Mock::VerifyAndClearExpectations(&mock_editor_manager());
}

TEST_F(MagicBoostStateTest, DisableLobsterSettings) {
  ASSERT_TRUE(prefs()->GetBoolean(ash::prefs::kLobsterEnabled));

  magic_boost_state()->DisableLobsterSettings();

  EXPECT_FALSE(prefs()->GetBoolean(ash::prefs::kLobsterEnabled));
}

struct MagicBoostHmrCardShowConditionTestCase {
  std::string test_name;
  HMRConsentStatus hmr_consent_status;
  bool expected_hmr_card_shown;
};

class MagicBoostHmrCardShowConditionTest
    : public MagicBoostStateTest,
      public testing::WithParamInterface<
          MagicBoostHmrCardShowConditionTestCase> {};

TEST_P(MagicBoostHmrCardShowConditionTest, ShouldShowHmrCard) {
  MagicBoostState::Get()->AsyncWriteConsentStatus(
      GetParam().hmr_consent_status);

  EXPECT_EQ(MagicBoostState::Get()->ShouldShowHmrCard(),
            GetParam().expected_hmr_card_shown);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    MagicBoostHmrCardShowConditionTest,
    testing::ValuesIn<MagicBoostHmrCardShowConditionTestCase>(
        {MagicBoostHmrCardShowConditionTestCase{
             /*test_name=*/"ConsentStatusUnset",
             /*hmr_consent_status=*/HMRConsentStatus::kUnset,
             /*expected_hmr_card_shown=*/true},
         MagicBoostHmrCardShowConditionTestCase{
             /*test_name=*/"ConsentStatusDeclined",
             /*hmr_consent_status=*/HMRConsentStatus::kDeclined,
             /*expected_hmr_card_shown=*/false},
         MagicBoostHmrCardShowConditionTestCase{
             /*test_name=*/"ConsentStatusApproved",
             /*hmr_consent_status=*/HMRConsentStatus::kApproved,
             /*expected_hmr_card_shown=*/true},
         MagicBoostHmrCardShowConditionTestCase{
             /*test_name=*/"ConsentStatusPendingDisclaimer",
             /*hmr_consent_status=*/HMRConsentStatus::kPendingDisclaimer,
             /*expected_hmr_card_shown=*/true}}),
    [](const testing::TestParamInfo<
        MagicBoostHmrCardShowConditionTest::ParamType>& info) {
      return info.param.test_name;
    });

TEST_F(MagicBoostStateTest,
       IsUserEligibleForGenAIFeatures_DefaultAndWithMahiEnabled) {
  MagicBoostState::Get()->AddObserver(observer());

  // By default in unit tests (`kFeatureManagementMahi` disabled), the user is
  // not eligible for GenAI features.
  EXPECT_EQ(MagicBoostState::Get()->is_user_eligible_for_genai_features(),
            base::ok(false));
  EXPECT_FALSE(MagicBoostState::Get()->IsUserEligibleForGenAIFeatures());

  // When Mahi is enabled and restrictions are overridden, refreshing tokens
  // marks the user as eligible and notifies observers.
  base::test::ScopedFeatureList feature_list(
      chromeos::features::kFeatureManagementMahi);
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitch(
      chromeos::switches::kMahiRestrictionsOverride);

  TriggerOnRefreshTokensReady();

  EXPECT_EQ(MagicBoostState::Get()->is_user_eligible_for_genai_features(),
            base::ok(true));
  EXPECT_TRUE(MagicBoostState::Get()->IsUserEligibleForGenAIFeatures());
  EXPECT_TRUE(observer()->user_eligible_for_genai_features());
  EXPECT_EQ(observer()->user_eligible_updated_count(), 1);

  MagicBoostState::Get()->RemoveObserver(observer());
}

TEST_F(
    MagicBoostStateTest,
    IsUserEligibleForGenAIFeatures_UninitializedReturnsFalseAndDoesNotCache) {
  auto* state = ReplaceWithConfigurableState(
      base::unexpected(MagicBoostState::Error::kUninitialized));
  MagicBoostState::Get()->AddObserver(observer());

  // Initial login check saw `kUninitialized`, so state remains uninitialized.
  EXPECT_EQ(state->is_user_eligible_for_genai_features(),
            base::unexpected(MagicBoostState::Error::kUninitialized));
  const int checks_after_login = state->eligibility_check_count();
  EXPECT_EQ(checks_after_login, 1);

  // Calling `IsUserEligibleForGenAIFeatures()` while still uninitialized
  // returns false as a fail-safe and leaves the state uninitialized for future
  // retries.
  EXPECT_FALSE(state->IsUserEligibleForGenAIFeatures());
  EXPECT_EQ(state->is_user_eligible_for_genai_features(),
            base::unexpected(MagicBoostState::Error::kUninitialized));
  EXPECT_EQ(state->eligibility_check_count(), checks_after_login + 1);
  EXPECT_EQ(observer()->user_eligible_updated_count(), 0);

  MagicBoostState::Get()->RemoveObserver(observer());
}

TEST_F(MagicBoostStateTest,
       IsUserEligibleForGenAIFeatures_LazyReloadsMinorIneligibleAndCaches) {
  auto* state = ReplaceWithConfigurableState(
      base::unexpected(MagicBoostState::Error::kUninitialized));
  ASSERT_EQ(state->is_user_eligible_for_genai_features(),
            base::unexpected(MagicBoostState::Error::kUninitialized));

  // Simulate account capability resolving to false (e.g. U13 / minor account).
  state->set_expected_eligibility(base::ok(false));
  const int checks_before = state->eligibility_check_count();

  EXPECT_FALSE(state->IsUserEligibleForGenAIFeatures());
  EXPECT_EQ(state->is_user_eligible_for_genai_features(), base::ok(false));
  EXPECT_EQ(state->eligibility_check_count(), checks_before + 1);

  // Subsequent calls use the cached value without re-querying.
  EXPECT_FALSE(state->IsUserEligibleForGenAIFeatures());
  EXPECT_EQ(state->eligibility_check_count(), checks_before + 1);
}

TEST_F(MagicBoostStateTest,
       IsUserEligibleForGenAIFeatures_LazyReloadsAdultEligibleAndCaches) {
  auto* state = ReplaceWithConfigurableState(
      base::unexpected(MagicBoostState::Error::kUninitialized));
  ASSERT_EQ(state->is_user_eligible_for_genai_features(),
            base::unexpected(MagicBoostState::Error::kUninitialized));

  // Simulate account capability resolving to true (eligible adult account).
  state->set_expected_eligibility(base::ok(true));
  const int checks_before = state->eligibility_check_count();

  EXPECT_TRUE(state->IsUserEligibleForGenAIFeatures());
  EXPECT_EQ(state->is_user_eligible_for_genai_features(), base::ok(true));
  EXPECT_EQ(state->eligibility_check_count(), checks_before + 1);

  // Subsequent calls use the cached value without re-querying.
  EXPECT_TRUE(state->IsUserEligibleForGenAIFeatures());
  EXPECT_EQ(state->eligibility_check_count(), checks_before + 1);
}

TEST_F(MagicBoostStateTest,
       OnRefreshTokensReady_UpdatesEligibilityAndNotifiesObserverOnChange) {
  auto* state = ReplaceWithConfigurableState(
      base::unexpected(MagicBoostState::Error::kUninitialized));
  MagicBoostState::Get()->AddObserver(observer());

  // Resolve to ineligible (minor account) on refresh tokens ready.
  state->set_expected_eligibility(base::ok(false));
  TriggerOnRefreshTokensReady();
  EXPECT_EQ(state->is_user_eligible_for_genai_features(), base::ok(false));
  EXPECT_FALSE(observer()->user_eligible_for_genai_features());
  EXPECT_EQ(observer()->user_eligible_updated_count(), 1);

  // Re-triggering with the same eligibility value does not notify again.
  TriggerOnRefreshTokensReady();
  EXPECT_EQ(observer()->user_eligible_updated_count(), 1);

  // Updating to eligible notifies the observer.
  state->set_expected_eligibility(base::ok(true));
  TriggerOnRefreshTokensReady();
  EXPECT_EQ(state->is_user_eligible_for_genai_features(), base::ok(true));
  EXPECT_TRUE(observer()->user_eligible_for_genai_features());
  EXPECT_EQ(observer()->user_eligible_updated_count(), 2);

  MagicBoostState::Get()->RemoveObserver(observer());
}

class MagicBoostStateDeathTest : public MagicBoostStateTest {
 protected:
  void SetUp() override {
    GTEST_FLAG_SET(death_test_style, "fast");
    MagicBoostStateTest::SetUp();
  }

 private:
  base::debug::OverrideStackTraceOutputForTesting suppress_stack_traces_{
      base::debug::OverrideStackTraceOutputForTesting::Mode::kSuppressOutput};
};

TEST_F(MagicBoostStateDeathTest,
       AssertPreconditionsOfHelpMeReadOrCrash_SucceedsWhenAllPreconditionsMet) {
  ReplaceWithConfigurableState(base::ok(true));
  prefs()->SetBoolean(ash::prefs::kMagicBoostEnabled, true);
  MagicBoostState::Get()->AsyncWriteHMREnabled(true);
  MagicBoostState::Get()->AsyncWriteConsentStatus(HMRConsentStatus::kApproved);

  MagicBoostState::AssertPreconditionsOfHelpMeReadOrCrash();
}

TEST_F(MagicBoostStateDeathTest,
       AssertPreconditionsOfHelpMeReadOrCrash_CrashesWhenStateIsNull) {
  ResetMagicBoostState();
  EXPECT_CHECK_DEATH(MagicBoostState::AssertPreconditionsOfHelpMeReadOrCrash());
}

TEST_F(MagicBoostStateDeathTest,
       AssertPreconditionsOfHelpMeReadOrCrash_CrashesWhenUserIneligible) {
  ReplaceWithConfigurableState(base::ok(false));
  prefs()->SetBoolean(ash::prefs::kMagicBoostEnabled, true);
  MagicBoostState::Get()->AsyncWriteHMREnabled(true);
  MagicBoostState::Get()->AsyncWriteConsentStatus(HMRConsentStatus::kApproved);

  EXPECT_CHECK_DEATH(MagicBoostState::AssertPreconditionsOfHelpMeReadOrCrash());
}

TEST_F(
    MagicBoostStateDeathTest,
    AssertPreconditionsOfHelpMeReadOrCrash_CrashesWhenEligibilityUninitialized) {
  ReplaceWithConfigurableState(
      base::unexpected(MagicBoostState::Error::kUninitialized));
  prefs()->SetBoolean(ash::prefs::kMagicBoostEnabled, true);
  MagicBoostState::Get()->AsyncWriteHMREnabled(true);
  MagicBoostState::Get()->AsyncWriteConsentStatus(HMRConsentStatus::kApproved);

  EXPECT_CHECK_DEATH(MagicBoostState::AssertPreconditionsOfHelpMeReadOrCrash());
}

TEST_F(MagicBoostStateDeathTest,
       AssertPreconditionsOfHelpMeReadOrCrash_CrashesWhenMagicBoostDisabled) {
  ReplaceWithConfigurableState(base::ok(true));
  prefs()->SetBoolean(ash::prefs::kMagicBoostEnabled, false);
  MagicBoostState::Get()->AsyncWriteHMREnabled(true);
  MagicBoostState::Get()->AsyncWriteConsentStatus(HMRConsentStatus::kApproved);

  EXPECT_CHECK_DEATH(MagicBoostState::AssertPreconditionsOfHelpMeReadOrCrash());
}

TEST_F(MagicBoostStateDeathTest,
       AssertPreconditionsOfHelpMeReadOrCrash_CrashesWhenHmrDisabled) {
  ReplaceWithConfigurableState(base::ok(true));
  prefs()->SetBoolean(ash::prefs::kMagicBoostEnabled, true);
  MagicBoostState::Get()->AsyncWriteConsentStatus(HMRConsentStatus::kApproved);
  MagicBoostState::Get()->AsyncWriteHMREnabled(false);

  EXPECT_CHECK_DEATH(MagicBoostState::AssertPreconditionsOfHelpMeReadOrCrash());
}

TEST_F(MagicBoostStateDeathTest,
       AssertPreconditionsOfHelpMeReadOrCrash_CrashesWhenConsentNotApproved) {
  ReplaceWithConfigurableState(base::ok(true));
  prefs()->SetBoolean(ash::prefs::kMagicBoostEnabled, true);
  MagicBoostState::Get()->AsyncWriteHMREnabled(true);

  for (HMRConsentStatus status :
       {HMRConsentStatus::kUnset, HMRConsentStatus::kPendingDisclaimer,
        HMRConsentStatus::kDeclined}) {
    MagicBoostState::Get()->AsyncWriteConsentStatus(status);
    EXPECT_CHECK_DEATH(
        MagicBoostState::AssertPreconditionsOfHelpMeReadOrCrash());
  }
}

}  // namespace ash
