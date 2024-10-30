// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/wizard_controller.h"

#include <memory>
#include <tuple>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/base/locale_util.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_test_utils.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/enrollment/enrollment_screen.h"
#include "chrome/browser/ash/login/enrollment/mock_auto_enrollment_check_screen.h"
#include "chrome/browser/ash/login/enrollment/mock_enrollment_screen.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/login_wizard.h"
#include "chrome/browser/ash/login/oobe_configuration.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/consolidated_consent_screen.h"
#include "chrome/browser/ash/login/screens/device_disabled_screen.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/screens/hid_detection_screen.h"
#include "chrome/browser/ash/login/screens/mock_consolidated_consent_screen.h"
#include "chrome/browser/ash/login/screens/mock_demo_preferences_screen.h"
#include "chrome/browser/ash/login/screens/mock_demo_setup_screen.h"
#include "chrome/browser/ash/login/screens/mock_device_disabled_screen_view.h"
#include "chrome/browser/ash/login/screens/mock_enable_adb_sideloading_screen.h"
#include "chrome/browser/ash/login/screens/mock_enable_debugging_screen.h"
#include "chrome/browser/ash/login/screens/mock_network_screen.h"
#include "chrome/browser/ash/login/screens/mock_update_screen.h"
#include "chrome/browser/ash/login/screens/mock_welcome_screen.h"
#include "chrome/browser/ash/login/screens/mock_wrong_hwid_screen.h"
#include "chrome/browser/ash/login/screens/remote_activity_notification_screen.h"
#include "chrome/browser/ash/login/screens/reset_screen.h"
#include "chrome/browser/ash/login/screens/welcome_screen.h"
#include "chrome/browser/ash/login/screens/wrong_hwid_screen.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/local_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/network_portal_detector_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_configuration_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/net/network_portal_detector_test_impl.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_client.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_controller.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_state.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_state_fetcher.h"
#include "chrome/browser/ash/policy/enrollment/fake_auto_enrollment_client.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_device_state.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_state_keys_broker.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/ash/login/webui_login_view.h"
#include "chrome/browser/ui/webui/ash/login/consolidated_consent_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/display_size_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_info_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gesture_navigation_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/local_state_error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/marketing_opt_in_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/browser/ui/webui/ash/login/remote_activity_notification_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/reset_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/theme_selection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/touchpad_scroll_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/update_required_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/dbus/device_management/fake_install_attributes_client.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_manager_client.h"
#include "chromeos/ash/components/dbus/system_clock/system_clock_client.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "chromeos/ash/components/http_auth_dialog/http_auth_dialog.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/ash/components/timezone/timezone_request.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "chromeos/test/chromeos_test_utils.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/testing_pref_store.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "remoting/host/chromeos/features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsNull;
using ::testing::Mock;
using ::testing::NotNull;

const char kDMServerURLPrefix[] =
    "https://m.google.com/devicemanagement/data/api";

const char kGeolocationResponseBody[] =
    "{\n"
    "  \"location\": {\n"
    "    \"lat\": 51.0,\n"
    "    \"lng\": -0.1\n"
    "  },\n"
    "  \"accuracy\": 1200.4\n"
    "}";

// Timezone should not match kGeolocationResponseBody to check that exactly
// this value will be used.
const char kTimezoneResponseBody[] =
    "{\n"
    "    \"dstOffset\" : 0.0,\n"
    "    \"rawOffset\" : -32400.0,\n"
    "    \"status\" : \"OK\",\n"
    "    \"timeZoneId\" : \"America/Anchorage\",\n"
    "    \"timeZoneName\" : \"Pacific Standard Time\"\n"
    "}";

const char kDisabledMessage[] = "This device has been disabled.";

const test::UIPath kGuestSessionLink = {"error-message",
                                        "error-guest-signin-fix-network"};

constexpr policy::AutoEnrollmentState kAutoEnrollmentConnectionError =
    base::unexpected(policy::AutoEnrollmentDMServerError{
        .dm_error = policy::DM_STATUS_REQUEST_FAILED,
        .network_error = net::ERR_CONNECTION_REFUSED});

constexpr policy::AutoEnrollmentState kAutoEnrollmentServerError =
    base::unexpected(policy::AutoEnrollmentDMServerError{
        .dm_error = policy::DM_STATUS_TEMPORARY_UNAVAILABLE});

constexpr policy::AutoEnrollmentState kAutoEnrollmentStateKeysRetrievalError =
    base::unexpected(policy::AutoEnrollmentStateKeysRetrievalError{});

// Matches on the mode parameter of an EnrollmentConfig object.
MATCHER_P(EnrollmentModeMatches, mode, "") {
  return arg.mode == mode;
}

template <typename Error>
policy::AutoEnrollmentState ToAutoEnrollmentState(Error error) {
  return base::unexpected(error);
}

class PrefStoreStub : public TestingPrefStore {
 public:
  // TestingPrefStore overrides:
  PrefReadError GetReadError() const override {
    return PersistentPrefStore::PREF_READ_ERROR_JSON_PARSE;
  }

  bool IsInitializationComplete() const override { return true; }

 private:
  ~PrefStoreStub() override = default;
};

// Used to set up a `FakeAutoEnrollmentClientFactory` for the duration of a
// test.
class ScopedFakeAutoEnrollmentClientFactory {
 public:
  explicit ScopedFakeAutoEnrollmentClientFactory(
      policy::AutoEnrollmentController* controller)
      : controller_(controller) {
    auto fake_auto_enrollment_client_factory =
        std::make_unique<policy::FakeAutoEnrollmentClient::FactoryImpl>(
            base::BindRepeating(&ScopedFakeAutoEnrollmentClientFactory::
                                    OnFakeAutoEnrollmentClientCreated,
                                base::Unretained(this)));
    controller_->SetAutoEnrollmentClientFactoryForTesting(
        std::move(fake_auto_enrollment_client_factory));
  }

  ScopedFakeAutoEnrollmentClientFactory(
      const ScopedFakeAutoEnrollmentClientFactory&) = delete;
  ScopedFakeAutoEnrollmentClientFactory& operator=(
      const ScopedFakeAutoEnrollmentClientFactory&) = delete;

  ~ScopedFakeAutoEnrollmentClientFactory() {
    controller_->SetAutoEnrollmentClientFactoryForTesting(nullptr);
  }

  // Waits until the `policy::AutoEnrollmentController` has requested the
  // creation of an `AutoEnrollmentClient`. Returns the created
  // `AutoEnrollmentClient`. If an `AutoEnrollmentClient` has already been
  // created, returns immediately. Note: The returned instance is owned by
  // `policy::AutoEnrollmentController`.
  policy::FakeAutoEnrollmentClient* WaitAutoEnrollmentClientCreated() {
    if (created_auto_enrollment_client_) {
      return created_auto_enrollment_client_;
    }

    base::RunLoop run_loop;
    run_on_auto_enrollment_client_created_ = run_loop.QuitClosure();
    run_loop.Run();

    return created_auto_enrollment_client_;
  }

  // Resets the cached `AutoEnrollmentClient`, so another `AutoEnrollmentClient`
  // may be created through this factory.
  void Reset() { created_auto_enrollment_client_ = nullptr; }

 private:
  // Called when `fake_auto_enrollment_client_factory_` was asked to create an
  // `AutoEnrollmentClient`.
  void OnFakeAutoEnrollmentClientCreated(
      policy::FakeAutoEnrollmentClient* auto_enrollment_client) {
    // Only allow an AutoEnrollmentClient to be created when the test expects
    // it. The test should call `Reset` to expect a new `AutoEnrollmentClient`
    // to be created.
    EXPECT_FALSE(created_auto_enrollment_client_);
    created_auto_enrollment_client_ = auto_enrollment_client;

    if (run_on_auto_enrollment_client_created_) {
      std::move(run_on_auto_enrollment_client_created_).Run();
    }
  }

  // The `policy::AutoEnrollmentController` which is using
  // `fake_auto_enrollment_client_factory_`.
  raw_ptr<policy::AutoEnrollmentController> controller_;

  raw_ptr<policy::FakeAutoEnrollmentClient> created_auto_enrollment_client_ =
      nullptr;
  base::OnceClosure run_on_auto_enrollment_client_created_;
};

// Used to set up a fake `EnrollmentStateFetcher::Factory` for the duration of a
// test, which invokes result callback with the specified enrollment state.
class ScopedEnrollmentStateFetcherFactory {
 public:
  explicit ScopedEnrollmentStateFetcherFactory(
      policy::AutoEnrollmentController* controller)
      : controller_(controller) {
    controller_->SetEnrollmentStateFetcherFactoryForTesting(base::BindRepeating(
        &ScopedEnrollmentStateFetcherFactory::Create, base::Unretained(this)));
  }

  ScopedEnrollmentStateFetcherFactory(
      const ScopedEnrollmentStateFetcherFactory&) = delete;
  ScopedEnrollmentStateFetcherFactory& operator=(
      const ScopedEnrollmentStateFetcherFactory&) = delete;

  ~ScopedEnrollmentStateFetcherFactory() {
    controller_->SetEnrollmentStateFetcherFactoryForTesting(
        base::NullCallback());
  }

  // Waits until the `policy::AutoEnrollmentController` has requested the
  // creation of an `EnrollmentStateFetcher`. If an `EnrollmentStateFetcher` has
  // already been created, returns immediately. Note: The returned instance is
  // owned by `policy::AutoEnrollmentController`.
  void WaitUntilEnrollmentStateFetcherCreated() {
    if (!fetcher_created_) {
      base::RunLoop run_loop;
      created_callback_ = run_loop.QuitClosure();
      run_loop.Run();
    }
  }

  void ReportEnrollmentState(policy::AutoEnrollmentState state) {
    ASSERT_TRUE(fetcher_created_);
    ASSERT_FALSE(report_result_.is_null());
    std::move(report_result_).Run(state);
    base::RunLoop().RunUntilIdle();
  }

  void Reset() {
    fetcher_created_ = false;
    created_callback_.Reset();
  }

 private:
  class MockEnrollmentStateFetcher
      : public testing::StrictMock<policy::EnrollmentStateFetcher> {
   public:
    MOCK_METHOD(void, Start, ());
  };

  std::unique_ptr<policy::EnrollmentStateFetcher> Create(
      base::OnceCallback<void(policy::AutoEnrollmentState)> report_result,
      PrefService* local_state,
      policy::EnrollmentStateFetcher::RlweClientFactory rlwe_client_factory,
      policy::DeviceManagementService* device_management_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      policy::ServerBackedStateKeysBroker* state_key_broker,
      ash::DeviceSettingsService* device_settings_service,
      OobeConfiguration* oobe_configuration) {
    // Only allow one `EnrollmentStateFetcher` to be created. The test should
    // call `Reset` to expect a new `EnrollmentStateFetcher` to be created.
    EXPECT_FALSE(fetcher_created_);
    fetcher_created_ = true;
    report_result_ = std::move(report_result);

    auto fetcher = std::make_unique<MockEnrollmentStateFetcher>();
    EXPECT_CALL(*fetcher, Start()).WillOnce(testing::Return());

    if (!created_callback_.is_null()) {
      std::move(created_callback_).Run();
    }
    return std::move(fetcher);
  }

  // The `policy::AutoEnrollmentController` which is using
  // `MockEnrollmentStateFetcher`.
  raw_ptr<policy::AutoEnrollmentController> controller_;

  bool fetcher_created_ = false;
  base::OnceCallback<void(policy::AutoEnrollmentState)> report_result_;
  base::OnceClosure created_callback_;
};

struct SwitchLanguageTestData {
  SwitchLanguageTestData() : result("", "", false), done(false) {}

  locale_util::LanguageSwitchResult result;
  bool done;
};

void OnLocaleSwitched(SwitchLanguageTestData* self,
                      const locale_util::LanguageSwitchResult& result) {
  self->result = result;
  self->done = true;
}

void RunSwitchLanguageTest(const std::string& locale,
                           const std::string& expected_locale,
                           const bool expect_success) {
  SwitchLanguageTestData data;
  locale_util::SwitchLanguageCallback callback(
      base::BindOnce(&OnLocaleSwitched, base::Unretained(&data)));
  locale_util::SwitchLanguage(locale, true, false, std::move(callback),
                              ProfileManager::GetActiveUserProfile());

  // Token writing moves control to BlockingPool and back.
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(data.done, true);
  EXPECT_EQ(data.result.requested_locale, locale);
  EXPECT_EQ(data.result.loaded_locale, expected_locale);
  EXPECT_EQ(data.result.success, expect_success);
}

void SetUpCrasAndEnableChromeVox(int volume_percent, bool mute_on) {
  AccessibilityManager* a11y = AccessibilityManager::Get();
  CrasAudioHandler* cras = CrasAudioHandler::Get();

  // Audio output is at `volume_percent` and `mute_on`. Spoken feedback
  // is disabled.
  cras->SetOutputVolumePercent(volume_percent);
  cras->SetOutputMute(mute_on);
  a11y->EnableSpokenFeedback(false);

  // Spoken feedback is enabled.
  a11y->EnableSpokenFeedback(true);
  base::RunLoop().RunUntilIdle();
}

void QuitLoopOnAutoEnrollmentProgress(
    policy::AutoEnrollmentState expected_state,
    base::RunLoop* loop,
    policy::AutoEnrollmentState actual_state) {
  if (expected_state == actual_state) {
    loop->Quit();
  }
}

// Returns a string which can be put into the VPD variable
// `kRlzEmbargoEndDateKey`. If `days_offset` is 0, the return
// value represents the current day. If `days_offset` is positive, the return
// value represents `days_offset` days in the future. If `days_offset` is
// negative, the return value represents `days_offset` days in the past.
std::string GenerateEmbargoEndDate(int days_offset) {
  return base::UnlocalizedTimeFormatWithPattern(
      base::Time::Now() + base::Days(days_offset), "yyyy-MM-dd",
      icu::TimeZone::getGMT());
}

template <typename View>
void ExpectBind(View* view) {
  // TODO(jdufault): The view* api should follow the bind/unbind pattern instead
  // of bind(ptr), bind(nullptr).
  EXPECT_CALL(*view, MockBind(NotNull())).Times(1);
  EXPECT_CALL(*view, MockBind(IsNull())).Times(1);
}

template <typename View>
void ExpectBindUnbind(View* view) {
  EXPECT_CALL(*view, MockBind(NotNull())).Times(1);
  EXPECT_CALL(*view, MockUnbind()).Times(1);
}

template <typename View>
void ExpectSetDelegate(View* view) {
  EXPECT_CALL(*view, MockSetDelegate(NotNull())).Times(1);
  EXPECT_CALL(*view, MockSetDelegate(IsNull())).Times(1);
}

template <typename Mock>
Mock* MockScreen(std::unique_ptr<Mock> mock) {
  auto mock0 = mock.get();
  WizardController::default_controller()->screen_manager()->SetScreenForTesting(
      std::move(mock));
  return mock0;
}

template <typename Mock>
Mock* MockScreenExpectLifecycle(std::unique_ptr<Mock> mock) {
  auto mock0 = MockScreen(std::move(mock));
  EXPECT_CALL(*mock0, ShowImpl()).Times(0);
  return mock0;
}

}  // namespace

class WizardControllerTest : public OobeBaseTest {
 public:
  WizardControllerTest(const WizardControllerTest&) = delete;
  WizardControllerTest& operator=(const WizardControllerTest&) = delete;

 protected:
  WizardControllerTest() = default;
  ~WizardControllerTest() override = default;

  ErrorScreen* GetErrorScreen() { return GetOobeUI()->GetErrorScreen(); }

  OobeUI* GetOobeUI() { return LoginDisplayHost::default_host()->GetOobeUI(); }

  content::WebContents* GetWebContents() {
    LoginDisplayHost* host = LoginDisplayHost::default_host();
    if (!host) {
      return nullptr;
    }
    return host->GetOobeWebContents();
  }

  bool JSExecute(const std::string& script) {
    return content::ExecJs(GetWebContents(), script);
  }

  bool JSExecuteBooleanExpression(const std::string& expression) {
    return content::EvalJs(GetWebContents(), "!!(" + expression + ");")
        .ExtractBool();
  }

  std::string JSExecuteStringExpression(const std::string& expression) {
    return content::EvalJs(GetWebContents(), expression).ExtractString();
  }

