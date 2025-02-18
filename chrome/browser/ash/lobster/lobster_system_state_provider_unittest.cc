// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lobster/lobster_system_state_provider.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/lobster/lobster_enums.h"
#include "ash/public/cpp/lobster/lobster_system_state.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_consent_status.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/mock_network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/input_method_descriptor.h"
#include "ui/base/ime/ash/mock_input_method_manager.h"
#include "ui/base/ime/text_input_type.h"

namespace {

class InputMethodManagerFake
    : public ash::input_method::MockInputMethodManager {
 public:
  InputMethodManagerFake() = default;

  InputMethodManagerFake(const InputMethodManagerFake&) = delete;
  InputMethodManagerFake& operator=(const InputMethodManagerFake&) = delete;

  // `MockInputMethodManager` implementation:
  scoped_refptr<ash::input_method::InputMethodManager::State>
  GetActiveIMEState() override {
    return state_;
  }

  void SetState(scoped_refptr<InputMethodManager::State> state) override {
    state_ = state;
  }
  class TestState : public ash::input_method::MockInputMethodManager::State {
   public:
    explicit TestState(const std::string& id)
        : current_input_method_(
              /*id=*/id,
              /*name=*/"",
              /*indicator=*/"",
              /*keyboard_layout=*/"",
              /*language_codes=*/{},
              /*is_login_keyboard*/ false,
              /*options_page_url=*/GURL(),
              /*input_view_url=*/GURL(),
              /*handwriting_language=*/"") {}
    TestState() = default;

    ash::input_method::InputMethodDescriptor GetCurrentInputMethod()
        const override {
      return current_input_method_;
    }

   private:
    ~TestState() override = default;
    ash::input_method::InputMethodDescriptor current_input_method_;
  };

 private:
  scoped_refptr<ash::input_method::InputMethodManager::State> state_;
};

class LobsterSystemStateProviderBaseTest : public testing::Test {
 public:
  LobsterSystemStateProviderBaseTest() : system_state_provider_(&profile_) {
    InputMethodManagerFake::Initialize(new InputMethodManagerFake);
  }
  ~LobsterSystemStateProviderBaseTest() override {
    InputMethodManagerFake::Shutdown();
  }

  void SetConsentStatus(chromeos::editor_menu::EditorConsentStatus status) {
    profile_.GetPrefs()->SetInteger(ash::prefs::kOrcaConsentStatus,
                                    static_cast<int>(status));
  }

  void SetSettingsToggle(bool enabled) {
    profile_.GetPrefs()->SetBoolean(ash::prefs::kLobsterEnabled, enabled);
  }

  void SetOnlineStatus(bool is_online) {
    network_notifier_ = net::test::MockNetworkChangeNotifier::Create();

    network_notifier_->SetConnectionType(
        is_online ? net::NetworkChangeNotifier::CONNECTION_WIFI
                  : net::NetworkChangeNotifier::CONNECTION_NONE);
  }

  void SetActiveIme(const std::string& active_ime_id) {
    InputMethodManagerFake::Get()->SetState(
        base::MakeRefCounted<InputMethodManagerFake::TestState>(
            ash::extension_ime_util::GetInputMethodIDByEngineID(
                active_ime_id)));
  }

  ash::LobsterSystemState GetSystemState(
      const ash::LobsterTextInputContext& text_input_context) {
    return system_state_provider_.GetSystemState(text_input_context);
  }

