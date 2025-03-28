// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lobster/lobster_system_state_provider_impl.h"

#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/lobster/lobster_enums.h"
#include "ash/public/cpp/lobster/lobster_system_state.h"
#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/editor_menu/public/cpp/editor_consent_status.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/test/test_enabled_state_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/variations/service/test_variations_service.h"
#include "components/variations/variations_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/mock_network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/ash/extension_ime_util.h"
#include "ui/base/ime/ash/input_method_descriptor.h"
#include "ui/base/ime/ash/mock_input_method_manager.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/display/tablet_state.h"
#include "ui/display/test/test_screen.h"

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

class LobsterSystemStateProviderImplBaseTest : public testing::Test {
 public:
  LobsterSystemStateProviderImplBaseTest()
      : test_screen_(/*create_display=*/true, /*register_screen=*/true),
        system_state_provider_(&pref_,
                               identity_test_environment_.identity_manager(),
                               /*is_in_demo_mode=*/false),
        metrics_enabled_state_provider_(/*consent=*/false, /*enabled=*/false) {
    // Sets up InputMethodManager
    InputMethodManagerFake::Initialize(new InputMethodManagerFake);

    RegisterSystemStateProviderPrefs();

    // Sets up test variations service
    variations::TestVariationsService::RegisterPrefs(
        local_state_pref()->registry());
    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        local_state_pref(), &metrics_enabled_state_provider_,
        /*backup_registry_key=*/std::wstring(),
        /*user_data_dir=*/base::FilePath(),
        metrics::StartupVisibility::kUnknown);
    variations_service_ = std::make_unique<variations::TestVariationsService>(
        local_state_pref(), metrics_state_manager_.get());
    TestingBrowserProcess::GetGlobal()->SetVariationsService(
        variations_service_.get());
  }

  ~LobsterSystemStateProviderImplBaseTest() override {
    TestingBrowserProcess::GetGlobal()->SetVariationsService(nullptr);
    variations_service_.reset();
    InputMethodManagerFake::Shutdown();
  }

  void RegisterSystemStateProviderPrefs() {
    pref_.registry()->RegisterIntegerPref(
        ash::prefs::kOrcaConsentStatus,
        static_cast<int>(chromeos::editor_menu::EditorConsentStatus::kUnset));
    pref_.registry()->RegisterBooleanPref(ash::prefs::kLobsterEnabled, true);
    pref_.registry()->RegisterIntegerPref(
        ash::prefs::kLobsterEnterprisePolicySettings,
        base::to_underlying(
            ash::LobsterEnterprisePolicyValue::kAllowedWithModelImprovement));
  }

  void SetUpEligibleHardware() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{ash::features::kLobster,
                              ash::features::kFeatureManagementLobster},
        /*disabled_features=*/{});
  }

  void SetConsentStatus(chromeos::editor_menu::EditorConsentStatus status) {
    pref_.SetInteger(ash::prefs::kOrcaConsentStatus, static_cast<int>(status));
  }

  void SetSettingsToggle(bool enabled) {
    pref_.SetBoolean(ash::prefs::kLobsterEnabled, enabled);
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

  void SetCountryCode(const std::string& country_code) {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        variations::switches::kVariationsOverrideCountry, country_code);
  }

  void SetAccountCapabilityValue(bool satisfied) {
    AccountInfo account =
        identity_test_environment_.MakePrimaryAccountAvailable(
            "someone@gmail.com", signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account.capabilities);
    mutator.set_can_use_manta_service(satisfied);
    signin::UpdateAccountInfoForAccount(
        identity_test_environment_.identity_manager(), account);
  }

  void SetTabletModeState(bool is_in_tablet_mode) {
    system_state_provider_.OnDisplayTabletStateChanged(
        is_in_tablet_mode ? display::TabletState::kInTabletMode
                          : display::TabletState::kInClamshellMode);
  }

  void SetPolicyValue(
      ash::LobsterEnterprisePolicyValue enterprise_policy_value) {
    pref_.SetInteger(ash::prefs::kLobsterEnterprisePolicySettings,
                     base::to_underlying(enterprise_policy_value));
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

  TestingPrefServiceSimple* local_state_pref() { return &local_state_pref_; }

 protected:
  content::BrowserTaskEnvironment task_environment_;

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<net::test::MockNetworkChangeNotifier> network_notifier_;
  TestingPrefServiceSimple local_state_pref_;
  TestingPrefServiceSimple pref_;
  signin::IdentityTestEnvironment identity_test_environment_;
  display::test::TestScreen test_screen_;
  LobsterSystemStateProviderImpl system_state_provider_;
  std::unique_ptr<variations::TestVariationsService> variations_service_;
  metrics::TestEnabledStateProvider metrics_enabled_state_provider_;
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager_;
};