  void CheckCurrentScreen(OobeScreenId screen) {
    BaseScreen* current_screen =
        WizardController::default_controller()->current_screen();
    const std::string actual_screen =
        current_screen ? current_screen->screen_id().name : "nullptr";
    const std::string expected_screen = screen.name;
    EXPECT_EQ(actual_screen, expected_screen);
  }

  WrongHWIDScreen* GetWrongHWIDScreen() {
    return static_cast<WrongHWIDScreen*>(
        WizardController::default_controller()->GetScreen(
            WrongHWIDScreenView::kScreenId));
  }
};

IN_PROC_BROWSER_TEST_F(WizardControllerTest, SwitchLanguage) {
  ASSERT_TRUE(WizardController::default_controller() != nullptr);
  WizardController::default_controller()->AdvanceToScreen(
      WelcomeView::kScreenId);
  test::WaitForWelcomeScreen();

  // Checking the default locale. Provided that the profile is cleared in SetUp.
  EXPECT_EQ("en-US", g_browser_process->GetApplicationLocale());
  EXPECT_STREQ("en", icu::Locale::getDefault().getLanguage());
  EXPECT_FALSE(base::i18n::IsRTL());
  const std::u16string en_str =
      l10n_util::GetStringUTF16(IDS_UPDATE_STATUS_TITLE);

  RunSwitchLanguageTest("fr", "fr", true);
  EXPECT_EQ("fr", g_browser_process->GetApplicationLocale());
  EXPECT_STREQ("fr", icu::Locale::getDefault().getLanguage());
  EXPECT_FALSE(base::i18n::IsRTL());
  const std::u16string fr_str =
      l10n_util::GetStringUTF16(IDS_UPDATE_STATUS_TITLE);

  EXPECT_NE(en_str, fr_str);

  RunSwitchLanguageTest("ar", "ar", true);
  EXPECT_EQ("ar", g_browser_process->GetApplicationLocale());
  EXPECT_STREQ("ar", icu::Locale::getDefault().getLanguage());
  EXPECT_TRUE(base::i18n::IsRTL());
  const std::u16string ar_str =
      l10n_util::GetStringUTF16(IDS_UPDATE_STATUS_TITLE);

  EXPECT_NE(fr_str, ar_str);
}

IN_PROC_BROWSER_TEST_F(WizardControllerTest, VolumeIsChangedForChromeVox) {
  SetUpCrasAndEnableChromeVox(75 /* volume_percent */, true /* mute_on */);

  // Check that output is unmuted now and at some level.
  CrasAudioHandler* cras = CrasAudioHandler::Get();
  ASSERT_FALSE(cras->IsOutputMuted());
  ASSERT_EQ(WizardController::kMinAudibleOutputVolumePercent,
            cras->GetOutputVolumePercent());
}

IN_PROC_BROWSER_TEST_F(WizardControllerTest, VolumeIsUnchangedForChromeVox) {
  SetUpCrasAndEnableChromeVox(75 /* volume_percent */, false /* mute_on */);

  // Check that output is unmuted now and at some level.
  CrasAudioHandler* cras = CrasAudioHandler::Get();
  ASSERT_FALSE(cras->IsOutputMuted());
  ASSERT_EQ(75, cras->GetOutputVolumePercent());
}

IN_PROC_BROWSER_TEST_F(WizardControllerTest, VolumeIsAdjustedForChromeVox) {
  SetUpCrasAndEnableChromeVox(5 /* volume_percent */, false /* mute_on */);

  // Check that output is unmuted now and at some level.
  CrasAudioHandler* cras = CrasAudioHandler::Get();
  ASSERT_FALSE(cras->IsOutputMuted());
  ASSERT_EQ(WizardController::kMinAudibleOutputVolumePercent,
            cras->GetOutputVolumePercent());
}

class WizardControllerFlowTest : public WizardControllerTest {
 public:
  WizardControllerFlowTest(const WizardControllerFlowTest&) = delete;
  WizardControllerFlowTest& operator=(const WizardControllerFlowTest&) = delete;

 protected:
  WizardControllerFlowTest() = default;
  // WizardControllerTest:
  void SetUpOnMainThread() override {
    WizardControllerTest::SetUpOnMainThread();

    // Make sure that OOBE is run as an "official" build.
    LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
        true;

    WizardController* wizard_controller =
        WizardController::default_controller();
    wizard_controller->SetCurrentScreen(nullptr);
    WaitForOobeUI();
    wizard_controller->SetSharedURLLoaderFactoryForTesting(
        test_url_loader_factory_.GetSafeWeakWrapper());
    SimpleGeolocationProvider::GetInstance()
        ->SetSharedUrlLoaderFactoryForTesting(
            test_url_loader_factory_.GetSafeWeakWrapper());

    // Set up the mocks for all screens.
    mock_welcome_screen_ =
        MockScreenExpectLifecycle(std::make_unique<MockWelcomeScreen>(
            GetOobeUI()->GetView<WelcomeScreenHandler>()->AsWeakPtr(),
            base::BindRepeating(&WizardController::OnWelcomeScreenExit,
                                base::Unretained(wizard_controller))));

    mock_demo_preferences_screen_view_ =
        std::make_unique<MockDemoPreferencesScreenView>();
    mock_demo_preferences_screen_ =
        MockScreenExpectLifecycle(std::make_unique<MockDemoPreferencesScreen>(
            mock_demo_preferences_screen_view_->AsWeakPtr(),
            base::BindRepeating(&WizardController::OnDemoPreferencesScreenExit,
                                base::Unretained(wizard_controller))));

    device_disabled_screen_view_ =
        std::make_unique<MockDeviceDisabledScreenView>();
    MockScreen(std::make_unique<DeviceDisabledScreen>(
        device_disabled_screen_view_->AsWeakPtr()));
    EXPECT_CALL(*device_disabled_screen_view_, Show(_)).Times(0);

    mock_network_screen_view_ = std::make_unique<MockNetworkScreenView>();
    mock_network_screen_ =
        MockScreenExpectLifecycle(std::make_unique<MockNetworkScreen>(
            mock_network_screen_view_.get()->AsWeakPtr(),
            base::BindRepeating(&WizardController::OnNetworkScreenExit,
                                base::Unretained(wizard_controller))));

    mock_update_view_ = std::make_unique<MockUpdateView>();
    mock_update_screen_ =
        MockScreenExpectLifecycle(std::make_unique<MockUpdateScreen>(
            mock_update_view_.get()->AsWeakPtr(), GetErrorScreen(),
            base::BindRepeating(&WizardController::OnUpdateScreenExit,
                                base::Unretained(wizard_controller))));

    mock_enrollment_screen_view_ = std::make_unique<MockEnrollmentScreenView>();
    mock_enrollment_screen_ =
        MockScreenExpectLifecycle(std::make_unique<MockEnrollmentScreen>(
            mock_enrollment_screen_view_->AsWeakPtr(), GetErrorScreen(),
            base::BindRepeating(&WizardController::OnEnrollmentScreenExit,
                                base::Unretained(wizard_controller))));

    mock_auto_enrollment_check_screen_view_ =
        std::make_unique<MockAutoEnrollmentCheckScreenView>();
    mock_auto_enrollment_check_screen_ = MockScreen(
        std::make_unique<testing::NiceMock<MockAutoEnrollmentCheckScreen>>(
            mock_auto_enrollment_check_screen_view_.get()->AsWeakPtr(),
            GetErrorScreen(),
            base::BindRepeating(
                &WizardController::OnAutoEnrollmentCheckScreenExit,
                base::Unretained(wizard_controller))));

    mock_wrong_hwid_screen_view_ = std::make_unique<MockWrongHWIDScreenView>();

    mock_wrong_hwid_screen_ =
        MockScreenExpectLifecycle(std::make_unique<MockWrongHWIDScreen>(
            mock_wrong_hwid_screen_view_.get()->AsWeakPtr(),
            base::BindRepeating(&WizardController::OnWrongHWIDScreenExit,
                                base::Unretained(wizard_controller))));

    mock_enable_adb_sideloading_screen_view_ =
        std::make_unique<MockEnableAdbSideloadingScreenView>();
    mock_enable_adb_sideloading_screen_ = MockScreenExpectLifecycle(
        std::make_unique<MockEnableAdbSideloadingScreen>(
            mock_enable_adb_sideloading_screen_view_->AsWeakPtr(),
            base::BindRepeating(
                &WizardController::OnEnableAdbSideloadingScreenExit,
                base::Unretained(wizard_controller))));

    mock_enable_debugging_screen_view_ =
        std::make_unique<MockEnableDebuggingScreenView>();
    mock_enable_debugging_screen_ =
        MockScreenExpectLifecycle(std::make_unique<MockEnableDebuggingScreen>(
            mock_enable_debugging_screen_view_.get()->AsWeakPtr(),
            base::BindRepeating(&WizardController::OnEnableDebuggingScreenExit,
                                base::Unretained(wizard_controller))));

    mock_demo_setup_screen_view_ = std::make_unique<MockDemoSetupScreenView>();
    mock_demo_setup_screen_ =
        MockScreenExpectLifecycle(std::make_unique<MockDemoSetupScreen>(
            mock_demo_setup_screen_view_->AsWeakPtr(),
            base::BindRepeating(&WizardController::OnDemoSetupScreenExit,
                                base::Unretained(wizard_controller))));

    mock_demo_preferences_screen_view_ =
        std::make_unique<MockDemoPreferencesScreenView>();
    mock_demo_preferences_screen_ =
        MockScreenExpectLifecycle(std::make_unique<MockDemoPreferencesScreen>(
            mock_demo_preferences_screen_view_->AsWeakPtr(),
            base::BindRepeating(&WizardController::OnDemoPreferencesScreenExit,
                                base::Unretained(wizard_controller))));

    mock_consolidated_consent_screen_view_ =
        std::make_unique<MockConsolidatedConsentScreenView>();
    mock_consolidated_consent_screen_ = MockScreenExpectLifecycle(
        std::make_unique<MockConsolidatedConsentScreen>(
            mock_consolidated_consent_screen_view_.get()->AsWeakPtr(),
            base::BindRepeating(
                &WizardController::OnConsolidatedConsentScreenExit,
                base::Unretained(wizard_controller))));

    // Switch to the initial screen.
    EXPECT_EQ(nullptr, wizard_controller->current_screen());
    EXPECT_CALL(*mock_welcome_screen_, ShowImpl()).Times(1);
    wizard_controller->AdvanceToScreen(WelcomeView::kScreenId);
  }

  void TearDownOnMainThread() override {
    mock_welcome_screen_ = nullptr;
    device_disabled_screen_view_.reset();
    test_url_loader_factory_.ClearResponses();
    WizardControllerTest::TearDownOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WizardControllerTest::SetUpCommandLine(command_line);

    // Default to now showing auto enrollment check screen. If you want to show
    // this screen, you can override the flags.
    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableForcedReEnrollment,
        policy::AutoEnrollmentTypeChecker::kForcedReEnrollmentNever);
    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableInitialEnrollment,
        policy::AutoEnrollmentTypeChecker::kInitialEnrollmentNever);
  }

  void InitNetworkPortalDetector() {
    network_portal_detector_ = new NetworkPortalDetectorTestImpl();
    network_portal_detector::InitializeForTesting(network_portal_detector_);

    // Default networks defaults to "eth1" in tests.
    const NetworkState* default_network =
        NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
    ASSERT_TRUE(default_network);
    network_portal_detector_->SetDefaultNetworkForTesting(
        default_network->guid());
  }

  void WaitUntilTimezoneResolved() {
    base::RunLoop loop;
    if (!WizardController::default_controller()
             ->SetOnTimeZoneResolvedForTesting(loop.QuitClosure())) {
      return;
    }

    loop.Run();
  }

  void ResetAutoEnrollmentCheckScreen() {
    WizardController::default_controller()
        ->screen_manager()
        ->DeleteScreenForTesting(AutoEnrollmentCheckScreenView::kScreenId);
  }

  void TestControlFlowMain() {
    CheckCurrentScreen(WelcomeView::kScreenId);

    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          if (base::StartsWith(
                  request.url.spec(),
                  SimpleGeolocationProvider::DefaultGeolocationProviderURL()
                      .spec(),
                  base::CompareCase::SENSITIVE)) {
            test_url_loader_factory_.AddResponse(request.url.spec(),
                                                 kGeolocationResponseBody);
          } else if (base::StartsWith(request.url.spec(),
                                      DefaultTimezoneProviderURL().spec(),
                                      base::CompareCase::SENSITIVE)) {
            test_url_loader_factory_.AddResponse(request.url.spec(),
                                                 kTimezoneResponseBody);
          }
        }));

    ASSERT_TRUE(LoginScreenTestApi::IsLoginShelfShown());

    EXPECT_CALL(*mock_welcome_screen_, HideImpl()).Times(1);
    EXPECT_CALL(*mock_network_screen_, ShowImpl()).Times(1);
    mock_welcome_screen_->ExitScreen(WelcomeScreen::Result::kNext);

    CheckCurrentScreen(NetworkScreenView::kScreenId);
    EXPECT_CALL(*mock_network_screen_, HideImpl()).Times(1);

    InitNetworkPortalDetector();

    EXPECT_CALL(*mock_update_screen_, ShowImpl()).Times(1);
    mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

    EXPECT_NE(SimpleGeolocationProvider::GetInstance(), nullptr);

    // Let update screen smooth time process (time = 0ms).
    content::RunAllPendingInMessageLoop();

    CheckCurrentScreen(UpdateView::kScreenId);
    EXPECT_CALL(*mock_update_screen_, HideImpl()).Times(1);
    EXPECT_CALL(*mock_auto_enrollment_check_screen_, ShowImpl()).Times(0);
    mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

    CheckCurrentScreen(UserCreationView::kScreenId);
    EXPECT_CALL(*mock_auto_enrollment_check_screen_, HideImpl()).Times(0);

    EXPECT_FALSE(ExistingUserController::current_controller() == nullptr);

    WaitUntilTimezoneResolved();
    EXPECT_EQ(
        "America/Anchorage",
        base::UTF16ToUTF8(
            system::TimezoneSettings::GetInstance()->GetCurrentTimezoneID()));
  }

  // All of the *Screen types are owned by WizardController. The views are owned
  // by this test class.
  raw_ptr<MockWelcomeScreen, DanglingUntriaged> mock_welcome_screen_ = nullptr;

  raw_ptr<MockNetworkScreen, DanglingUntriaged> mock_network_screen_ = nullptr;
  std::unique_ptr<MockNetworkScreenView> mock_network_screen_view_;

  raw_ptr<MockUpdateScreen, DanglingUntriaged> mock_update_screen_ = nullptr;
  std::unique_ptr<MockUpdateView> mock_update_view_;

  raw_ptr<MockEnrollmentScreen, DanglingUntriaged> mock_enrollment_screen_ =
      nullptr;
  std::unique_ptr<MockEnrollmentScreenView> mock_enrollment_screen_view_;

  // Auto enrollment check screen is a nice mock because it may or may not be
  // shown depending on when asynchronous auto enrollment check finishes. Only
  // add expectations for this if you are sure they are not affected by race
  // conditions.
  raw_ptr<testing::NiceMock<MockAutoEnrollmentCheckScreen>, DanglingUntriaged>
      mock_auto_enrollment_check_screen_ = nullptr;
  std::unique_ptr<MockAutoEnrollmentCheckScreenView>
      mock_auto_enrollment_check_screen_view_;

  raw_ptr<MockWrongHWIDScreen, DanglingUntriaged> mock_wrong_hwid_screen_ =
      nullptr;
  std::unique_ptr<MockWrongHWIDScreenView> mock_wrong_hwid_screen_view_;

  raw_ptr<MockEnableAdbSideloadingScreen, DanglingUntriaged>
      mock_enable_adb_sideloading_screen_ = nullptr;
  std::unique_ptr<MockEnableAdbSideloadingScreenView>
      mock_enable_adb_sideloading_screen_view_;

  raw_ptr<MockEnableDebuggingScreen, DanglingUntriaged>
      mock_enable_debugging_screen_ = nullptr;
  std::unique_ptr<MockEnableDebuggingScreenView>
      mock_enable_debugging_screen_view_;

  raw_ptr<MockDemoSetupScreen, DanglingUntriaged> mock_demo_setup_screen_ =
      nullptr;
  std::unique_ptr<MockDemoSetupScreenView> mock_demo_setup_screen_view_;

  raw_ptr<MockDemoPreferencesScreen, DanglingUntriaged>
      mock_demo_preferences_screen_ = nullptr;
  std::unique_ptr<MockDemoPreferencesScreenView>
      mock_demo_preferences_screen_view_;

  raw_ptr<MockConsolidatedConsentScreen, DanglingUntriaged>
      mock_consolidated_consent_screen_ = nullptr;
  std::unique_ptr<MockConsolidatedConsentScreenView>
      mock_consolidated_consent_screen_view_;

  std::unique_ptr<MockDeviceDisabledScreenView> device_disabled_screen_view_;

  network::TestURLLoaderFactory test_url_loader_factory_;

 private:
  raw_ptr<NetworkPortalDetectorTestImpl, DanglingUntriaged>
      network_portal_detector_ = nullptr;
  std::unique_ptr<base::AutoReset<bool>> branded_build_override_;
};

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest, ControlFlowMain) {
  TestControlFlowMain();
}

