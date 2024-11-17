// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/magic_boost/magic_boost_state_ash.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/values.h"
#include "chrome/browser/ash/magic_boost/mock_editor_panel_manager.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "chromeos/crosapi/mojom/editor_panel.mojom-shared.h"
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

    magic_boost_state_ = std::make_unique<MagicBoostStateAsh>();
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

  // The observer class should get a notification when the pref value
  // changes.
  MagicBoostState::Get()->AsyncWriteHMREnabled(false);
  EXPECT_FALSE(observer()->hmr_enabled());

  prefs()->SetBoolean(ash::prefs::kMagicBoostEnabled, true);

  // Both HMR and Orca should be enabled when `kMagicBoostEnabled` is true.
  EXPECT_TRUE(MagicBoostState::Get()->hmr_enabled().value());
  EXPECT_TRUE(prefs()->GetBoolean(ash::prefs::kOrcaEnabled));

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
  // `EditorPanelManager` to see if opt-in is needed for Orca.
  EXPECT_CALL(mock_editor_manager(), GetEditorPanelContext);
  magic_boost_state()->ShouldIncludeOrcaInOptIn(
      base::BindOnce([](bool result) {}));
  testing::Mock::VerifyAndClearExpectations(&mock_editor_manager());
}

TEST_F(MagicBoostStateAshTest, ShouldIncludeOrcaInOptInBlocked) {
  ON_CALL(mock_editor_manager(), GetEditorPanelContext)
      .WillByDefault(
          [](base::OnceCallback<void(crosapi::mojom::EditorPanelContextPtr)>
                 callback) {
            auto context = crosapi::mojom::EditorPanelContext::New();
            context->editor_panel_mode =
                crosapi::mojom::EditorPanelMode::kHardBlocked;
            std::move(callback).Run(std::move(context));
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
      .WillByDefault([](base::OnceCallback<void(
                            crosapi::mojom::EditorPanelContextPtr)> callback) {
        auto context = crosapi::mojom::EditorPanelContext::New();
        context->editor_panel_mode = crosapi::mojom::EditorPanelMode::kWrite;
        context->consent_status_settled = true;
        std::move(callback).Run(std::move(context));
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
      .WillByDefault([](base::OnceCallback<void(
                            crosapi::mojom::EditorPanelContextPtr)> callback) {
        auto context = crosapi::mojom::EditorPanelContext::New();
        context->editor_panel_mode = crosapi::mojom::EditorPanelMode::kWrite;
        context->consent_status_settled = false;
        std::move(callback).Run(std::move(context));
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
  // `EditorPanelManager`.
  EXPECT_CALL(mock_editor_manager(), OnConsentRejected);
  EXPECT_CALL(mock_editor_manager(), OnPromoCardDeclined);

  magic_boost_state()->DisableOrcaFeature();
  testing::Mock::VerifyAndClearExpectations(&mock_editor_manager());
}

TEST_F(MagicBoostStateAshTest, EnableOrcaFeature) {
  // `EnableOrcaFeature` should trigger the correct functions from
  // `EditorPanelManager`.
  EXPECT_CALL(mock_editor_manager(), OnConsentApproved);

  magic_boost_state()->EnableOrcaFeature();
  testing::Mock::VerifyAndClearExpectations(&mock_editor_manager());
}

}  // namespace ash