class LobsterSystemStateProviderImplGeolocationTest
    : public LobsterSystemStateProviderImplBaseTest,
      public ::testing::WithParamInterface<std::tuple<
          /*country_code=*/std::string,
          /*expected_lobster_status=*/ash::LobsterStatus>> {
 public:
  void SetUp() override {
    SetUpEligibleHardware();
    SetConsentStatus(chromeos::editor_menu::EditorConsentStatus::kApproved);
    SetSettingsToggle(/*enabled=*/true);
    SetOnlineStatus(/*is_online=*/true);
    SetActiveIme("xkb:us::eng");
    SetAccountCapabilityValue(true);
    SetCountryCode(std::get<0>(GetParam()));
    SetTabletModeState(false);
  }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    LobsterSystemStateProviderImplGeolocationTest,
    testing::Values(
        std::make_tuple(/*country_code=*/"au", ash::LobsterStatus::kEnabled),
        std::make_tuple(/*country_code=*/"us", ash::LobsterStatus::kEnabled),
        std::make_tuple(/*country_code=*/"hk", ash::LobsterStatus::kBlocked)));

TEST_P(LobsterSystemStateProviderImplGeolocationTest,
       ChecksTheSystemStateStatus) {
  EXPECT_EQ(GetSystemState(GetValidTextInputContext()).status,
            std::get<1>(GetParam()));
}

class LobsterSystemStateProviderImplAccountCapabilityTest
    : public LobsterSystemStateProviderImplBaseTest,
      public ::testing::WithParamInterface<std::tuple<
          /*satisfied=*/bool,
          /*expected_lobster_status=*/ash::LobsterStatus>> {
 public:
  void SetUp() override {
    SetUpEligibleHardware();
    SetConsentStatus(chromeos::editor_menu::EditorConsentStatus::kApproved);
    SetSettingsToggle(/*enabled=*/true);
    SetOnlineStatus(/*is_online=*/true);
    SetActiveIme("xkb:us::eng");
    SetCountryCode("au");
    SetAccountCapabilityValue(/*satisfied=*/std::get<0>(GetParam()));
    SetTabletModeState(false);
  }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    LobsterSystemStateProviderImplAccountCapabilityTest,
    testing::Values(
        std::make_tuple(/*satisfied=*/true, ash::LobsterStatus::kEnabled),
        std::make_tuple(/*satisfied=*/false, ash::LobsterStatus::kBlocked)));

TEST_P(LobsterSystemStateProviderImplAccountCapabilityTest,
       ChecksTheSystemStateStatus) {
  EXPECT_EQ(GetSystemState(GetValidTextInputContext()).status,
            std::get<1>(GetParam()));
}

class LobsterSystemStateProviderImplTextInputFieldTest
    : public LobsterSystemStateProviderImplBaseTest,
      public ::testing::WithParamInterface<std::tuple<
          /*input_field_type=*/ui::TextInputType,
          /*expected_lobster_status=*/ash::LobsterStatus>> {
 public:
  void SetUp() override {
    SetUpEligibleHardware();
    SetConsentStatus(chromeos::editor_menu::EditorConsentStatus::kApproved);
    SetSettingsToggle(/*enabled=*/true);
    SetOnlineStatus(/*is_online=*/true);
    SetActiveIme("xkb:us::eng");
    SetCountryCode("au");
    SetAccountCapabilityValue(true);
    SetTabletModeState(false);
  }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    LobsterSystemStateProviderImplTextInputFieldTest,
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

TEST_P(LobsterSystemStateProviderImplTextInputFieldTest,
       ChecksTheSystemStateStatus) {
  EXPECT_EQ(GetSystemState(ash::LobsterTextInputContext(
                               /*text_input_type=*/std::get<0>(GetParam()),
                               /*caret_bounds=*/gfx::Rect(),
                               /*support_image_insertion=*/true))
                .status,
            std::get<1>(GetParam()));
}

class LobsterSystemStateProviderImplNetworkStatusTest
    : public LobsterSystemStateProviderImplBaseTest,
      public ::testing::WithParamInterface<std::tuple<
          /*is_online=*/bool,
          /*expected_lobster_status=*/ash::LobsterStatus>> {
 public:
  void SetUp() override {
    SetUpEligibleHardware();
    SetConsentStatus(chromeos::editor_menu::EditorConsentStatus::kApproved);
    SetSettingsToggle(/*enabled=*/true);
    SetOnlineStatus(/*is_online=*/std::get<0>(GetParam()));
    SetActiveIme("xkb:us::eng");
    SetCountryCode("au");
    SetAccountCapabilityValue(true);
    SetTabletModeState(false);
  }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    LobsterSystemStateProviderImplNetworkStatusTest,
    testing::Values(
        std::make_tuple(/*is_online=*/true, ash::LobsterStatus::kEnabled),
        std::make_tuple(/*is_online=*/false, ash::LobsterStatus::kBlocked)));

TEST_P(LobsterSystemStateProviderImplNetworkStatusTest,
       ChecksTheSystemStateStatus) {
  EXPECT_EQ(GetSystemState(GetValidTextInputContext()).status,
            std::get<1>(GetParam()));
}

class LobsterSystemStateProviderImplImeTest
    : public LobsterSystemStateProviderImplBaseTest,
      public ::testing::WithParamInterface<std::tuple<
          /*ime=*/std::string,
          /*expected_lobster_status=*/ash::LobsterStatus>> {
 public:
  void SetUp() override {
    SetUpEligibleHardware();
    SetConsentStatus(chromeos::editor_menu::EditorConsentStatus::kApproved);
    SetSettingsToggle(/*enabled=*/true);
    SetOnlineStatus(/*is_online=*/true);
    SetActiveIme(std::get<0>(GetParam()));
    SetCountryCode("au");
    SetAccountCapabilityValue(true);
    SetTabletModeState(false);
  }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    LobsterSystemStateProviderImplImeTest,
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
        std::make_tuple(/*ime=*/"xkb:ru::rus", ash::LobsterStatus::kBlocked)));