// This test verifies that if WizardController fails to apply a non-critical
// update before the OOBE is marked complete, it allows the user to proceed to
// log in.
IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest,
                       ControlFlowErrorUpdateNonCriticalUpdate) {
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, ShowImpl()).Times(0);
  EXPECT_CALL(*mock_network_screen_, ShowImpl()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, HideImpl()).Times(1);
  mock_welcome_screen_->ExitScreen(WelcomeScreen::Result::kNext);

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_network_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_update_screen_, ShowImpl()).Times(1);

  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, ShowImpl()).Times(0);
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(UserCreationView::kScreenId);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, HideImpl()).Times(0);

  EXPECT_FALSE(ExistingUserController::current_controller() == nullptr);
}

// This test verifies that if WizardController fails to apply a critical update
// before the OOBE is marked complete, it goes back the network selection
// screen and thus prevents the user from proceeding to log in.
IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest,
                       ControlFlowErrorUpdateCriticalUpdate) {
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, ShowImpl()).Times(0);
  EXPECT_CALL(*mock_network_screen_, ShowImpl()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, HideImpl()).Times(1);
  mock_welcome_screen_->ExitScreen(WelcomeScreen::Result::kNext);

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_network_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_update_screen_, ShowImpl()).Times(1);

  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, ShowImpl()).Times(0);
  EXPECT_CALL(*mock_network_screen_, ShowImpl()).Times(1);
  EXPECT_CALL(*mock_network_screen_, HideImpl()).Times(0);  // last transition
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_ERROR);
  CheckCurrentScreen(NetworkScreenView::kScreenId);
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest, ControlFlowSkipUpdateEnroll) {
  InitNetworkPortalDetector();

  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, ShowImpl()).Times(0);
  EXPECT_CALL(*mock_network_screen_, ShowImpl()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, HideImpl()).Times(1);
  mock_welcome_screen_->ExitScreen(WelcomeScreen::Result::kNext);

  EXPECT_CALL(*mock_network_screen_, HideImpl()).Times(1);

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, ShowImpl()).Times(0);

  WizardController::default_controller()
      ->wizard_context_->enrollment_triggered_early = true;
  EXPECT_CALL(*mock_enrollment_screen_view_,
              SetEnrollmentConfig(
                  EnrollmentModeMatches(policy::EnrollmentConfig::MODE_MANUAL)))
      .Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, ShowImpl()).Times(0);
  EXPECT_CALL(*mock_enrollment_screen_, ShowImpl()).Times(1);
  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(EnrollmentScreenView::kScreenId);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, HideImpl()).Times(0);
  EXPECT_CALL(*mock_enrollment_screen_, HideImpl()).Times(0);
  content::RunAllPendingInMessageLoop();
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest,
                       ControlFlowEnrollmentCompleted) {
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, ShowImpl()).Times(0);
  EXPECT_CALL(*mock_enrollment_screen_view_,
              SetEnrollmentConfig(
                  EnrollmentModeMatches(policy::EnrollmentConfig::MODE_MANUAL)))
      .Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, ShowImpl()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, HideImpl()).Times(1);

  WizardController::default_controller()->AdvanceToScreen(
      EnrollmentScreenView::kScreenId);
  CheckCurrentScreen(EnrollmentScreenView::kScreenId);
  mock_enrollment_screen_->ExitScreen(EnrollmentScreen::Result::COMPLETED);

  EXPECT_FALSE(ExistingUserController::current_controller() == nullptr);
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest,
                       ControlFlowWrongHWIDScreenFromLogin) {
  CheckCurrentScreen(WelcomeView::kScreenId);

  // Verify and clear all expectations on the mock welcome screen before setting
  // new ones.
  testing::Mock::VerifyAndClearExpectations(mock_welcome_screen_);

  EXPECT_CALL(*mock_welcome_screen_, HideImpl()).Times(1);
  LoginDisplayHost::default_host()->StartSignInScreen();
  EXPECT_FALSE(ExistingUserController::current_controller() == nullptr);

  EXPECT_CALL(*mock_wrong_hwid_screen_, ShowImpl()).Times(1);
  WizardController::default_controller()->AdvanceToScreen(
      WrongHWIDScreenView::kScreenId);

  CheckCurrentScreen(WrongHWIDScreenView::kScreenId);

  // Verify and clear all expectations on the mock wrong hwid screen before
  // setting new ones.
  testing::Mock::VerifyAndClearExpectations(mock_wrong_hwid_screen_);

  // After warning is skipped, user returns to sign-in screen.
  // And this destroys WizardController.
  EXPECT_CALL(*mock_wrong_hwid_screen_, HideImpl()).Times(1);
  GetWrongHWIDScreen()->OnExit();
  EXPECT_FALSE(ExistingUserController::current_controller() == nullptr);
}

// This parameterized test class extends WizardControllerFlowTest to verify how
// WizardController behaves if it does not find or fails to apply an update
// after the OOBE is marked complete.
class WizardControllerUpdateAfterCompletedOobeTest
    : public WizardControllerFlowTest,
      public testing::WithParamInterface<UpdateScreen::Result>,
      public LocalStateMixin::Delegate {
 public:
  WizardControllerUpdateAfterCompletedOobeTest(
      const WizardControllerUpdateAfterCompletedOobeTest&) = delete;
  WizardControllerUpdateAfterCompletedOobeTest& operator=(
      const WizardControllerUpdateAfterCompletedOobeTest&) = delete;

 protected:
  WizardControllerUpdateAfterCompletedOobeTest() = default;

  // LocalStateMixin::Delegate:
  void SetUpLocalState() override {
    StartupUtils::MarkOobeCompleted();  // Pretend OOBE was complete.
  }

 private:
  LocalStateMixin local_state_mixin_{&mixin_host_, this};
};

// This test verifies that if WizardController reports any result after the
// OOBE is marked complete, it allows the user to proceed to log in.
IN_PROC_BROWSER_TEST_P(WizardControllerUpdateAfterCompletedOobeTest,
                       ControlFlowErrorUpdate) {
  CheckCurrentScreen(WelcomeView::kScreenId);

  // Verify and clear all expectations on the mock welcome screen before setting
  // new ones.
  testing::Mock::VerifyAndClearExpectations(mock_welcome_screen_);

  EXPECT_CALL(*mock_update_screen_, ShowImpl()).Times(0);
  EXPECT_CALL(*mock_network_screen_, ShowImpl()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, HideImpl()).Times(1);
  mock_welcome_screen_->ExitScreen(WelcomeScreen::Result::kNext);

  CheckCurrentScreen(NetworkScreenView::kScreenId);

  // Verify and clear all expectations on the mock network screen before setting
  // new ones.
  testing::Mock::VerifyAndClearExpectations(mock_network_screen_);

  EXPECT_CALL(*mock_network_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_update_screen_, ShowImpl()).Times(1);

  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(UpdateView::kScreenId);

  testing::Mock::VerifyAndClearExpectations(mock_update_screen_);
  EXPECT_CALL(*mock_update_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, ShowImpl()).Times(0);
  mock_update_screen_->RunExit(GetParam());

  CheckCurrentScreen(UserCreationView::kScreenId);

  testing::Mock::VerifyAndClearExpectations(mock_auto_enrollment_check_screen_);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, HideImpl()).Times(0);

  EXPECT_NE(nullptr, ExistingUserController::current_controller());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WizardControllerUpdateAfterCompletedOobeTest,
    testing::Values(UpdateScreen::Result::UPDATE_NOT_REQUIRED,
                    UpdateScreen::Result::UPDATE_ERROR));

class WizardControllerDeviceStateTest : public WizardControllerFlowTest {
 public:
  WizardControllerDeviceStateTest(const WizardControllerDeviceStateTest&) =
      delete;
  WizardControllerDeviceStateTest& operator=(
      const WizardControllerDeviceStateTest&) = delete;

 protected:
  WizardControllerDeviceStateTest() {
    fake_statistics_provider_.SetMachineStatistic(system::kSerialNumberKey,
                                                  "test");
    fake_statistics_provider_.SetMachineStatistic(system::kActivateDateKey,
                                                  "2000-01");
    fake_statistics_provider_.SetVpdStatus(
        system::StatisticsProvider::VpdStatus::kValid);
    // Make all requests to DMServer fail with net::ERR_CONNECTION_REFUSED.
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          if (request.url.spec().starts_with(kDMServerURLPrefix)) {
            test_url_loader_factory_.AddResponse(
                request.url, network::mojom::URLResponseHead::New(),
                std::string(),
                network::URLLoaderCompletionStatus(
                    net::ERR_CONNECTION_REFUSED));
          }
        }));
  }

  static policy::AutoEnrollmentController* auto_enrollment_controller() {
    return WizardController::default_controller()
        ->GetAutoEnrollmentController();
  }

  static void WaitForAutoEnrollmentState(policy::AutoEnrollmentState state) {
    base::RunLoop loop;
    base::CallbackListSubscription progress_subscription =
        auto_enrollment_controller()->RegisterProgressCallback(
            base::BindRepeating(&QuitLoopOnAutoEnrollmentProgress, state,
                                &loop));
    loop.Run();
  }

  // WizardControllerFlowTest:
  void SetUpOnMainThread() override {
    WizardControllerFlowTest::SetUpOnMainThread();

    histogram_tester_ = std::make_unique<base::HistogramTester>();

    // Initialize the FakeShillManagerClient. This does not happen
    // automatically because of the `DBusThreadManager::Initialize`
    // call in `SetUpInProcessBrowserTestFixture`. See https://crbug.com/847422.
    // TODO(pmarko): Find a way for FakeShillManagerClient to be initialized
    // automatically (https://crbug.com/847422).
    ShillManagerClient::Get()->GetTestInterface()->SetupDefaultEnvironment();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WizardControllerFlowTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableForcedReEnrollment,
        policy::AutoEnrollmentTypeChecker::kForcedReEnrollmentAlways);
    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnrollmentInitialModulus, "1");
    command_line->AppendSwitchASCII(switches::kEnterpriseEnrollmentModulusLimit,
                                    "2");
  }

  system::ScopedFakeStatisticsProvider fake_statistics_provider_;

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

 private:
  DeviceStateMixin device_state_{&mixin_host_,
                                 DeviceStateMixin::State::BEFORE_OOBE};

  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(WizardControllerDeviceStateTest,
                       AutoEnrollmentControllerStartedAfterNetworkScreen) {
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_welcome_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_network_screen_, ShowImpl()).Times(1);
  mock_welcome_screen_->ExitScreen(WelcomeScreen::Result::kNext);

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_network_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_update_screen_, ShowImpl()).Times(1);
  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  content::RunAllTasksUntilIdle();

  // Verify that the state fetch has started or already completed.
  EXPECT_TRUE(auto_enrollment_controller()->IsInProgress() ||
              auto_enrollment_controller()->state().has_value());
}

IN_PROC_BROWSER_TEST_F(WizardControllerDeviceStateTest,
                       ControlFlowNoForcedReEnrollmentOnFirstBoot) {
  fake_statistics_provider_.ClearMachineStatistic(system::kActivateDateKey);
  EXPECT_NE(policy::AutoEnrollmentResult::kNoEnrollment,
            auto_enrollment_controller()->state());

  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_welcome_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_network_screen_, ShowImpl()).Times(1);
  mock_welcome_screen_->ExitScreen(WelcomeScreen::Result::kNext);

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_network_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_update_screen_, ShowImpl()).Times(1);
  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, ShowImpl()).Times(0);
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(UserCreationView::kScreenId);
  EXPECT_EQ(policy::AutoEnrollmentResult::kNoEnrollment,
            auto_enrollment_controller()->state());
  EXPECT_EQ(1,
            FakeInstallAttributesClient::Get()
                ->remove_firmware_management_parameters_from_tpm_call_count());
  EXPECT_EQ(1, FakeSessionManagerClient::Get()
                   ->clear_forced_re_enrollment_vpd_call_count());
}

class WizardControllerDeviceLegacyTest
    : public WizardControllerDeviceStateTest {
 public:
  WizardControllerDeviceLegacyTest(
      const WizardControllerDeviceLegacyTest&) = delete;
  WizardControllerDeviceLegacyTest& operator=(
      const WizardControllerDeviceLegacyTest&) = delete;

 protected:
  WizardControllerDeviceLegacyTest() = default;
  ~WizardControllerDeviceLegacyTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WizardControllerDeviceStateTest::SetUpCommandLine(command_line);

    // Explicitly test legacy state determination flow.
    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableUnifiedStateDetermination,
        policy::AutoEnrollmentTypeChecker::kUnifiedStateDeterminationNever);
  }
};

IN_PROC_BROWSER_TEST_F(WizardControllerDeviceLegacyTest,
                       ServerAdvertisedReEnrollment) {
  base::Value::Dict device_state;
  device_state.Set(
      policy::kDeviceStateMode,
      base::Value(policy::kDeviceStateRestoreModeReEnrollmentRequested));
  g_browser_process->local_state()->SetDict(prefs::kServerBackedDeviceState,
                                            std::move(device_state));

  EXPECT_CALL(*mock_network_screen_, ShowImpl()).Times(1);
  EXPECT_CALL(*mock_update_screen_, ShowImpl()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, ShowImpl()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, ShowImpl()).Times(1);

  mock_welcome_screen_->ExitScreen(WelcomeScreen::Result::kNext);
  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  base::RunLoop().RunUntilIdle();
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);
  mock_auto_enrollment_check_screen_->ExitScreen();

  CheckCurrentScreen(EnrollmentScreenView::kScreenId);
  mock_enrollment_screen_->ExitScreen(EnrollmentScreen::Result::BACK);
  CheckCurrentScreen(UserCreationView::kScreenId);
  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
}

// TODO(crbug.com/41429868) Flaky time outs on Linux ChromiumOS ASan LSan bot.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_ControlFlowDeviceDisabled DISABLED_ControlFlowDeviceDisabled
#else
#define MAYBE_ControlFlowDeviceDisabled ControlFlowDeviceDisabled
#endif
IN_PROC_BROWSER_TEST_F(WizardControllerDeviceLegacyTest,
                       MAYBE_ControlFlowDeviceDisabled) {
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_welcome_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_network_screen_, ShowImpl()).Times(1);
  mock_welcome_screen_->ExitScreen(WelcomeScreen::Result::kNext);

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_network_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_update_screen_, ShowImpl()).Times(1);

  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, ShowImpl()).Times(1);
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, HideImpl()).Times(1);
  mock_auto_enrollment_check_screen_->RealShow();

  // Wait for auto-enrollment controller to encounter the connection error.
  WaitForAutoEnrollmentState(kAutoEnrollmentConnectionError);

  // The error screen shows up if device state could not be retrieved.
  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  EXPECT_EQ(AutoEnrollmentCheckScreenView::kScreenId.AsId(),
            GetErrorScreen()->GetParentScreen());
  base::Value::Dict device_state;
  device_state.Set(policy::kDeviceStateMode,
                   base::Value(policy::kDeviceStateModeDisabled));
  device_state.Set(policy::kDeviceStateDisabledMessage,
                   base::Value(kDisabledMessage));
  g_browser_process->local_state()->SetDict(prefs::kServerBackedDeviceState,
                                            std::move(device_state));
  EXPECT_CALL(*device_disabled_screen_view_,
              Show(Field(&DeviceDisabledScreenView::Params::message,
                         Eq(kDisabledMessage))))
      .Times(1);
  mock_auto_enrollment_check_screen_->ExitScreen();

  base::RunLoop().RunUntilIdle();
  ResetAutoEnrollmentCheckScreen();

  // Make sure the device disabled screen is shown.
  CheckCurrentScreen(DeviceDisabledScreenView::kScreenId);

  EXPECT_EQ(0,
            FakeInstallAttributesClient::Get()
                ->remove_firmware_management_parameters_from_tpm_call_count());
  EXPECT_EQ(0, FakeSessionManagerClient::Get()
                   ->clear_forced_re_enrollment_vpd_call_count());

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
}

