// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_consent_store.h"

#include <optional>

#include "ash/constants/ash_pref_names.h"
#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/browser/ash/input_method/editor_context.h"
#include "chrome/browser/ash/input_method/editor_geolocation_mock_provider.h"
#include "chrome/browser/ash/input_method/editor_metrics_recorder.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::input_method {
namespace {

constexpr std::string_view kAllowedCountryCode = "au";

class FakeContextObserver : public EditorContext::Observer {
 public:
  FakeContextObserver() = default;
  ~FakeContextObserver() override = default;

  // EditorContext::Observer overrides
  void OnContextUpdated() override {}
  void OnImeChange(std::string_view engine_id) override {}
};

class FakeSystem : public EditorContext::System {
 public:
  FakeSystem() = default;
  ~FakeSystem() override = default;

  // EditorContext::System overrides
  std::optional<ukm::SourceId> GetUkmSourceId() override {
    return std::nullopt;
  }
};

class EditorConsentStoreTest : public ::testing::Test {
 public:
  EditorConsentStoreTest() = default;
  ~EditorConsentStoreTest() override = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(EditorConsentStoreTest,
       ReceivingDeclineResponseWillLeadToConsentDecline) {
  TestingProfile profile_;
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder metrics_recorder(&context,
                                         EditorOpportunityMode::kInvalidInput);
  EditorConsentStore store(profile_.GetPrefs(), &metrics_recorder);

  store.ProcessConsentAction(ConsentAction::kDecline);

  EXPECT_EQ(store.GetConsentStatus(), ConsentStatus::kDeclined);
}

TEST_F(EditorConsentStoreTest,
       ReceivingApprovalResponseWillLeadToConsentApproval) {
  TestingProfile profile_;
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder metrics_recorder(&context,
                                         EditorOpportunityMode::kInvalidInput);
  EditorConsentStore store(profile_.GetPrefs(), &metrics_recorder);

  store.ProcessConsentAction(ConsentAction::kApprove);

  EXPECT_EQ(store.GetConsentStatus(), ConsentStatus::kApproved);
}

TEST_F(EditorConsentStoreTest,
       SwitchingOnSettingToggleWillResetConsentWhichWasPreviouslyDeclined) {
  TestingProfile profile_;
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder metrics_recorder(&context,
                                         EditorOpportunityMode::kInvalidInput);
  EditorConsentStore store(profile_.GetPrefs(), &metrics_recorder);

  store.ProcessConsentAction(ConsentAction::kDecline);
  // Simulate a user action to switch on the orca toggle.
  profile_.GetPrefs()->SetBoolean(prefs::kOrcaEnabled, true);

  EXPECT_EQ(store.GetConsentStatus(), ConsentStatus::kUnset);
}

TEST_F(EditorConsentStoreTest,
       DecliningThePromoCardWillSwitchOffFeatureToggle) {
  TestingProfile profile_;
  FakeSystem system;
  FakeContextObserver observer;
  EditorGeolocationMockProvider geolocation_provider(kAllowedCountryCode);
  EditorContext context(&observer, &system, &geolocation_provider);
  EditorMetricsRecorder metrics_recorder(&context,
                                         EditorOpportunityMode::kInvalidInput);
  EditorConsentStore store(profile_.GetPrefs(), &metrics_recorder);

  // Switch on the orca toggle in the setting page.
  profile_.GetPrefs()->SetBoolean(prefs::kOrcaEnabled, true);
  // Simulate a user action to explicitly decline the promo card.
  store.ProcessPromoCardAction(PromoCardAction::kDecline);

  EXPECT_FALSE(profile_.GetPrefs()->GetBoolean(prefs::kOrcaEnabled));
}

}  // namespace
}  // namespace ash::input_method