TEST_P(LobsterSystemStateProviderImplImeTest, ChecksTheSystemStateStatus) {
  EXPECT_EQ(GetSystemState(GetValidTextInputContext()).status,
            std::get<1>(GetParam()));
}

class LobsterSystemStateProviderImplTabletModeTest
    : public LobsterSystemStateProviderImplBaseTest,
      public ::testing::WithParamInterface<std::tuple<
          /*is_in_tablet_mode=*/bool,
          /*expected_lobster_status=*/ash::LobsterStatus>> {
 public:
  void SetUp() override {
    SetUpEligibleHardware();
    SetConsentStatus(chromeos::editor_menu::EditorConsentStatus::kApproved);
    SetSettingsToggle(/*enabled=*/true);
    SetOnlineStatus(true);
    SetActiveIme("xkb:us::eng");
    SetCountryCode("au");
    SetAccountCapabilityValue(true);
    SetTabletModeState(/*is_in_tablet_mode=*/std::get<0>(GetParam()));
  }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    LobsterSystemStateProviderImplTabletModeTest,
    testing::Values(std::make_tuple(/*is_in_tablet_mode=*/true,
                                    ash::LobsterStatus::kBlocked),
                    std::make_tuple(/*is_in_tablet_mode=*/false,
                                    ash::LobsterStatus::kEnabled)));

TEST_P(LobsterSystemStateProviderImplTabletModeTest,
       ChecksTheSystemStateStatus) {
  EXPECT_EQ(GetSystemState(GetValidTextInputContext()).status,
            std::get<1>(GetParam()));
}

class LobsterSystemStateProviderImplEnterprisePolicyTest
    : public LobsterSystemStateProviderImplBaseTest,
      public ::testing::WithParamInterface<std::tuple<
          /*enterprise_policy_value=*/ash::LobsterEnterprisePolicyValue,
          /*expected_lobster_status=*/ash::LobsterStatus>> {
 public:
  void SetUp() override {
    SetUpEligibleHardware();
    SetConsentStatus(chromeos::editor_menu::EditorConsentStatus::kApproved);
    SetSettingsToggle(/*enabled=*/true);
    SetOnlineStatus(true);
    SetActiveIme("xkb:us::eng");
    SetCountryCode("au");
    SetAccountCapabilityValue(true);
    SetTabletModeState(/*is_in_tablet_mode=*/false);
    SetPolicyValue(std::get<0>(GetParam()));
  }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    LobsterSystemStateProviderImplEnterprisePolicyTest,
    testing::Values(
        std::make_tuple(/*enterprise_policy_value=*/ash::
                            LobsterEnterprisePolicyValue::kDisabled,
                        ash::LobsterStatus::kBlocked),
        std::make_tuple(
            /*enterprise_policy_value=*/ash::LobsterEnterprisePolicyValue::
                kAllowedWithModelImprovement,
            ash::LobsterStatus::kEnabled),
        std::make_tuple(
            /*enterprise_policy_value=*/ash::LobsterEnterprisePolicyValue::
                kAllowedWithoutModelImprovement,
            ash::LobsterStatus::kEnabled)));

TEST_P(LobsterSystemStateProviderImplEnterprisePolicyTest,
       ChecksTheSystemStateStatus) {
  EXPECT_EQ(GetSystemState(GetValidTextInputContext()).status,
            std::get<1>(GetParam()));
}

}  // namespace