// Allows testing different behavior if forced re-enrollment is performed but
// not explicitly required (instantiated with `false`) vs. if forced
// re-enrollment is explicitly required (instantiated with `true`).
class WizardControllerDeviceStateExplicitRequirementTest
    : public WizardControllerDeviceStateTest,
      public testing::WithParamInterface<bool /* fre_explicitly_required */> {
 public:
  WizardControllerDeviceStateExplicitRequirementTest(
      const WizardControllerDeviceStateExplicitRequirementTest&) = delete;
  WizardControllerDeviceStateExplicitRequirementTest& operator=(
      const WizardControllerDeviceStateExplicitRequirementTest&) = delete;

 protected:
  WizardControllerDeviceStateExplicitRequirementTest() {
    if (IsFREExplicitlyRequired()) {
      fake_statistics_provider_.SetMachineStatistic(system::kCheckEnrollmentKey,
                                                    "1");
    }
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WizardControllerDeviceStateTest::SetUpCommandLine(command_line);

    // Explicitly test legacy state determination flow.
    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableUnifiedStateDetermination,
        policy::AutoEnrollmentTypeChecker::kUnifiedStateDeterminationNever);
  }

  // Returns true if forced re-enrollment was explicitly required (which
  // corresponds to the check_enrollment VPD value being set to "1").
  bool IsFREExplicitlyRequired() { return GetParam(); }
};

// Test the control flow for Forced Re-Enrollment. First, a connection error
// occurs, leading to a network error screen. On the network error screen, the
// test verifies that the user may enter a guest session if FRE was not
// explicitly required, and that the user may not enter a guest session if FRE
// was explicitly required. Then, a retry is performed and FRE indicates that
// the device should be enrolled.
IN_PROC_BROWSER_TEST_P(WizardControllerDeviceStateExplicitRequirementTest,
                       ControlFlowForcedReEnrollment) {
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_welcome_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_network_screen_, ShowImpl()).Times(1);
  mock_welcome_screen_->ExitScreen(WelcomeScreen::Result::kNext);

  CheckCurrentScreen(NetworkScreenView::kScreenId);

  EXPECT_CALL(*mock_network_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_update_screen_, ShowImpl()).Times(1);

  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  // Let update screen smooth time process (time = 0ms).
  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, ShowImpl()).Times(1);
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, HideImpl()).Times(1);
  mock_auto_enrollment_check_screen_->RealShow();

  // Wait for auto-enrollment controller to encounter the connection error.
  WaitForAutoEnrollmentState(kAutoEnrollmentConnectionError);

  // The error screen shows up if there's no auto-enrollment decision.
  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  EXPECT_EQ(AutoEnrollmentCheckScreenView::kScreenId.AsId(),
            GetErrorScreen()->GetParentScreen());

  if (IsFREExplicitlyRequired()) {
    // Check that guest sign-in is not allowed on the network error screen
    // (because the check_enrollment VPD key was set to "1", making FRE
    // explicitly required).
    test::OobeJS().ExpectHiddenPath(kGuestSessionLink);
  } else {
    // Check that guest sign-in is allowed if FRE was not explicitly required.
    test::OobeJS().ExpectVisiblePath(kGuestSessionLink);
  }
  EXPECT_EQ(0,
            FakeInstallAttributesClient::Get()
                ->remove_firmware_management_parameters_from_tpm_call_count());
  EXPECT_EQ(0, FakeSessionManagerClient::Get()
                   ->clear_forced_re_enrollment_vpd_call_count());

  base::Value::Dict device_state;
  device_state.Set(
      policy::kDeviceStateMode,
      base::Value(policy::kDeviceStateRestoreModeReEnrollmentEnforced));
  g_browser_process->local_state()->SetDict(prefs::kServerBackedDeviceState,
                                            std::move(device_state));
  EXPECT_CALL(*mock_enrollment_screen_, ShowImpl()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_view_,
              SetEnrollmentConfig(EnrollmentModeMatches(
                  policy::EnrollmentConfig::MODE_SERVER_FORCED)))
      .Times(1);
  mock_auto_enrollment_check_screen_->ExitScreen();

  ResetAutoEnrollmentCheckScreen();

  // Make sure enterprise enrollment page shows up.
  CheckCurrentScreen(EnrollmentScreenView::kScreenId);
  mock_enrollment_screen_->ExitScreen(EnrollmentScreen::Result::COMPLETED);

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
}

// Tests that a server error occurs during a check for Forced Re-Enrollment.
// When Forced Re-Enrollment is not explicitly required (there is no
// "check_enrollment" VPD key), the expectation is that the server error is
// treated as "don't force enrollment".
// When Forced Re-Enrollment is explicitly required (the "check_enrollment" VPD
// key is set to "1"), the expectation is that a network error screen shows up
// (from which it's not possible to enter a Guest session).
IN_PROC_BROWSER_TEST_P(WizardControllerDeviceStateExplicitRequirementTest,
                       ControlFlowForcedReEnrollmentServerError) {
  ScopedFakeAutoEnrollmentClientFactory fake_auto_enrollment_client_factory(
      auto_enrollment_controller());

  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_welcome_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_network_screen_, ShowImpl()).Times(1);
  mock_welcome_screen_->ExitScreen(WelcomeScreen::Result::kNext);

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_network_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_update_screen_, ShowImpl()).Times(1);

  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  // Let update screen smooth time process (time = 0ms).
  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, ShowImpl()).Times(1);
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  mock_auto_enrollment_check_screen_->RealShow();

  policy::FakeAutoEnrollmentClient* fake_auto_enrollment_client =
      fake_auto_enrollment_client_factory.WaitAutoEnrollmentClientCreated();
  if (IsFREExplicitlyRequired()) {
    // Expect that the auto enrollment screen will be hidden, because OOBE is
    // switching to the error screen.
    EXPECT_CALL(*mock_auto_enrollment_check_screen_, HideImpl()).Times(1);

    // Make AutoEnrollmentClient notify the controller that a server error
    // occurred.
    fake_auto_enrollment_client->SetState(kAutoEnrollmentServerError);
    base::RunLoop().RunUntilIdle();

    // The error screen shows up.
    EXPECT_FALSE(StartupUtils::IsOobeCompleted());
    CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
    EXPECT_EQ(AutoEnrollmentCheckScreenView::kScreenId.AsId(),
              GetErrorScreen()->GetParentScreen());

    // Check that guest sign-in is not allowed on the network error screen
    // (because the check_enrollment VPD key was set to "1", making FRE
    // explicitly required).
    test::OobeJS().ExpectHiddenPath(kGuestSessionLink);

    base::Value::Dict device_state;
    device_state.Set(
        policy::kDeviceStateMode,
        base::Value(policy::kDeviceStateRestoreModeReEnrollmentEnforced));
    g_browser_process->local_state()->SetDict(prefs::kServerBackedDeviceState,
                                              std::move(device_state));
    EXPECT_CALL(*mock_enrollment_screen_, ShowImpl()).Times(1);
    EXPECT_CALL(*mock_enrollment_screen_view_,
                SetEnrollmentConfig(EnrollmentModeMatches(
                    policy::EnrollmentConfig::MODE_SERVER_FORCED)))
        .Times(1);
    fake_auto_enrollment_client->SetState(
        policy::AutoEnrollmentResult::kEnrollment);
    mock_auto_enrollment_check_screen_->ExitScreen();

    ResetAutoEnrollmentCheckScreen();

    // Make sure enterprise enrollment page shows up.
    CheckCurrentScreen(EnrollmentScreenView::kScreenId);
    mock_enrollment_screen_->ExitScreen(EnrollmentScreen::Result::COMPLETED);

    EXPECT_TRUE(StartupUtils::IsOobeCompleted());
    EXPECT_EQ(
        0, FakeInstallAttributesClient::Get()
               ->remove_firmware_management_parameters_from_tpm_call_count());
    EXPECT_EQ(0, FakeSessionManagerClient::Get()
                     ->clear_forced_re_enrollment_vpd_call_count());
  } else {
    // Make AutoEnrollmentClient notify the controller that a server error
    // occurred.
    fake_auto_enrollment_client->SetState(kAutoEnrollmentServerError);
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(StartupUtils::IsOobeCompleted());
    // Don't expect that the auto enrollment screen will be hidden, because
    // OOBE is exited from the auto enrollment screen. Instead only expect
    // that the sign-in screen is reached.
    OobeScreenWaiter(GetFirstSigninScreen()).Wait();
    EXPECT_EQ(
        0, FakeInstallAttributesClient::Get()
               ->remove_firmware_management_parameters_from_tpm_call_count());
    EXPECT_EQ(0, FakeSessionManagerClient::Get()
                     ->clear_forced_re_enrollment_vpd_call_count());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         WizardControllerDeviceStateExplicitRequirementTest,
                         testing::Values(false, true));

class WizardControllerUnifiedEnrollmentTest
    : public WizardControllerDeviceStateTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WizardControllerDeviceStateTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableUnifiedStateDetermination,
        policy::AutoEnrollmentTypeChecker::kUnifiedStateDeterminationAlways);
  }

  void ProgressUntilAutoEnrollmentCheckScreen() {
    CheckCurrentScreen(WelcomeView::kScreenId);
    EXPECT_CALL(*mock_welcome_screen_, HideImpl());
    EXPECT_CALL(*mock_network_screen_, ShowImpl());
    mock_welcome_screen_->ExitScreen(WelcomeScreen::Result::kNext);

    CheckCurrentScreen(NetworkScreenView::kScreenId);
    EXPECT_CALL(*mock_network_screen_, HideImpl());
    EXPECT_CALL(*mock_update_screen_, ShowImpl());

    mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

    // Let update screen smooth time process (time = 0ms).
    base::RunLoop().RunUntilIdle();

    CheckCurrentScreen(UpdateView::kScreenId);
    EXPECT_CALL(*mock_update_screen_, HideImpl());
    EXPECT_CALL(*mock_auto_enrollment_check_screen_, ShowImpl());
    mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

    CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
    mock_auto_enrollment_check_screen_->RealShow();
  }
};

// Tests that when EnrollmentStateFetcher reports kNoEnrollment, we skip
// enrollment screen and set state correctly.
IN_PROC_BROWSER_TEST_F(WizardControllerUnifiedEnrollmentTest, NoEnrollment) {
  ScopedEnrollmentStateFetcherFactory fetcher_factory(
      auto_enrollment_controller());
  ProgressUntilAutoEnrollmentCheckScreen();
  fetcher_factory.WaitUntilEnrollmentStateFetcherCreated();

  fetcher_factory.ReportEnrollmentState(
      policy::AutoEnrollmentResult::kNoEnrollment);

  CheckCurrentScreen(UserCreationView::kScreenId);
  EXPECT_CALL(*mock_enrollment_screen_, ShowImpl()).Times(0);
  EXPECT_CALL(*mock_enrollment_screen_, HideImpl()).Times(0);
  EXPECT_EQ(policy::AutoEnrollmentResult::kNoEnrollment,
            auto_enrollment_controller()->state());
  EXPECT_EQ(policy::AutoEnrollmentTypeChecker::CheckType::
                kForcedReEnrollmentExplicitlyRequired,
            auto_enrollment_controller()->auto_enrollment_check_type());
}

// Tests that when EnrollmentStateFetcher reports state keys retrieval error, we
// show an error on enrollment check screen and that it is not possible to enter
// guest mode.
IN_PROC_BROWSER_TEST_F(WizardControllerUnifiedEnrollmentTest,
                       BlockedByCommunicationErrorOnStateKeysRetrieval) {
  ScopedEnrollmentStateFetcherFactory fetcher_factory(
      auto_enrollment_controller());
  ProgressUntilAutoEnrollmentCheckScreen();
  fetcher_factory.WaitUntilEnrollmentStateFetcherCreated();

  fetcher_factory.ReportEnrollmentState(kAutoEnrollmentStateKeysRetrievalError);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  EXPECT_EQ(AutoEnrollmentCheckScreenView::kScreenId.AsId(),
            GetErrorScreen()->GetParentScreen());
  test::OobeJS().ExpectHiddenPath(kGuestSessionLink);
}

// Tests that when EnrollmentStateFetcher reports kServerError, we show an error
// on enrollment check screen and that it is not possible to enter guest mode.
IN_PROC_BROWSER_TEST_F(WizardControllerUnifiedEnrollmentTest, ServerError) {
  ScopedEnrollmentStateFetcherFactory fetcher_factory(
      auto_enrollment_controller());
  ProgressUntilAutoEnrollmentCheckScreen();
  fetcher_factory.WaitUntilEnrollmentStateFetcherCreated();

  fetcher_factory.ReportEnrollmentState(kAutoEnrollmentServerError);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  EXPECT_EQ(AutoEnrollmentCheckScreenView::kScreenId.AsId(),
            GetErrorScreen()->GetParentScreen());
  test::OobeJS().ExpectHiddenPath(kGuestSessionLink);
}

// Tests that when EnrollmentStateFetcher reports kConnectionError, we show an
// error on enrollment check screen and that it is not possible to enter guest
// mode (like in FRE).
IN_PROC_BROWSER_TEST_F(WizardControllerUnifiedEnrollmentTest, ConnectionError) {
  ScopedEnrollmentStateFetcherFactory fetcher_factory(
      auto_enrollment_controller());
  ProgressUntilAutoEnrollmentCheckScreen();
  fetcher_factory.WaitUntilEnrollmentStateFetcherCreated();

  fetcher_factory.ReportEnrollmentState(kAutoEnrollmentConnectionError);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  EXPECT_EQ(AutoEnrollmentCheckScreenView::kScreenId.AsId(),
            GetErrorScreen()->GetParentScreen());
  test::OobeJS().ExpectHiddenPath(kGuestSessionLink);
}

// Tests that when EnrollmentStateFetcher reports kEnrollment and configures
// device state mode as FRE, we shown enrollment screen.
IN_PROC_BROWSER_TEST_F(WizardControllerUnifiedEnrollmentTest, Enrollment) {
  ScopedEnrollmentStateFetcherFactory fetcher_factory(
      auto_enrollment_controller());
  ProgressUntilAutoEnrollmentCheckScreen();
  fetcher_factory.WaitUntilEnrollmentStateFetcherCreated();

  EXPECT_CALL(*mock_auto_enrollment_check_screen_, HideImpl());
  EXPECT_CALL(*mock_enrollment_screen_, ShowImpl());
  base::Value::Dict device_state;
  device_state.Set(
      policy::kDeviceStateMode,
      base::Value(policy::kDeviceStateRestoreModeReEnrollmentEnforced));
  g_browser_process->local_state()->SetDict(prefs::kServerBackedDeviceState,
                                            std::move(device_state));
  fetcher_factory.ReportEnrollmentState(
      policy::AutoEnrollmentResult::kEnrollment);

  CheckCurrentScreen(EnrollmentScreenView::kScreenId);
}

// Tests that when EnrollmentStateFetcher reports kDisabled and configures
// corresponding device state mode, we show device disabled screen.
IN_PROC_BROWSER_TEST_F(WizardControllerUnifiedEnrollmentTest, Disabled) {
  ScopedEnrollmentStateFetcherFactory fetcher_factory(
      auto_enrollment_controller());
  ProgressUntilAutoEnrollmentCheckScreen();
  fetcher_factory.WaitUntilEnrollmentStateFetcherCreated();

  EXPECT_CALL(*mock_auto_enrollment_check_screen_, HideImpl());
  EXPECT_CALL(*device_disabled_screen_view_,
              Show(Field(&DeviceDisabledScreenView::Params::message,
                         Eq(kDisabledMessage))));
  base::Value::Dict device_state;
  device_state.Set(policy::kDeviceStateMode,
                   base::Value(policy::kDeviceStateModeDisabled));
  device_state.Set(policy::kDeviceStateDisabledMessage,
                   base::Value(kDisabledMessage));
  g_browser_process->local_state()->SetDict(prefs::kServerBackedDeviceState,
                                            std::move(device_state));
  fetcher_factory.ReportEnrollmentState(
      policy::AutoEnrollmentResult::kDisabled);

  CheckCurrentScreen(DeviceDisabledScreenView::kScreenId);
}