  ash::LobsterTextInputContext GetValidTextInputContext() {
    return ash::LobsterTextInputContext(
        /*text_input_type=*/ui::TEXT_INPUT_TYPE_TEXT,
        /*caret_bounds=*/gfx::Rect(),
        /*support_image_insertion=*/true);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<net::test::MockNetworkChangeNotifier> network_notifier_;
  TestingProfile profile_;
  LobsterSystemStateProvider system_state_provider_;
};

class LobsterSystemStateProviderTextInputFieldTest
    : public LobsterSystemStateProviderBaseTest,
      public ::testing::WithParamInterface<std::tuple<
          /*input_field_type=*/ui::TextInputType,
          /*expected_lobster_status*/ ash::LobsterStatus>> {
 public:
  void SetUp() override {
    SetConsentStatus(chromeos::editor_menu::EditorConsentStatus::kApproved);
    SetSettingsToggle(/*enabled=*/true);
    SetOnlineStatus(/*is_online=*/true);
    SetActiveIme("xkb:us::eng");
  }
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

class LobsterSystemStateProviderNetworkStatusTest
    : public LobsterSystemStateProviderBaseTest,
      public ::testing::WithParamInterface<std::tuple<
          /*is_online=*/bool,
          /*expected_lobster_status*/ ash::LobsterStatus>> {
 public:
  void SetUp() override {
    SetConsentStatus(chromeos::editor_menu::EditorConsentStatus::kApproved);
    SetSettingsToggle(/*enabled=*/true);
    SetOnlineStatus(/*is_online=*/std::get<0>(GetParam()));
    SetActiveIme("xkb:us::eng");
  }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    LobsterSystemStateProviderNetworkStatusTest,
    testing::Values(
        std::make_tuple(/*is_online*/ true, ash::LobsterStatus::kEnabled),
        std::make_tuple(/*is_online*/ false, ash::LobsterStatus::kBlocked), ));

TEST_P(LobsterSystemStateProviderNetworkStatusTest,
       ChecksTheSystemStateStatus) {
  EXPECT_EQ(GetSystemState(GetValidTextInputContext()).status,
            std::get<1>(GetParam()));
}

class LobsterSystemStateProviderImeTest
    : public LobsterSystemStateProviderBaseTest,
      public ::testing::WithParamInterface<std::tuple<
          /*ime=*/std::string,
          /*expected_lobster_status*/ ash::LobsterStatus>> {
 public:
  void SetUp() override {
    SetConsentStatus(chromeos::editor_menu::EditorConsentStatus::kApproved);
    SetSettingsToggle(/*enabled=*/true);
    SetOnlineStatus(/*is_online=*/true);
    SetActiveIme(std::get<0>(GetParam()));
  }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    LobsterSystemStateProviderImeTest,
    testing::Values(
        std::make_tuple(/*ime=*/"xkb:ca:eng:eng", ash::LobsterStatus::kEnabled),
        std::make_tuple(/*ime=*/"xkb:gb::eng", ash::LobsterStatus::kEnabled),
        std::make_tuple(/*ime=*/"xkb:gb:extd:eng",
                        ash::LobsterStatus::kEnabled),
        std::make_tuple(/*ime=*/"xkb:gb:dvorak:eng",
                        ash::LobsterStatus::kEnabled),
        std::make_tuple(/*ime=*/"xkb:in::eng", ash::LobsterStatus::kEnabled),
        std::make_tuple(/*ime=*/"xkb:pk::eng", ash::LobsterStatus::kEnabled),
        std::make_tuple(/*ime=*/"xkb:us:altgr-intl:eng",
                        ash::LobsterStatus::kEnabled),
        std::make_tuple(/*ime=*/"xkb:us:colemak:eng",
                        ash::LobsterStatus::kEnabled),
        std::make_tuple(/*ime=*/"xkb:us:dvorak:eng",
                        ash::LobsterStatus::kEnabled),
        std::make_tuple(/*ime=*/"xkb:us:dvp:eng", ash::LobsterStatus::kEnabled),
        std::make_tuple(/*ime=*/"xkb:us:intl_pc:eng",
                        ash::LobsterStatus::kEnabled),
        std::make_tuple(/*ime=*/"xkb:us:intl:eng",
                        ash::LobsterStatus::kEnabled),
        std::make_tuple(/*ime=*/"xkb:us:workman-intl:eng",
                        ash::LobsterStatus::kEnabled),
        std::make_tuple(/*ime=*/"xkb:us:workman:eng",
                        ash::LobsterStatus::kEnabled),
        std::make_tuple(/*ime=*/"xkb:us::eng", ash::LobsterStatus::kEnabled),
        std::make_tuple(/*ime=*/"xkb:za:gb:eng", ash::LobsterStatus::kEnabled),
        std::make_tuple(/*ime=*/"xkb:fr::fra", ash::LobsterStatus::kBlocked),
        std::make_tuple(/*ime=*/"xkb:de::ger", ash::LobsterStatus::kBlocked),
        std::make_tuple(/*ime=*/"xkb:ru::rus",
                        ash::LobsterStatus::kBlocked), ));

TEST_P(LobsterSystemStateProviderImeTest, ChecksTheSystemStateStatus) {
  EXPECT_EQ(GetSystemState(GetValidTextInputContext()).status,
            std::get<1>(GetParam()));
}

}  // namespace
