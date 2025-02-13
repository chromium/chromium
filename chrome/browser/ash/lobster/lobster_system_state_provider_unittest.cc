// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lobster/lobster_system_state_provider.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/lobster/lobster_enums.h"
#include "ash/public/cpp/lobster/lobster_system_state.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_consent_status.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/text_input_type.h"

namespace {

class LobsterSystemStateProviderTextInputFieldTest
    : public ChromeAshTestBase,
      public ::testing::WithParamInterface<std::tuple<
          /*input_field_type=*/ui::TextInputType,
          /*expected_lobster_status*/ ash::LobsterStatus>> {
 protected:
  LobsterSystemStateProviderTextInputFieldTest()
      : system_state_provider_(&profile_) {
    SetConsentStatus(chromeos::editor_menu::EditorConsentStatus::kApproved);
    SetSettingsToggle(/*enabled=*/true);
  }

  ~LobsterSystemStateProviderTextInputFieldTest() override = default;

  void SetConsentStatus(chromeos::editor_menu::EditorConsentStatus status) {
    profile_.GetPrefs()->SetInteger(ash::prefs::kOrcaConsentStatus,
                                    static_cast<int>(status));
  }

  void SetSettingsToggle(bool enabled) {
    profile_.GetPrefs()->SetBoolean(ash::prefs::kLobsterEnabled, enabled);
  }

  ash::LobsterSystemState GetSystemState(
      const ash::LobsterTextInputContext& text_input_context) {
    return system_state_provider_.GetSystemState(text_input_context);
  }

 private:
  TestingProfile profile_;
  LobsterSystemStateProvider system_state_provider_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    LobsterSystemStateProviderTextInputFieldTest,
    testing::Values(
        std::make_tuple(ui::TEXT_INPUT_TYPE_NONE, ash::LobsterStatus::kBlocked),
        std::make_tuple(ui::TEXT_INPUT_TYPE_TEXT, ash::LobsterStatus::kEnabled),
        std::make_tuple(ui::TEXT_INPUT_TYPE_PASSWORD,
                        ash::LobsterStatus::kBlocked),
        std::make_tuple(ui::TEXT_INPUT_TYPE_SEARCH,
                        ash::LobsterStatus::kBlocked),
        std::make_tuple(ui::TEXT_INPUT_TYPE_EMAIL,
                        ash::LobsterStatus::kBlocked),
        std::make_tuple(ui::TEXT_INPUT_TYPE_NUMBER,
                        ash::LobsterStatus::kBlocked),
        std::make_tuple(ui::TEXT_INPUT_TYPE_TELEPHONE,
                        ash::LobsterStatus::kBlocked),
        std::make_tuple(ui::TEXT_INPUT_TYPE_URL, ash::LobsterStatus::kBlocked),
        std::make_tuple(ui::TEXT_INPUT_TYPE_DATE, ash::LobsterStatus::kBlocked),
        std::make_tuple(ui::TEXT_INPUT_TYPE_DATE_TIME,
                        ash::LobsterStatus::kBlocked),
        std::make_tuple(ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL,
                        ash::LobsterStatus::kBlocked),
        std::make_tuple(ui::TEXT_INPUT_TYPE_MONTH,
                        ash::LobsterStatus::kBlocked),
        std::make_tuple(ui::TEXT_INPUT_TYPE_TIME, ash::LobsterStatus::kBlocked),
        std::make_tuple(ui::TEXT_INPUT_TYPE_WEEK, ash::LobsterStatus::kBlocked),
        std::make_tuple(ui::TEXT_INPUT_TYPE_TEXT_AREA,
                        ash::LobsterStatus::kEnabled),
        std::make_tuple(ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE,
                        ash::LobsterStatus::kEnabled),
        std::make_tuple(ui::TEXT_INPUT_TYPE_DATE_TIME_FIELD,
                        ash::LobsterStatus::kBlocked),
        std::make_tuple(ui::TEXT_INPUT_TYPE_NULL,
                        ash::LobsterStatus::kBlocked)));

TEST_P(LobsterSystemStateProviderTextInputFieldTest,
       ChecksTheSystemStateStatus) {
  EXPECT_EQ(GetSystemState(ash::LobsterTextInputContext(
                               /*text_input_type=*/std::get<0>(GetParam()),
                               /*caret_bounds=*/gfx::Rect(),
                               /*support_image_insertion=*/true))
                .status,
            std::get<1>(GetParam()));
}

}  // namespace