// Tests that when EnrollmentStateFetcher reports kDisabled and configures
// corresponding device state mode, we show device disabled screen after
// attempting to enter demo mode.
IN_PROC_BROWSER_TEST_F(WizardControllerUnifiedEnrollmentTest,
                       DisabledDemoMode) {
  ScopedEnrollmentStateFetcherFactory fetcher_factory(
      auto_enrollment_controller());

  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_welcome_screen_, HideImpl());
  EXPECT_CALL(*mock_network_screen_, ShowImpl());

  WizardController::default_controller()->StartDemoModeSetup();

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_network_screen_, HideImpl());
  EXPECT_CALL(*mock_demo_preferences_screen_, ShowImpl());

  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  CheckCurrentScreen(DemoPreferencesScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_demo_preferences_screen_, HideImpl());
  EXPECT_CALL(*mock_update_screen_, ShowImpl());

  mock_demo_preferences_screen_->ExitScreen(
      DemoPreferencesScreen::Result::COMPLETED);

  // Let update screen smooth time process (time = 0ms).
  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, HideImpl());
  EXPECT_CALL(*mock_consolidated_consent_screen_, ShowImpl());

  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(ConsolidatedConsentScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_consolidated_consent_screen_, HideImpl());
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, ShowImpl());

  mock_consolidated_consent_screen_->ExitScreen(
      ConsolidatedConsentScreen::Result::ACCEPTED_DEMO_ONLINE);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  mock_auto_enrollment_check_screen_->RealShow();

  fetcher_factory.WaitUntilEnrollmentStateFetcherCreated();

  base::Value::Dict device_state;
  device_state.Set(policy::kDeviceStateMode,
                   base::Value(policy::kDeviceStateModeDisabled));
  device_state.Set(policy::kDeviceStateDisabledMessage,
                   base::Value(kDisabledMessage));
  g_browser_process->local_state()->SetDict(prefs::kServerBackedDeviceState,
                                            std::move(device_state));

  EXPECT_CALL(*mock_auto_enrollment_check_screen_, HideImpl());
  EXPECT_CALL(*device_disabled_screen_view_,
              Show(Field(&DeviceDisabledScreenView::Params::message,
                         Eq(kDisabledMessage))));

  fetcher_factory.ReportEnrollmentState(
      policy::AutoEnrollmentResult::kDisabled);

  CheckCurrentScreen(DeviceDisabledScreenView::kScreenId);
}

// Tests that when EnrollmentStateFetcher times out, we set state correctly,
// show an error on enrollment check screen and that it is not possible to enter
// guest mode (like in FRE).
IN_PROC_BROWSER_TEST_F(WizardControllerUnifiedEnrollmentTest, Timeout) {
  ScopedEnrollmentStateFetcherFactory fetcher_factory(
      auto_enrollment_controller());
  ProgressUntilAutoEnrollmentCheckScreen();
  fetcher_factory.WaitUntilEnrollmentStateFetcherCreated();

  auto_enrollment_controller()->SafeguardTimerForTesting().FireNow();

  // Ensure that we show an error on enrollment check screen and that it is not
  // possible to enter guest mode (like in FRE).
  EXPECT_EQ(
      auto_enrollment_controller()->state(),
      ToAutoEnrollmentState(policy::AutoEnrollmentSafeguardTimeoutError{}));
  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  EXPECT_EQ(AutoEnrollmentCheckScreenView::kScreenId.AsId(),
            GetErrorScreen()->GetParentScreen());
  test::OobeJS().ExpectHiddenPath(kGuestSessionLink);
  histogram_tester()->ExpectBucketCount(
      "Enterprise.AutoEnrollmentControllerTimeout", 3 /*kTimeoutUnified*/, 1);
}

// Tests that AutoEnrollmentController does not create another
// EnrollmentStateFetcher while previous one is still running, but also that it
// does create one when previous has completed and reported a state.
IN_PROC_BROWSER_TEST_F(WizardControllerUnifiedEnrollmentTest, OneFetchAtATime) {
  ScopedEnrollmentStateFetcherFactory fetcher_factory(
      auto_enrollment_controller());
  ProgressUntilAutoEnrollmentCheckScreen();
  fetcher_factory.WaitUntilEnrollmentStateFetcherCreated();

  // This should determine that the enrollment state is already being fetched
  // and skip creating a new EnrollmentStateFetcher. Otherwise, it would fail an
  // expectation in fetcher_factory allowing only one EnrollmentStateFetcher to
  // be created.
  auto_enrollment_controller()->Start();

  // Simulate connection error, reset factory and attempt a retry.
  fetcher_factory.ReportEnrollmentState(kAutoEnrollmentConnectionError);
  fetcher_factory.Reset();
  auto_enrollment_controller()->Retry();

  fetcher_factory.WaitUntilEnrollmentStateFetcherCreated();
}

class WizardControllerDeviceStateWithInitialEnrollmentTest
    : public WizardControllerDeviceStateTest {
 public:
  WizardControllerDeviceStateWithInitialEnrollmentTest(
      const WizardControllerDeviceStateWithInitialEnrollmentTest&) = delete;
  WizardControllerDeviceStateWithInitialEnrollmentTest& operator=(
      const WizardControllerDeviceStateWithInitialEnrollmentTest&) = delete;

 protected:
  WizardControllerDeviceStateWithInitialEnrollmentTest() {
    fake_statistics_provider_.SetMachineStatistic(system::kSerialNumberKey,
                                                  "test");
    fake_statistics_provider_.SetMachineStatistic(system::kRlzBrandCodeKey,
                                                  "AABC");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WizardControllerDeviceStateTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableInitialEnrollment,
        policy::AutoEnrollmentTypeChecker::kInitialEnrollmentAlways);

    // Explicitly test legacy state determination flow.
    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableUnifiedStateDetermination,
        policy::AutoEnrollmentTypeChecker::kUnifiedStateDeterminationNever);
  }

  // Test initial enrollment. This method is shared by the tests for initial
  // enrollment for a device that is new or in consumer mode. |check_fre|
  // specifies if forced re-enrollment check is needed.
  void DoInitialEnrollment(bool check_fre) {
    fake_statistics_provider_.SetMachineStatistic(
        system::kRlzEmbargoEndDateKey,
        GenerateEmbargoEndDate(-15 /* days_offset */));
    base::Value::Dict device_state;
    device_state.Set(
        policy::kDeviceStateMode,
        base::Value(policy::kDeviceStateRestoreModeReEnrollmentEnforced));
    g_browser_process->local_state()->SetDict(prefs::kServerBackedDeviceState,
                                              std::move(device_state));
    CheckCurrentScreen(WelcomeView::kScreenId);
    EXPECT_CALL(*mock_welcome_screen_, HideImpl()).Times(1);
    EXPECT_CALL(*mock_network_screen_, ShowImpl()).Times(1);
    mock_welcome_screen_->ExitScreen(WelcomeScreen::Result::kNext);

    CheckCurrentScreen(NetworkScreenView::kScreenId);
    EXPECT_CALL(*mock_network_screen_, HideImpl()).Times(1);
    EXPECT_CALL(*mock_update_screen_, ShowImpl()).Times(1);

    mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

    if (check_fre) {
      // Wait for auto-enrollment controller to encounter the connection error.
      WaitForAutoEnrollmentState(
          ToAutoEnrollmentState(policy::AutoEnrollmentDMServerError{
              .dm_error = policy::DM_STATUS_REQUEST_FAILED,
              .network_error = net::ERR_CONNECTION_REFUSED}));

      // Let update screen smooth time process (time = 0ms).
      base::RunLoop().RunUntilIdle();
    }

    CheckCurrentScreen(UpdateView::kScreenId);
    EXPECT_CALL(*mock_update_screen_, HideImpl()).Times(1);
    if (check_fre) {
      EXPECT_CALL(*mock_auto_enrollment_check_screen_, ShowImpl()).Times(1);
    } else {
      EXPECT_CALL(*mock_auto_enrollment_check_screen_, ShowImpl()).Times(0);
      EXPECT_CALL(*mock_enrollment_screen_, ShowImpl()).Times(1);
      EXPECT_CALL(*mock_enrollment_screen_view_,
                  SetEnrollmentConfig(EnrollmentModeMatches(
                      policy::EnrollmentConfig::MODE_SERVER_FORCED)))
          .Times(1);
    }
    mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

    if (check_fre) {
      CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
      EXPECT_CALL(*mock_auto_enrollment_check_screen_, HideImpl()).Times(1);
      mock_auto_enrollment_check_screen_->RealShow();

      EXPECT_CALL(*mock_enrollment_screen_, ShowImpl()).Times(1);
      EXPECT_CALL(*mock_enrollment_screen_view_,
                  SetEnrollmentConfig(EnrollmentModeMatches(
                      policy::EnrollmentConfig::MODE_SERVER_FORCED)))
          .Times(1);
      mock_auto_enrollment_check_screen_->ExitScreen();
    } else {
      EXPECT_CALL(*mock_auto_enrollment_check_screen_, HideImpl()).Times(0);
    }

    // The error screen shows up if there's no auto-enrollment decision.
    EXPECT_FALSE(StartupUtils::IsOobeCompleted());
    if (check_fre) {
      EXPECT_EQ(AutoEnrollmentCheckScreenView::kScreenId.AsId(),
                GetErrorScreen()->GetParentScreen());
    }

    ResetAutoEnrollmentCheckScreen();

    // Make sure enterprise enrollment page shows up.
    CheckCurrentScreen(EnrollmentScreenView::kScreenId);
    mock_enrollment_screen_->ExitScreen(EnrollmentScreen::Result::COMPLETED);

    EXPECT_TRUE(StartupUtils::IsOobeCompleted());
  }

  SystemClockClient::TestInterface* system_clock_client() {
    return SystemClockClient::Get()->GetTestInterface();
  }
};

// Tests that a device that is brand new properly does initial enrollment.
IN_PROC_BROWSER_TEST_F(WizardControllerDeviceStateWithInitialEnrollmentTest,
                       ControlFlowInitialEnrollment) {
  fake_statistics_provider_.ClearMachineStatistic(system::kActivateDateKey);

  DoInitialEnrollment(/*check_fre=*/true);
}

// Tests that a device that is in consumer mode can do another initial
// enrollment.
IN_PROC_BROWSER_TEST_F(WizardControllerDeviceStateWithInitialEnrollmentTest,
                       ControlFlowSecondaryInitialEnrollment) {
  // Mark the device has being in consumer mode.
  fake_statistics_provider_.SetMachineStatistic(system::kCheckEnrollmentKey,
                                                "0");

  // TODO(igorcov): Change to /*check_fre=*/false when b/238592446 is fixed.
  // At that point we might need to add
  //   WizardController::default_controller()
  //    ->GetAutoEnrollmentControllerForTesting()
  //    ->SetRlweClientFactoryForTesting(
  //        base::BindRepeating(&policy::psm::FakeRlweClient::Create));
  DoInitialEnrollment(/*check_fre=*/true);
}

// Tests that a server error occurs during the Initial Enrollment check.  The
// expectation is that a network error screen shows up (from which it's possible
// to enter a Guest session).
IN_PROC_BROWSER_TEST_F(WizardControllerDeviceStateWithInitialEnrollmentTest,
                       ControlFlowInitialEnrollmentServerError) {
  ScopedFakeAutoEnrollmentClientFactory fake_auto_enrollment_client_factory(
      auto_enrollment_controller());

  fake_statistics_provider_.ClearMachineStatistic(system::kActivateDateKey);
  fake_statistics_provider_.SetMachineStatistic(
      system::kRlzEmbargoEndDateKey,
      GenerateEmbargoEndDate(-15 /* days_offset */));
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_welcome_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_network_screen_, ShowImpl()).Times(1);
  mock_welcome_screen_->ExitScreen(WelcomeScreen::Result::kNext);

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_network_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_update_screen_, ShowImpl()).Times(1);

  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  // Let update screen smooth time process (time = 0ms).
  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, ShowImpl()).Times(1);
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  mock_auto_enrollment_check_screen_->RealShow();

  policy::FakeAutoEnrollmentClient* fake_auto_enrollment_client =
      fake_auto_enrollment_client_factory.WaitAutoEnrollmentClientCreated();

  // Expect that the auto enrollment screen will be hidden, because OOBE is
  // switching to the error screen.
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, HideImpl()).Times(1);

  // Make AutoEnrollmentClient notify the controller that a server error
  // occurred.
  fake_auto_enrollment_client->SetState(kAutoEnrollmentServerError);
  base::RunLoop().RunUntilIdle();

  // The error screen shows up if there's no auto-enrollment decision.
  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_EQ(AutoEnrollmentCheckScreenView::kScreenId.AsId(),
            GetErrorScreen()->GetParentScreen());

  // Check that guest sign-in is allowed on the network error screen for initial
  // enrollment.
  test::OobeJS().ExpectVisiblePath(kGuestSessionLink);

  base::Value::Dict device_state;
  device_state.Set(
      policy::kDeviceStateMode,
      base::Value(policy::kDeviceStateRestoreModeReEnrollmentEnforced));
  g_browser_process->local_state()->SetDict(prefs::kServerBackedDeviceState,
                                            std::move(device_state));
  EXPECT_CALL(*mock_enrollment_screen_, ShowImpl()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_view_,
              SetEnrollmentConfig(EnrollmentModeMatches(
                  policy::EnrollmentConfig::MODE_SERVER_FORCED)))
      .Times(1);
  fake_auto_enrollment_client->SetState(
      policy::AutoEnrollmentResult::kEnrollment);
  mock_auto_enrollment_check_screen_->ExitScreen();

  ResetAutoEnrollmentCheckScreen();

  // Make sure enterprise enrollment page shows up.
  CheckCurrentScreen(EnrollmentScreenView::kScreenId);
  mock_enrollment_screen_->ExitScreen(EnrollmentScreen::Result::COMPLETED);

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
}

IN_PROC_BROWSER_TEST_F(WizardControllerDeviceStateWithInitialEnrollmentTest,
                       ControlFlowNoInitialEnrollmentDuringEmbargoPeriod) {
  system_clock_client()->SetNetworkSynchronized(true);
  system_clock_client()->NotifyObserversSystemClockUpdated();

  fake_statistics_provider_.ClearMachineStatistic(system::kActivateDateKey);
  fake_statistics_provider_.SetMachineStatistic(
      system::kRlzEmbargoEndDateKey,
      GenerateEmbargoEndDate(1 /* days_offset */));
  EXPECT_NE(policy::AutoEnrollmentResult::kNoEnrollment,
            auto_enrollment_controller()->state());

  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_welcome_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_network_screen_, ShowImpl()).Times(1);
  mock_welcome_screen_->ExitScreen(WelcomeScreen::Result::kNext);

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_network_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_update_screen_, ShowImpl()).Times(1);

  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  // Let update screen smooth time process (time = 0ms).
  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, ShowImpl()).Times(0);
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(UserCreationView::kScreenId);
  EXPECT_EQ(policy::AutoEnrollmentResult::kNoEnrollment,
            auto_enrollment_controller()->state());
}

IN_PROC_BROWSER_TEST_F(WizardControllerDeviceStateWithInitialEnrollmentTest,
                       ControlFlowWaitSystemClockSyncThenEmbargoPeriod) {
  fake_statistics_provider_.ClearMachineStatistic(system::kActivateDateKey);
  fake_statistics_provider_.SetMachineStatistic(
      system::kRlzEmbargoEndDateKey,
      GenerateEmbargoEndDate(1 /* days_offset */));
  EXPECT_NE(policy::AutoEnrollmentResult::kNoEnrollment,
            auto_enrollment_controller()->state());

  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_welcome_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_network_screen_, ShowImpl()).Times(1);
  mock_welcome_screen_->ExitScreen(WelcomeScreen::Result::kNext);

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_network_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_update_screen_, ShowImpl()).Times(1);

  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  // Let update screen smooth time process (time = 0ms).
  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, ShowImpl()).Times(1);
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  mock_auto_enrollment_check_screen_->RealShow();
  EXPECT_EQ(policy::AutoEnrollmentTypeChecker::CheckType::
                kUnknownDueToMissingSystemClockSync,
            auto_enrollment_controller()->auto_enrollment_check_type());

  system_clock_client()->SetNetworkSynchronized(true);
  system_clock_client()->NotifyObserversSystemClockUpdated();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(policy::AutoEnrollmentTypeChecker::CheckType::kNone,
            auto_enrollment_controller()->auto_enrollment_check_type());
  EXPECT_EQ(policy::AutoEnrollmentResult::kNoEnrollment,
            auto_enrollment_controller()->state());
}

IN_PROC_BROWSER_TEST_F(WizardControllerDeviceStateWithInitialEnrollmentTest,
                       ControlFlowWaitSystemClockSyncTimeout) {
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();

  base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner);
  fake_statistics_provider_.ClearMachineStatistic(system::kActivateDateKey);
  fake_statistics_provider_.SetMachineStatistic(
      system::kRlzEmbargoEndDateKey,
      GenerateEmbargoEndDate(1 /* days_offset */));
  EXPECT_NE(policy::AutoEnrollmentResult::kNoEnrollment,
            auto_enrollment_controller()->state());

  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_welcome_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_network_screen_, ShowImpl()).Times(1);
  mock_welcome_screen_->ExitScreen(WelcomeScreen::Result::kNext);

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_network_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_update_screen_, ShowImpl()).Times(1);

  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  // Let update screen smooth time process (time = 0ms).
  task_runner->RunUntilIdle();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, ShowImpl()).Times(1);
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  mock_auto_enrollment_check_screen_->RealShow();
  EXPECT_EQ(policy::AutoEnrollmentTypeChecker::CheckType::
                kUnknownDueToMissingSystemClockSync,
            auto_enrollment_controller()->auto_enrollment_check_type());

  // The timeout is 45 seconds, see `auto_enrollment_controller.cc`.
  // Fast-forward by a bit more than that.
  task_runner->FastForwardBy(base::Seconds(45 + 1));

  EXPECT_EQ(policy::AutoEnrollmentTypeChecker::CheckType::
                kUnknownDueToMissingSystemClockSync,
            auto_enrollment_controller()->auto_enrollment_check_type());
  EXPECT_EQ(
      auto_enrollment_controller()->state(),
      ToAutoEnrollmentState(policy::AutoEnrollmentSystemClockSyncError{}));
}

IN_PROC_BROWSER_TEST_F(WizardControllerDeviceStateWithInitialEnrollmentTest,
                       ControlFlowWaitSystemClockSyncThenInitialEnrollment) {
  ScopedFakeAutoEnrollmentClientFactory fake_auto_enrollment_client_factory(
      auto_enrollment_controller());

  fake_statistics_provider_.ClearMachineStatistic(system::kActivateDateKey);
  fake_statistics_provider_.SetMachineStatistic(
      system::kRlzEmbargoEndDateKey,
      GenerateEmbargoEndDate(1 /* days_offset */));
  EXPECT_NE(policy::AutoEnrollmentResult::kNoEnrollment,
            auto_enrollment_controller()->state());

  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_welcome_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_network_screen_, ShowImpl()).Times(1);
  mock_welcome_screen_->ExitScreen(WelcomeScreen::Result::kNext);

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_network_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_update_screen_, ShowImpl()).Times(1);

  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  // Let update screen smooth time process (time = 0ms).
  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, ShowImpl()).Times(1);
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  mock_auto_enrollment_check_screen_->RealShow();
  EXPECT_EQ(policy::AutoEnrollmentTypeChecker::CheckType::
                kUnknownDueToMissingSystemClockSync,
            auto_enrollment_controller()->auto_enrollment_check_type());

  // Simulate that the clock moved forward, passing the embargo period, by
  // moving the embargo period back in time.
  fake_statistics_provider_.SetMachineStatistic(
      system::kRlzEmbargoEndDateKey,
      GenerateEmbargoEndDate(-1 /* days_offset */));
  base::Value::Dict device_state;
  device_state.Set(
      policy::kDeviceStateMode,
      base::Value(policy::kDeviceStateRestoreModeReEnrollmentEnforced));
  g_browser_process->local_state()->SetDict(prefs::kServerBackedDeviceState,
                                            std::move(device_state));

  system_clock_client()->SetNetworkSynchronized(true);
  system_clock_client()->NotifyObserversSystemClockUpdated();

  policy::FakeAutoEnrollmentClient* fake_auto_enrollment_client =
      fake_auto_enrollment_client_factory.WaitAutoEnrollmentClientCreated();

  EXPECT_CALL(*mock_auto_enrollment_check_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, ShowImpl()).Times(1);

  EXPECT_CALL(*mock_enrollment_screen_view_,
              SetEnrollmentConfig(EnrollmentModeMatches(
                  policy::EnrollmentConfig::MODE_SERVER_FORCED)))
      .Times(1);
  mock_auto_enrollment_check_screen_->ExitScreen();
  ResetAutoEnrollmentCheckScreen();

  fake_auto_enrollment_client->SetState(
      policy::AutoEnrollmentResult::kEnrollment);

  // Make sure enterprise enrollment page shows up.
  CheckCurrentScreen(EnrollmentScreenView::kScreenId);
  mock_enrollment_screen_->ExitScreen(EnrollmentScreen::Result::COMPLETED);
  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
}

class WizardControllerScreenPriorityOOBETest : public OobeBaseTest {
 protected:
  WizardControllerScreenPriorityOOBETest() = default;
  ~WizardControllerScreenPriorityOOBETest() override = default;

  void CheckCurrentScreen(OobeScreenId screen) {
    EXPECT_EQ(WizardController::default_controller()->GetScreen(screen),
              WizardController::default_controller()->current_screen());
  }
};

IN_PROC_BROWSER_TEST_F(WizardControllerScreenPriorityOOBETest,
                       DefaultPriorityTest) {
  ASSERT_TRUE(WizardController::default_controller() != nullptr);
  LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build = true;

  CheckCurrentScreen(WelcomeView::kScreenId);
  // Showing network screen should pass it has default priority which is same as
  // welcome screen.
  WizardController::default_controller()->AdvanceToScreen(
      NetworkScreenView::kScreenId);
  CheckCurrentScreen(NetworkScreenView::kScreenId);

  // Showing update screen should pass it has default priority which is same as
  // network screen.
  WizardController::default_controller()->AdvanceToScreen(
      UpdateView::kScreenId);
  CheckCurrentScreen(UpdateView::kScreenId);
}

class WizardControllerScreenPriorityTest : public LoginManagerTest,
                                           public LocalStateMixin::Delegate {
 protected:
  WizardControllerScreenPriorityTest() {
    login_manager_mixin_.AppendRegularUsers(1);
  }
  ~WizardControllerScreenPriorityTest() override = default;

  void CheckCurrentScreen(OobeScreenId screen) {
    EXPECT_EQ(WizardController::default_controller()->GetScreen(screen),
              WizardController::default_controller()->current_screen());
  }

  // LocalStateMixin::Delegate:
  void SetUpLocalState() override {
    // Set pref to show reset screen on startup.
    g_browser_process->local_state()->SetBoolean(prefs::kFactoryResetRequested,
                                                 true);
  }

 private:
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
  LocalStateMixin local_state_mixin_{&mixin_host_, this};
};

IN_PROC_BROWSER_TEST_F(WizardControllerScreenPriorityTest, CanNavigateToTest) {
  WizardController* const wizard_controller =
      WizardController::default_controller();
  ASSERT_TRUE(wizard_controller != nullptr);
  EXPECT_EQ(1, LoginScreenTestApi::GetUsersCount());

  // Check reset screen is visible on startup.
  OobeScreenWaiter(ResetView::kScreenId).Wait();
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());

  // Showing update required screen should fail due to lower priority than reset
  // screen.
  LoginDisplayHost::default_host()->StartWizard(UpdateRequiredView::kScreenId);
  CheckCurrentScreen(ResetView::kScreenId);
  // Wizard controller should not be recreated.
  EXPECT_EQ(wizard_controller, WizardController::default_controller());

  // Showing device disabled screen is allowed due to higher priority than reset
  // screen.
  LoginDisplayHost::default_host()->StartWizard(
      DeviceDisabledScreenView::kScreenId);
  CheckCurrentScreen(DeviceDisabledScreenView::kScreenId);
  // Wizard controller should not be recreated.
  EXPECT_EQ(wizard_controller, WizardController::default_controller());

  // Showing update required screen should fail due to lower priority than
  // device disabled screen.
  LoginDisplayHost::default_host()->StartWizard(UpdateRequiredView::kScreenId);
  CheckCurrentScreen(DeviceDisabledScreenView::kScreenId);
  EXPECT_EQ(wizard_controller, WizardController::default_controller());
}

class WizardControllerBrokenLocalStateTest : public WizardControllerTest {
 public:
  WizardControllerBrokenLocalStateTest(
      const WizardControllerBrokenLocalStateTest&) = delete;
  WizardControllerBrokenLocalStateTest& operator=(
      const WizardControllerBrokenLocalStateTest&) = delete;

 protected:
  WizardControllerBrokenLocalStateTest() = default;
  ~WizardControllerBrokenLocalStateTest() override = default;

  // WizardControllerTest:
  void SetUpInProcessBrowserTestFixture() override {
    WizardControllerTest::SetUpInProcessBrowserTestFixture();
    PrefServiceFactory factory;
    factory.set_user_prefs(base::MakeRefCounted<PrefStoreStub>());
    local_state_ = factory.Create(new PrefRegistrySimple());
    WizardController::set_local_state_for_testing(local_state_.get());
  }

 private:
  std::unique_ptr<PrefService> local_state_;
};

IN_PROC_BROWSER_TEST_F(WizardControllerBrokenLocalStateTest,
                       LocalStateCorrupted) {
  // Checks that after wizard controller initialization local error screen
  // is displayed.
  ASSERT_EQ(WizardController::default_controller()->GetScreen(
                LocalStateErrorScreenView::kScreenId),
            WizardController::default_controller()->current_screen());

  OobeScreenWaiter(LocalStateErrorScreenView::kScreenId).Wait();

  // Checks visibility of the powerwash button.
  test::OobeJS().ExpectVisiblePath({"local-state-error", "powerwashButton"});

  // Emulates user click on the "Restart and Powerwash" button.
  ASSERT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  test::OobeJS().TapOnPath({"local-state-error", "powerwashButton"});
  ASSERT_EQ(1, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
}

class WizardControllerProxyAuthOnSigninTest : public OobeBaseTest {
 public:
  WizardControllerProxyAuthOnSigninTest(
      const WizardControllerProxyAuthOnSigninTest&) = delete;
  WizardControllerProxyAuthOnSigninTest& operator=(
      const WizardControllerProxyAuthOnSigninTest&) = delete;

 protected:
  WizardControllerProxyAuthOnSigninTest()
      : proxy_server_(net::SpawnedTestServer::TYPE_BASIC_AUTH_PROXY,
                      base::FilePath()) {}
  ~WizardControllerProxyAuthOnSigninTest() override = default;

  // WizardControllerTest:
  void SetUp() override {
    ASSERT_TRUE(proxy_server_.Start());
    OobeBaseTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(::switches::kProxyServer,
                                    proxy_server_.host_port_pair().ToString());
  }

  net::SpawnedTestServer& proxy_server() { return proxy_server_; }

 private:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  net::SpawnedTestServer proxy_server_;
};

// TODO(crbug.com/1286218): Flakes on CrOS.
// TODO(crbug.com/41486698): Re-enable this test
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_ProxyAuthDialogOnSigninScreen \
  DISABLED_ProxyAuthDialogOnSigninScreen
#else
#define MAYBE_ProxyAuthDialogOnSigninScreen ProxyAuthDialogOnSigninScreen
#endif
IN_PROC_BROWSER_TEST_F(WizardControllerProxyAuthOnSigninTest,
                       MAYBE_ProxyAuthDialogOnSigninScreen) {
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
  ASSERT_TRUE(base::test::RunUntil(
      []() { return HttpAuthDialog::GetAllDialogsForTest().size() == 1; }));
}

class WizardControllerKioskFlowTest : public WizardControllerFlowTest {
 public:
  WizardControllerKioskFlowTest(const WizardControllerKioskFlowTest&) = delete;
  WizardControllerKioskFlowTest& operator=(
      const WizardControllerKioskFlowTest&) = delete;

 protected:
  WizardControllerKioskFlowTest() = default;

  // WizardControllerFlowTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WizardControllerFlowTest::SetUpCommandLine(command_line);
    base::FilePath test_data_dir;
    ASSERT_TRUE(chromeos::test_utils::GetTestDataPath(
        "app_mode", "kiosk_manifest", &test_data_dir));
    command_line->AppendSwitchPath(
        switches::kAppOemManifestFile,
        test_data_dir.AppendASCII("kiosk_manifest.json"));
  }
};

IN_PROC_BROWSER_TEST_F(WizardControllerKioskFlowTest,
                       ControlFlowKioskForcedEnrollment) {
  EXPECT_CALL(*mock_enrollment_screen_view_,
              SetEnrollmentConfig(EnrollmentModeMatches(
                  policy::EnrollmentConfig::MODE_LOCAL_FORCED)))
      .Times(1);
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_welcome_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_network_screen_, ShowImpl()).Times(1);
  mock_welcome_screen_->ExitScreen(WelcomeScreen::Result::kNext);

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_network_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_update_screen_, ShowImpl()).Times(1);

  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, ShowImpl()).Times(0);
  EXPECT_CALL(*mock_enrollment_screen_, ShowImpl()).Times(1);
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(EnrollmentScreenView::kScreenId);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, HideImpl()).Times(0);

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());

  // Make sure enterprise enrollment page shows up right after update screen.
  mock_enrollment_screen_->ExitScreen(EnrollmentScreen::Result::COMPLETED);

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
}

IN_PROC_BROWSER_TEST_F(WizardControllerKioskFlowTest,
                       ControlFlowEnrollmentBackForced) {
  EXPECT_CALL(*mock_enrollment_screen_view_,
              SetEnrollmentConfig(EnrollmentModeMatches(
                  policy::EnrollmentConfig::MODE_LOCAL_FORCED)))
      .Times(2);

  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_welcome_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_network_screen_, ShowImpl()).Times(1);
  mock_welcome_screen_->ExitScreen(WelcomeScreen::Result::kNext);

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_network_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_update_screen_, ShowImpl()).Times(1);

  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, ShowImpl()).Times(0);
  EXPECT_CALL(*mock_enrollment_screen_, ShowImpl()).Times(1);
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(EnrollmentScreenView::kScreenId);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, HideImpl()).Times(0);

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());

  // Make sure enterprise enrollment page shows up right after update screen.
  EXPECT_CALL(*mock_enrollment_screen_, ShowImpl()).Times(0);
  EXPECT_CALL(*mock_enrollment_screen_, HideImpl()).Times(0);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, ShowImpl()).Times(0);
  mock_enrollment_screen_->ExitScreen(
      EnrollmentScreen::Result::BACK_TO_AUTO_ENROLLMENT_CHECK);

  CheckCurrentScreen(EnrollmentScreenView::kScreenId);
  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
}

class WizardControllerEnableAdbSideloadingTest
    : public WizardControllerFlowTest {
 public:
  WizardControllerEnableAdbSideloadingTest(
      const WizardControllerEnableAdbSideloadingTest&) = delete;
  WizardControllerEnableAdbSideloadingTest& operator=(
      const WizardControllerEnableAdbSideloadingTest&) = delete;

 protected:
  WizardControllerEnableAdbSideloadingTest() = default;

  template <class T>
  void SkipToScreen(OobeScreenId screen, T* screen_mock) {
    EXPECT_CALL(*screen_mock, ShowImpl()).Times(1);
    auto* const wizard_controller = WizardController::default_controller();
    wizard_controller->AdvanceToScreen(screen);
  }
};

IN_PROC_BROWSER_TEST_F(WizardControllerEnableAdbSideloadingTest,
                       ShowAndEnableSideloading) {
  CheckCurrentScreen(WelcomeView::kScreenId);

  EXPECT_CALL(*mock_welcome_screen_, HideImpl()).Times(1);
  SkipToScreen(EnableAdbSideloadingScreenView::kScreenId,
               mock_enable_adb_sideloading_screen_.get());
  CheckCurrentScreen(EnableAdbSideloadingScreenView::kScreenId);

  EXPECT_CALL(*mock_enable_adb_sideloading_screen_,
              OnUserAction(ElementsAre("enable-pressed")))
      .Times(1);
  test::OobeJS().ClickOnPath(
      {"adb-sideloading", "enable-adb-sideloading-ok-button"});

  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(EnableAdbSideloadingScreenView::kScreenId);
  EXPECT_CALL(*mock_enable_adb_sideloading_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, ShowImpl()).Times(1);

  mock_enable_adb_sideloading_screen_->ExitScreen();

  // Let update screen smooth time process (time = 0ms).
  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(WelcomeView::kScreenId);
}

IN_PROC_BROWSER_TEST_F(WizardControllerEnableAdbSideloadingTest,
                       ShowAndDoNotEnableSideloading) {
  CheckCurrentScreen(WelcomeView::kScreenId);

  EXPECT_CALL(*mock_welcome_screen_, HideImpl()).Times(1);
  SkipToScreen(EnableAdbSideloadingScreenView::kScreenId,
               mock_enable_adb_sideloading_screen_.get());
  CheckCurrentScreen(EnableAdbSideloadingScreenView::kScreenId);

  EXPECT_CALL(*mock_enable_adb_sideloading_screen_,
              OnUserAction(ElementsAre("cancel-pressed")))
      .Times(1);
  test::OobeJS().ClickOnPath(
      {"adb-sideloading", "enable-adb-sideloading-cancel-button"});

  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*mock_enable_adb_sideloading_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, ShowImpl()).Times(1);

  mock_enable_adb_sideloading_screen_->ExitScreen();

  // Let update screen smooth time process (time = 0ms).
  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(WelcomeView::kScreenId);
}

class WizardControllerEnableDebuggingTest : public WizardControllerFlowTest {
 public:
  WizardControllerEnableDebuggingTest(
      const WizardControllerEnableDebuggingTest&) = delete;
  WizardControllerEnableDebuggingTest& operator=(
      const WizardControllerEnableDebuggingTest&) = delete;

 protected:
  WizardControllerEnableDebuggingTest() = default;

  // MixinBasedInProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WizardControllerFlowTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(chromeos::switches::kSystemDevMode);
  }
};

IN_PROC_BROWSER_TEST_F(WizardControllerEnableDebuggingTest,
                       ShowAndCancelEnableDebugging) {
  CheckCurrentScreen(WelcomeView::kScreenId);

  EXPECT_CALL(*mock_welcome_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_enable_debugging_screen_, ShowImpl()).Times(1);

  mock_welcome_screen_->ExitScreen(WelcomeScreen::Result::kEnableDebugging);

  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(EnableDebuggingScreenView::kScreenId);
  EXPECT_CALL(*mock_enable_debugging_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, ShowImpl()).Times(1);

  mock_enable_debugging_screen_->ExitScreen();

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(WelcomeView::kScreenId);
}

class WizardControllerDemoSetupTest : public WizardControllerFlowTest {
 public:
  WizardControllerDemoSetupTest(const WizardControllerDemoSetupTest&) = delete;
  WizardControllerDemoSetupTest& operator=(
      const WizardControllerDemoSetupTest&) = delete;

 protected:
  WizardControllerDemoSetupTest() = default;
  ~WizardControllerDemoSetupTest() override = default;

  // MixinBasedInProcessBrowserTest:
  void SetUpOnMainThread() override {
    WizardControllerFlowTest::SetUpOnMainThread();
    testing::Mock::VerifyAndClearExpectations(mock_welcome_screen_);
  }

  template <class T>
  void SkipToScreen(OobeScreenId screen, T* screen_mock) {
    EXPECT_CALL(*screen_mock, ShowImpl()).Times(1);
    auto* const wizard_controller = WizardController::default_controller();
    wizard_controller->SimulateDemoModeSetupForTesting();
    wizard_controller->AdvanceToScreen(screen);
  }
};

IN_PROC_BROWSER_TEST_F(WizardControllerDemoSetupTest,
                       OnlineDemoSetupFlowFinished) {
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_welcome_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_network_screen_, ShowImpl()).Times(1);

  WizardController::default_controller()->StartDemoModeSetup();

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_network_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_demo_preferences_screen_, ShowImpl()).Times(1);

  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  CheckCurrentScreen(DemoPreferencesScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
  EXPECT_CALL(*mock_demo_preferences_screen_, HideImpl()).Times(1);

  EXPECT_CALL(*mock_update_screen_, ShowImpl()).Times(1);
  mock_demo_preferences_screen_->ExitScreen(
      DemoPreferencesScreen::Result::COMPLETED);

  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_consolidated_consent_screen_, ShowImpl()).Times(1);
  EXPECT_CALL(*mock_update_screen_, HideImpl()).Times(1);
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  EXPECT_CALL(*mock_consolidated_consent_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, ShowImpl()).Times(0);
  EXPECT_CALL(*mock_demo_setup_screen_, ShowImpl()).Times(1);
  mock_consolidated_consent_screen_->ExitScreen(
      ConsolidatedConsentScreen::Result::ACCEPTED_DEMO_ONLINE);

  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_auto_enrollment_check_screen_, HideImpl()).Times(0);

  CheckCurrentScreen(DemoSetupScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  test::LockDemoDeviceInstallAttributes();
  mock_demo_setup_screen_->ExitScreen(DemoSetupScreen::Result::kCompleted);

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
  EXPECT_TRUE(ExistingUserController::current_controller());
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
}

IN_PROC_BROWSER_TEST_F(WizardControllerDemoSetupTest, DemoSetupCanceled) {
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_welcome_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_network_screen_, ShowImpl()).Times(1);

  WizardController::default_controller()->StartDemoModeSetup();

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_network_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_demo_preferences_screen_, ShowImpl()).Times(1);

  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  CheckCurrentScreen(DemoPreferencesScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_demo_preferences_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_update_screen_, ShowImpl()).Times(1);

  mock_demo_preferences_screen_->ExitScreen(
      DemoPreferencesScreen::Result::COMPLETED);

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_update_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_consolidated_consent_screen_, ShowImpl()).Times(1);
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(ConsolidatedConsentScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_consolidated_consent_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, ShowImpl()).Times(0);
  EXPECT_CALL(*mock_demo_setup_screen_, ShowImpl()).Times(1);

  mock_consolidated_consent_screen_->ExitScreen(
      ConsolidatedConsentScreen::Result::ACCEPTED_DEMO_ONLINE);

  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, HideImpl()).Times(0);

  CheckCurrentScreen(DemoSetupScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_demo_setup_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, ShowImpl()).Times(1);

  mock_demo_setup_screen_->ExitScreen(DemoSetupScreen::Result::kCanceled);

  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
}

IN_PROC_BROWSER_TEST_F(WizardControllerDemoSetupTest, DemoPreferencesCanceled) {
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
  SkipToScreen(DemoPreferencesScreenView::kScreenId,
               mock_demo_preferences_screen_.get());

  CheckCurrentScreen(DemoPreferencesScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_demo_preferences_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_network_screen_, ShowImpl()).Times(1);

  mock_demo_preferences_screen_->ExitScreen(
      DemoPreferencesScreen::Result::CANCELED);

  CheckCurrentScreen(NetworkScreenView::kScreenId);

  EXPECT_CALL(*mock_network_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, ShowImpl()).Times(1);

  mock_network_screen_->ExitScreen(NetworkScreen::Result::BACK);

  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
}

IN_PROC_BROWSER_TEST_F(WizardControllerDemoSetupTest, NetworkBackPressed) {
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
  SkipToScreen(NetworkScreenView::kScreenId, mock_network_screen_.get());

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_network_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, ShowImpl()).Times(1);

  mock_network_screen_->ExitScreen(NetworkScreen::Result::BACK);

  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
}

class WizardControllerDemoSetupDeviceDisabledTest
    : public WizardControllerDeviceStateTest {
 public:
  WizardControllerDemoSetupDeviceDisabledTest(
      const WizardControllerDemoSetupDeviceDisabledTest&) = delete;
  WizardControllerDemoSetupDeviceDisabledTest& operator=(
      const WizardControllerDemoSetupDeviceDisabledTest&) = delete;

 protected:
  WizardControllerDemoSetupDeviceDisabledTest() = default;
  ~WizardControllerDemoSetupDeviceDisabledTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WizardControllerDeviceStateTest::SetUpCommandLine(command_line);

    // Explicitly test legacy state determination flow.
    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableUnifiedStateDetermination,
        policy::AutoEnrollmentTypeChecker::kUnifiedStateDeterminationNever);
  }

  // MixinBasedInProcessBrowserTest:
  void SetUpOnMainThread() override {
    WizardControllerDeviceStateTest::SetUpOnMainThread();
    testing::Mock::VerifyAndClearExpectations(mock_welcome_screen_);
  }
};

IN_PROC_BROWSER_TEST_F(WizardControllerDemoSetupDeviceDisabledTest,
                       OnlineDemoSetup) {
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_welcome_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_network_screen_, ShowImpl()).Times(1);

  WizardController::default_controller()->StartDemoModeSetup();

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_network_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_demo_preferences_screen_, ShowImpl()).Times(1);

  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  CheckCurrentScreen(DemoPreferencesScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_demo_preferences_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_update_screen_, ShowImpl()).Times(1);

  mock_demo_preferences_screen_->ExitScreen(
      DemoPreferencesScreen::Result::COMPLETED);

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_consolidated_consent_screen_, ShowImpl()).Times(1);
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(ConsolidatedConsentScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_consolidated_consent_screen_, HideImpl()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, ShowImpl()).Times(1);

  mock_consolidated_consent_screen_->ExitScreen(
      ConsolidatedConsentScreen::Result::ACCEPTED_DEMO_ONLINE);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_auto_enrollment_check_screen_, HideImpl()).Times(1);

  mock_auto_enrollment_check_screen_->RealShow();

  // Wait for auto-enrollment controller to encounter the connection error.
  WaitForAutoEnrollmentState(kAutoEnrollmentConnectionError);

  // The error screen shows up if device state could not be retrieved.
  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  EXPECT_EQ(AutoEnrollmentCheckScreenView::kScreenId.AsId(),
            GetErrorScreen()->GetParentScreen());
  base::Value::Dict device_state;
  device_state.Set(policy::kDeviceStateMode,
                   base::Value(policy::kDeviceStateModeDisabled));
  device_state.Set(policy::kDeviceStateDisabledMessage,
                   base::Value(kDisabledMessage));
  g_browser_process->local_state()->SetDict(prefs::kServerBackedDeviceState,
                                            std::move(device_state));

  EXPECT_CALL(*device_disabled_screen_view_,
              Show(Field(&DeviceDisabledScreenView::Params::message,
                         Eq(kDisabledMessage))))
      .Times(1);
  mock_auto_enrollment_check_screen_->ExitScreen();

  base::RunLoop().RunUntilIdle();

  ResetAutoEnrollmentCheckScreen();
  CheckCurrentScreen(DeviceDisabledScreenView::kScreenId);

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
}

class WizardControllerOobeResumeTest : public WizardControllerTest {
 public:
  WizardControllerOobeResumeTest(const WizardControllerOobeResumeTest&) =
      delete;
  WizardControllerOobeResumeTest& operator=(
      const WizardControllerOobeResumeTest&) = delete;

 protected:
  WizardControllerOobeResumeTest() = default;
  // WizardControllerTest:
  void SetUpOnMainThread() override {
    WizardControllerTest::SetUpOnMainThread();

    // Make sure that OOBE is run as an "official" build.
    LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
        true;

    WizardController* wizard_controller =
        WizardController::default_controller();
    wizard_controller->SetCurrentScreen(nullptr);

    // Set up the mocks for all screens.
    mock_welcome_view_ = std::make_unique<MockWelcomeView>();
    mock_welcome_screen_ =
        MockScreenExpectLifecycle(std::make_unique<MockWelcomeScreen>(
            mock_welcome_view_->AsWeakPtr(),
            base::BindRepeating(&WizardController::OnWelcomeScreenExit,
                                base::Unretained(wizard_controller))));

    mock_enrollment_screen_view_ = std::make_unique<MockEnrollmentScreenView>();
    mock_enrollment_screen_ =
        MockScreenExpectLifecycle(std::make_unique<MockEnrollmentScreen>(
            mock_enrollment_screen_view_->AsWeakPtr(), GetErrorScreen(),
            base::BindRepeating(&WizardController::OnEnrollmentScreenExit,
                                base::Unretained(wizard_controller))));
  }

  std::unique_ptr<MockWelcomeView> mock_welcome_view_;
  raw_ptr<MockWelcomeScreen, DanglingUntriaged> mock_welcome_screen_;

  std::unique_ptr<MockEnrollmentScreenView> mock_enrollment_screen_view_;
  raw_ptr<MockEnrollmentScreen, DanglingUntriaged> mock_enrollment_screen_;

  std::unique_ptr<base::AutoReset<bool>> branded_build_override_;
};

IN_PROC_BROWSER_TEST_F(WizardControllerOobeResumeTest,
                       PRE_ControlFlowResumeInterruptedOobe) {
  // Switch to the initial screen.
  EXPECT_CALL(*mock_welcome_screen_, ShowImpl()).Times(1);
  WizardController::default_controller()->AdvanceToScreen(
      WelcomeView::kScreenId);
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_enrollment_screen_view_,
              SetEnrollmentConfig(
                  EnrollmentModeMatches(policy::EnrollmentConfig::MODE_MANUAL)))
      .Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, ShowImpl()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, HideImpl()).Times(1);

  WizardController::default_controller()->AdvanceToScreen(
      EnrollmentScreenView::kScreenId);
  CheckCurrentScreen(EnrollmentScreenView::kScreenId);
}

IN_PROC_BROWSER_TEST_F(WizardControllerOobeResumeTest,
                       ControlFlowResumeInterruptedOobe) {
  EXPECT_EQ(EnrollmentScreenView::kScreenId.AsId(),
            WizardController::default_controller()->first_screen_for_testing());
}

class WizardControllerOnboardingResumeTest : public WizardControllerTest {
 protected:
  void SetUpOnMainThread() override {
    cryptohome_mixin_.ApplyAuthConfigIfUserExists(
        user_, test::UserAuthConfig::Create(test::kDefaultAuthSetup));
    WizardControllerTest::SetUpOnMainThread();
  }

  DeviceStateMixin device_state_{
      &mixin_host_,
      DeviceStateMixin::State::OOBE_COMPLETED_PERMANENTLY_UNOWNED};
  FakeGaiaMixin gaia_mixin_{&mixin_host_};
  CryptohomeMixin cryptohome_mixin_{&mixin_host_};
  LoginManagerMixin login_mixin_{&mixin_host_, LoginManagerMixin::UserList(),
                                 &gaia_mixin_};
  AccountId user_{
      AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId)};
};

IN_PROC_BROWSER_TEST_F(WizardControllerOnboardingResumeTest,
                       PRE_ControlFlowResumeInterruptedOnboarding) {
  LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build = true;
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  LoginManagerMixin::TestUserInfo test_user(user_);
  login_mixin_.LoginWithDefaultContext(test_user);
  OobeScreenExitWaiter(UserCreationView::kScreenId).Wait();
  WizardController::default_controller()->AdvanceToScreen(
      MarketingOptInScreenView::kScreenId);
  OobeScreenWaiter(MarketingOptInScreenView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(WizardControllerOnboardingResumeTest,
                       ControlFlowResumeInterruptedOnboarding) {
  LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build = true;
  login_mixin_.LoginAsNewRegularUser();
  OobeScreenWaiter(MarketingOptInScreenView::kScreenId).Wait();
}

class WizardControllerCellularFirstTest : public WizardControllerFlowTest {
 public:
  WizardControllerCellularFirstTest(const WizardControllerCellularFirstTest&) =
      delete;
  WizardControllerCellularFirstTest& operator=(
      const WizardControllerCellularFirstTest&) = delete;

 protected:
  WizardControllerCellularFirstTest() = default;

  // WizardControllerFlowTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WizardControllerFlowTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kCellularFirst);
  }
};

IN_PROC_BROWSER_TEST_F(WizardControllerCellularFirstTest, CellularFirstFlow) {
  TestControlFlowMain();
}

class WizardControllerOobeConfigurationTest : public WizardControllerTest {
 public:
  WizardControllerOobeConfigurationTest(
      const WizardControllerOobeConfigurationTest&) = delete;
  WizardControllerOobeConfigurationTest& operator=(
      const WizardControllerOobeConfigurationTest&) = delete;

 protected:
  WizardControllerOobeConfigurationTest() = default;

  // WizardControllerTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WizardControllerTest::SetUpCommandLine(command_line);

    base::FilePath configuration_file;
    ASSERT_TRUE(chromeos::test_utils::GetTestDataPath(
        "oobe_configuration", "non_empty_configuration.json",
        &configuration_file));
    command_line->AppendSwitchPath(chromeos::switches::kFakeOobeConfiguration,
                                   configuration_file);
  }
};

IN_PROC_BROWSER_TEST_F(WizardControllerOobeConfigurationTest,
                       ConfigurationIsLoaded) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  WelcomeScreen* screen =
      WizardController::default_controller()->GetScreen<WelcomeScreen>();
  const base::Value::Dict& configuration = screen->GetConfigurationForTesting();
  EXPECT_FALSE(configuration.empty());
}

// Verifies that incomplete token-based enrollment flows (e.g. device is
// rebooted after enrollment fails) resume enrollment without issue. See
// b/336337134 for more details.
class WizardControllerEnrollmentTokenRebootTest
    : public WizardControllerTest,
      public LocalStateMixin::Delegate {
 public:
  WizardControllerEnrollmentTokenRebootTest(
      const WizardControllerEnrollmentTokenRebootTest&) = delete;
  WizardControllerEnrollmentTokenRebootTest operator=(
      const WizardControllerEnrollmentTokenRebootTest&) = delete;

 protected:
  WizardControllerEnrollmentTokenRebootTest() = default;
  // WizardControllerTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WizardControllerTest::SetUpCommandLine(command_line);

    command_line->AppendSwitch(ash::switches::kRevenBranding);
    base::FilePath configuration_file;
    ASSERT_TRUE(chromeos::test_utils::GetTestDataPath(
        "oobe_configuration", "enrollment_token_configuration.json",
        &configuration_file));
    command_line->AppendSwitchPath(chromeos::switches::kFakeOobeConfiguration,
                                   configuration_file);
  }

  void SetUpLocalState() override {
    // Simulate device having previously gone through state determination.
    base::Value::Dict device_state;
    device_state.Set(
        policy::kDeviceStateMode,
        base::Value(policy::kDeviceStateInitialModeTokenEnrollment));
    g_browser_process->local_state()->SetDict(prefs::kServerBackedDeviceState,
                                              std::move(device_state));
  }

 private:
  LocalStateMixin local_state_mixin_{&mixin_host_, this};
};

IN_PROC_BROWSER_TEST_F(WizardControllerEnrollmentTokenRebootTest,
                       ConfigurationIsLoaded) {
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  WelcomeScreen* screen =
      WizardController::default_controller()->GetScreen<WelcomeScreen>();
  const base::Value::Dict& configuration = screen->GetConfigurationForTesting();
  EXPECT_FALSE(configuration.empty());
}

class WizardControllerRemoteActivityNotificationTest
    : public WizardControllerTest,
      public LocalStateMixin::Delegate {
 public:
  WizardControllerRemoteActivityNotificationTest(
      const WizardControllerRemoteActivityNotificationTest&) = delete;
  WizardControllerRemoteActivityNotificationTest& operator=(
      const WizardControllerRemoteActivityNotificationTest&) = delete;

 protected:
  WizardControllerRemoteActivityNotificationTest() = default;
  ~WizardControllerRemoteActivityNotificationTest() override = default;

  // WizardControllerTest:
  void SetUpInProcessBrowserTestFixture() override {
    WizardControllerTest::SetUpInProcessBrowserTestFixture();
    feature_list_.InitAndEnableFeature(
        remoting::features::kEnableCrdAdminRemoteAccessV2);
    login_manager_mixin_.AppendRegularUsers(1);
  }

  void SetUpOnMainThread() override {
    WizardControllerTest::SetUpOnMainThread();
  }

  // LocalStateMixin::Delegate:
  void SetUpLocalState() override { StartupUtils::MarkOobeCompleted(); }

  void SetPref(const std::string& pref, bool value) {
    local_state()->SetBoolean(pref, value);
  }

  bool GetPref(const std::string& pref) {
    return local_state()->GetBoolean(pref);
  }

  PrefService* local_state() { return g_browser_process->local_state(); }

  bool IsCurrentScreen(OobeScreenId screen) {
    BaseScreen* current_screen =
        WizardController::default_controller()->current_screen();
    const std::string actual_screen =
        current_screen ? current_screen->screen_id().name : "nullptr";
    const std::string expected_screen = screen.name;
    return actual_screen == expected_screen;
  }

  bool IsRemoteActivityScreenVisible() {
    return LoginScreenTestApi::IsOobeDialogVisible() &&
           IsCurrentScreen(RemoteActivityNotificationView::kScreenId);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
  LocalStateMixin local_state_mixin_{&mixin_host_, this};
};

class RemoteActivityNotificationTestWhenPrefIsSet
    : public WizardControllerRemoteActivityNotificationTest {
  void SetUpLocalState() override {
    SetPref(prefs::kRemoteAdminWasPresent, true);
  }
};

class RemoteActivityNotificationTestWhenPrefIsNotSet
    : public WizardControllerRemoteActivityNotificationTest {
  void SetUpLocalState() override {
    SetPref(prefs::kRemoteAdminWasPresent, false);
  }
};

IN_PROC_BROWSER_TEST_F(RemoteActivityNotificationTestWhenPrefIsSet,
                       ShouldSetPrefOnRemoteActivityScreenExit) {
  OobeScreenWaiter(RemoteActivityNotificationView::kScreenId).Wait();
  CheckCurrentScreen(RemoteActivityNotificationView::kScreenId);
  ASSERT_TRUE(GetPref(prefs::kRemoteAdminWasPresent));
  ASSERT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());

  test::OobeJS().ClickOnPath({"remote-activity-notification", "cancelButton"});

  test::TestPredicateWaiter(base::BindRepeating([]() {
    return !LoginScreenTestApi::IsOobeDialogVisible();
  })).Wait();
  EXPECT_FALSE(IsRemoteActivityScreenVisible());
  EXPECT_FALSE(GetPref(prefs::kRemoteAdminWasPresent));
}

IN_PROC_BROWSER_TEST_F(RemoteActivityNotificationTestWhenPrefIsNotSet,
                       NotificationShouldNotBeVisible) {
  ASSERT_FALSE(GetPref(prefs::kRemoteAdminWasPresent));

  EXPECT_FALSE(IsRemoteActivityScreenVisible());
}

class RemoteActivityNotificationTestWhenNoLoginAccountPresentTest
    : public WizardControllerTest {
 public:
  // WizardControllerTest:
  void SetUpInProcessBrowserTestFixture() override {
    WizardControllerTest::SetUpInProcessBrowserTestFixture();
    feature_list_.InitAndEnableFeature(
        remoting::features::kEnableCrdAdminRemoteAccessV2);
  }

  void SetUpOnMainThread() override {
    WizardControllerTest::SetUpOnMainThread();
  }

  PrefService* local_state() { return g_browser_process->local_state(); }

 protected:
  base::test::ScopedFeatureList feature_list_;
  DeviceStateMixin device_state_{&mixin_host_,
                                 DeviceStateMixin::State::BEFORE_OOBE};
  FakeGaiaMixin gaia_mixin_{&mixin_host_};
  LoginManagerMixin login_mixin_{&mixin_host_, LoginManagerMixin::UserList(),
                                 &gaia_mixin_};
};

IN_PROC_BROWSER_TEST_F(
    RemoteActivityNotificationTestWhenNoLoginAccountPresentTest,
    PRE_ShouldGoToPreviousScreenWhenNotificationIsDismissed) {
  CheckCurrentScreen(UserCreationView::kScreenId);

  local_state()->SetBoolean(prefs::kRemoteAdminWasPresent, true);
}

IN_PROC_BROWSER_TEST_F(
    RemoteActivityNotificationTestWhenNoLoginAccountPresentTest,
    ShouldGoToPreviousScreenWhenNotificationIsDismissed) {
  OobeScreenWaiter(RemoteActivityNotificationView::kScreenId).Wait();
  CheckCurrentScreen(RemoteActivityNotificationView::kScreenId);

  test::OobeJS().ClickOnPath({"remote-activity-notification", "cancelButton"});

  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  CheckCurrentScreen(UserCreationView::kScreenId);
}

class WizardControllerThemeSelectionTest : public WizardControllerTest {
 protected:
  FakeGaiaMixin gaia_mixin_{&mixin_host_};
  LoginManagerMixin login_mixin_{&mixin_host_, LoginManagerMixin::UserList(),
                                 &gaia_mixin_};
};

IN_PROC_BROWSER_TEST_F(WizardControllerThemeSelectionTest,
                       TransitionToMarketingOptIn) {
  LoginDisplayHost::default_host()
      ->GetWizardContextForTesting()
      ->skip_choobe_for_tests = true;

  LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build = true;
  login_mixin_.LoginAsNewRegularUser();
  WizardController::default_controller()->AdvanceToScreen(
      ThemeSelectionScreenView::kScreenId);
  test::OobeJS().ClickOnPath({"theme-selection", "nextButton"});
  OobeScreenWaiter(MarketingOptInScreenView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(WizardControllerThemeSelectionTest,
                       TransitionToThemeSelection) {
  LoginDisplayHost::default_host()
      ->GetWizardContextForTesting()
      ->skip_choobe_for_tests = true;

  login_mixin_.LoginAsNewRegularUser();
  if (features::IsOobeDisplaySizeEnabled()) {
    WizardController::default_controller()->AdvanceToScreen(
        DisplaySizeScreenView::kScreenId);
    test::OobeJS().ClickOnPath({"display-size", "nextButton"});
  } else if (features::IsOobeTouchpadScrollEnabled()) {
    WizardController::default_controller()->AdvanceToScreen(
        TouchpadScrollScreenView::kScreenId);
    test::OobeJS().ClickOnPath({"touchpad-scroll", "nextButton"});
  } else {
    WizardController::default_controller()->AdvanceToScreen(
        GestureNavigationScreenView::kScreenId);
  }
  OobeScreenWaiter(ThemeSelectionScreenView::kScreenId).Wait();
}

class GaiaInfoTest : public WizardControllerTest {
 public:
  GaiaInfoTest() {
    feature_list_.InitAndEnableFeature(features::kOobeGaiaInfoScreen);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

class GaiaInfoScreenForEnterpriseEnrollmentTest : public GaiaInfoTest {
 private:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

IN_PROC_BROWSER_TEST_F(GaiaInfoScreenForEnterpriseEnrollmentTest,
                       SkippingGaiaInfo) {
  WizardController::default_controller()->AdvanceToScreen(
      GaiaInfoScreenView::kScreenId);
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(GaiaInfoTest, TransitionToGaiaInfo) {
  WaitForOobeUI();
  WizardController::default_controller()->AdvanceToScreen(
      UserCreationView::kScreenId);
  test::OobeJS().ClickOnPath({"user-creation", "selfButton"});
  test::OobeJS().ClickOnPath({"user-creation", "nextButton"});
  test::WaitForConsumerUpdateScreen();
  test::ExitConsumerUpdateScreenNoUpdate();
  OobeScreenWaiter(GaiaInfoScreenView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(GaiaInfoTest, TransitionFromGaiaInfo) {
  WaitForOobeUI();
  WizardController::default_controller()->AdvanceToScreen(
      GaiaInfoScreenView::kScreenId);
  test::OobeJS().ClickOnPath({"gaia-info", "nextButton"});
  OobeScreenWaiter(GaiaView::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(GaiaInfoTest, SkipGaiaInfoForChildAccount) {
  WaitForOobeUI();
  WizardController::default_controller()->AdvanceToScreen(
      UserCreationView::kScreenId);
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();

  test::OobeJS().ClickOnPath({"user-creation", "childButton"});
  test::OobeJS().ClickOnPath({"user-creation", "nextButton"});
  test::OobeJS().ClickOnPath({"user-creation", "childAccountButton"});
  test::OobeJS().ClickOnPath({"user-creation", "childSetupNextButton"});
  test::WaitForConsumerUpdateScreen();
  test::ExitConsumerUpdateScreenNoUpdate();
  OobeScreenWaiter(AddChildScreenView::kScreenId).Wait();
}

class WizardControllerGaiaTest : public GaiaInfoTest {
 protected:
  FakeGaiaMixin fake_gaia_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(WizardControllerGaiaTest, GoBackToGaiaInfo) {
  LoginDisplayHost::default_host()
      ->GetWizardContext()
      ->is_user_creation_enabled = true;
  WaitForOobeUI();
  WizardController::default_controller()->AdvanceToScreen(
      GaiaInfoScreenView::kScreenId);
  test::OobeJS().ClickOnPath({"gaia-info", "nextButton"});
  OobeScreenWaiter(GaiaView::kScreenId).Wait();

  test::OobeJS().ClickOnPath(
      {"gaia-signin", "signin-frame-dialog", "signin-back-button"});

  if (features::IsOobeSoftwareUpdateEnabled()) {
    OobeScreenWaiter(UserCreationView::kScreenId).Wait();
  } else {
    OobeScreenWaiter(GaiaInfoScreenView::kScreenId).Wait();
  }
}

IN_PROC_BROWSER_TEST_F(WizardControllerGaiaTest,
                       GoBackSkippingGaiaInfoInAddPersonFlow) {
  LoginDisplayHost::default_host()
      ->GetWizardContext()
      ->is_user_creation_enabled = true;
  LoginDisplayHost::default_host()->GetWizardContext()->is_add_person_flow =
      true;
  WaitForOobeUI();

  WizardController::default_controller()->AdvanceToScreen(
      GaiaInfoScreenView::kScreenId);
  OobeScreenWaiter(GaiaView::kScreenId).Wait();

  test::OobeJS().ClickOnPath(
      {"gaia-signin", "signin-frame-dialog", "signin-back-button"});
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();
}

class GoingBackFromGaiaScreenInChildFlowTest
    : public GaiaInfoTest,
      public testing::WithParamInterface<std::tuple<bool, std::string>> {
  FakeGaiaMixin fake_gaia_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_P(GoingBackFromGaiaScreenInChildFlowTest,
                       SkippingGaiaInfoScreen) {
  LoginDisplayHost::default_host()->GetWizardContext()->is_add_person_flow =
      std::get<0>(GetParam());

  WizardController::default_controller()->AdvanceToScreen(
      UserCreationView::kScreenId);
  OobeScreenWaiter(UserCreationView::kScreenId).Wait();

  test::OobeJS().ClickOnPath({"user-creation", "childButton"});
  test::OobeJS().ClickOnPath({"user-creation", "nextButton"});
  test::OobeJS().ClickOnPath({"user-creation", "childAccountButton"});
  test::OobeJS().ClickOnPath({"user-creation", "childSetupNextButton"});

  if (!LoginDisplayHost::default_host()
           ->GetWizardContext()
           ->is_add_person_flow) {
    test::WaitForConsumerUpdateScreen();
    test::ExitConsumerUpdateScreenNoUpdate();
  }

  OobeScreenWaiter(AddChildScreenView::kScreenId).Wait();
  test::OobeJS().ClickOnPath({"add-child", std::get<1>(GetParam())});
  test::OobeJS().ClickOnPath({"add-child", "childNextButton"});

  OobeScreenWaiter(GaiaView::kScreenId).Wait();

  test::OobeJS().ClickOnPath(
      {"gaia-signin", "signin-frame-dialog", "signin-back-button"});

  OobeScreenWaiter(AddChildScreenView::kScreenId).Wait();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GoingBackFromGaiaScreenInChildFlowTest,
    testing::Values(std::make_tuple(true, "childCreateButton"),
                    std::make_tuple(true, "childSignInButton"),
                    std::make_tuple(false, "childCreateButton"),
                    std::make_tuple(false, "childSignInButton")));
// TODO(nkostylev): Add test for WebUI accelerators http://crosbug.com/22571

// TODO(merkulova): Add tests for bluetooth HID detection screen variations when
// UI and logic is ready. http://crbug.com/127016

// TODO(khmel): Add tests for ARC OptIn flow.
// http://crbug.com/651144

// TODO(fukino): Add tests for encryption migration UI.
// http://crbug.com/706017

// TODO(alemate): Add tests for Sync Consent UI.

// TODO(rsgingerrs): Add tests for Recommend Apps UI.

// TODO(alemate): Add tests for Marketing Opt-In.

// TODO(khorimoto): Add tests for MultiDevice Setup UI.

}  // namespace ash
