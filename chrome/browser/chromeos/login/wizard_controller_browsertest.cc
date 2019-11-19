// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wizard_controller.h"

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/base/locale_util.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_controller.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper.h"
#include "chrome/browser/chromeos/login/enrollment/mock_auto_enrollment_check_screen.h"
#include "chrome/browser/chromeos/login/enrollment/mock_enrollment_screen.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/device_disabled_screen.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/screens/hid_detection_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_arc_terms_of_service_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_demo_preferences_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_demo_setup_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_device_disabled_screen_view.h"
#include "chrome/browser/chromeos/login/screens/mock_enable_adb_sideloading_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_enable_debugging_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_eula_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_network_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_update_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_welcome_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_wrong_hwid_screen.h"
#include "chrome/browser/chromeos/login/screens/reset_screen.h"
#include "chrome/browser/chromeos/login/screens/welcome_screen.h"
#include "chrome/browser/chromeos/login/screens/wrong_hwid_screen.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/oobe_configuration_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"
#include "chrome/browser/chromeos/policy/auto_enrollment_client.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "chrome/browser/chromeos/policy/fake_auto_enrollment_client.h"
#include "chrome/browser/chromeos/policy/server_backed_device_state.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/dbus/shill/fake_shill_manager_client.h"
#include "chromeos/dbus/system_clock/system_clock_client.h"
#include "chromeos/geolocation/simple_geolocation_provider.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/settings/timezone_settings.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/system/statistics_provider.h"
#include "chromeos/test/chromeos_test_utils.h"
#include "chromeos/timezone/timezone_request.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/testing_pref_store.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Exactly;
using ::testing::Invoke;
using ::testing::IsNull;
using ::testing::Mock;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Return;

namespace chromeos {

namespace {

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

// Matches on the mode parameter of an EnrollmentConfig object.
MATCHER_P(EnrollmentModeMatches, mode, "") {
  return arg.mode == mode;
}

class PrefStoreStub : public TestingPrefStore {
 public:
  // TestingPrefStore overrides:
  PrefReadError GetReadError() const override {
    return PersistentPrefStore::PREF_READ_ERROR_JSON_PARSE;
  }

  bool IsInitializationComplete() const override { return true; }

 private:
  ~PrefStoreStub() override {}
};

// Matches on non-empty DictionaryValue* object.
MATCHER(NonEmptyConfiguration, "") {
  return arg && !arg->DictEmpty();
}

// Used to set up a |FakeAutoEnrollmentClientFactory| for the duration of a
// test.
class ScopedFakeAutoEnrollmentClientFactory {
 public:
  explicit ScopedFakeAutoEnrollmentClientFactory(
      AutoEnrollmentController* controller)
      : controller_(controller),
        fake_auto_enrollment_client_factory_(
            base::BindRepeating(&ScopedFakeAutoEnrollmentClientFactory::
                                    OnFakeAutoEnrollmentClientCreated,
                                base::Unretained(this))) {
    controller_->SetAutoEnrollmentClientFactoryForTesting(
        &fake_auto_enrollment_client_factory_);
  }

  ~ScopedFakeAutoEnrollmentClientFactory() {
    controller_->SetAutoEnrollmentClientFactoryForTesting(nullptr);
  }

  // Waits until the |AutoEnrollmentController| has requested the creation of an
  // |AutoEnrollmentClient|. Returns the created |AutoEnrollmentClient|. If an
  // |AutoEnrollmentClient| has already been created, returns immediately.
  // Note: The returned instance is owned by |AutoEnrollmentController|.
  policy::FakeAutoEnrollmentClient* WaitAutoEnrollmentClientCreated() {
    if (created_auto_enrollment_client_)
      return created_auto_enrollment_client_;

    base::RunLoop run_loop;
    run_on_auto_enrollment_client_created_ = run_loop.QuitClosure();
    run_loop.Run();

    return created_auto_enrollment_client_;
  }

  // Resets the cached |AutoEnrollmentClient|, so another |AutoEnrollmentClient|
  // may be created through this factory.
  void Reset() { created_auto_enrollment_client_ = nullptr; }

 private:
  // Called when |fake_auto_enrollment_client_factory_| was asked to create an
  // |AutoEnrollmentClient|.
  void OnFakeAutoEnrollmentClientCreated(
      policy::FakeAutoEnrollmentClient* auto_enrollment_client) {
    // Only allow an AutoEnrollmentClient to be created when the test expects
    // it. The test should call |Reset| to expect a new |AutoEnrollmentClient|
    // to be created.
    EXPECT_FALSE(created_auto_enrollment_client_);
    created_auto_enrollment_client_ = auto_enrollment_client;

    if (run_on_auto_enrollment_client_created_)
      std::move(run_on_auto_enrollment_client_created_).Run();
  }

  // The |AutoEnrollmentController| which is using
  // |fake_auto_enrollment_client_factory_|.
  AutoEnrollmentController* controller_;
  policy::FakeAutoEnrollmentClient::FactoryImpl
      fake_auto_enrollment_client_factory_;

  policy::FakeAutoEnrollmentClient* created_auto_enrollment_client_ = nullptr;
  base::OnceClosure run_on_auto_enrollment_client_created_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFakeAutoEnrollmentClientFactory);
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
      base::Bind(&OnLocaleSwitched, base::Unretained(&data)));
  locale_util::SwitchLanguage(locale, true, false, callback,
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

  // Audio output is at |volume_percent| and |mute_on|. Spoken feedback
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
  if (expected_state == actual_state)
    loop->Quit();
}

// Returns a string which can be put into the |kRlzEmbargoEndDateKey| VPD
// variable. If |days_offset| is 0, the return value represents the current day.
// If |days_offset| is positive, the return value represents |days_offset| days
// in the future. If |days_offset| is negative, the return value represents
// |days_offset| days in the past.
std::string GenerateEmbargoEndDate(int days_offset) {
  base::Time::Exploded exploded;
  base::Time target_time =
      base::Time::Now() + base::TimeDelta::FromDays(days_offset);
  target_time.UTCExplode(&exploded);

  std::string rlz_embargo_end_date_string = base::StringPrintf(
      "%04d-%02d-%02d", exploded.year, exploded.month, exploded.day_of_month);

  // Sanity check that base::Time::FromUTCString can read back the format used
  // here.
  base::Time reparsed_time;
  EXPECT_TRUE(base::Time::FromUTCString(rlz_embargo_end_date_string.c_str(),
                                        &reparsed_time));
  EXPECT_EQ(target_time.ToDeltaSinceWindowsEpoch().InMicroseconds() /
                base::Time::kMicrosecondsPerDay,
            reparsed_time.ToDeltaSinceWindowsEpoch().InMicroseconds() /
                base::Time::kMicrosecondsPerDay);

  return rlz_embargo_end_date_string;
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
  EXPECT_CALL(*mock0, Show()).Times(0);
  EXPECT_CALL(*mock0, Hide()).Times(0);
  return mock0;
}

}  // namespace

class WizardControllerTest : public MixinBasedInProcessBrowserTest {
 protected:
  WizardControllerTest() = default;
  ~WizardControllerTest() override = default;

  // MixinBasedInProcessBrowserTest:
  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    AccessibilityManager::Get()->SetProfileForTest(
        ProfileHelper::GetSigninProfile());
    ShowLoginWizard(OobeScreen::SCREEN_TEST_NO_WINDOW);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kLoginManager);
  }

  ErrorScreen* GetErrorScreen() { return GetOobeUI()->GetErrorScreen(); }

  OobeUI* GetOobeUI() { return LoginDisplayHost::default_host()->GetOobeUI(); }

  content::WebContents* GetWebContents() {
    LoginDisplayHost* host = LoginDisplayHost::default_host();
    if (!host)
      return nullptr;
    return host->GetOobeWebContents();
  }

  void WaitUntilJSIsReady() {
    LoginDisplayHost* host = LoginDisplayHost::default_host();
    if (!host)
      return;
    chromeos::OobeUI* oobe_ui = host->GetOobeUI();
    if (!oobe_ui)
      return;
    base::RunLoop run_loop;
    const bool oobe_ui_ready = oobe_ui->IsJSReady(run_loop.QuitClosure());
    if (!oobe_ui_ready)
      run_loop.Run();
  }

  bool JSExecute(const std::string& script) {
    return content::ExecuteScript(GetWebContents(), script);
  }

  bool JSExecuteBooleanExpression(const std::string& expression) {
    bool result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        GetWebContents(),
        "window.domAutomationController.send(!!(" + expression + "));",
        &result));
    return result;
  }

  std::string JSExecuteStringExpression(const std::string& expression) {
    std::string result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        GetWebContents(),
        "window.domAutomationController.send(" + expression + ");", &result));
    return result;
  }

  void CheckCurrentScreen(OobeScreenId screen) {
    EXPECT_EQ(WizardController::default_controller()->GetScreen(screen),
              WizardController::default_controller()->current_screen());
  }

  WrongHWIDScreen* GetWrongHWIDScreen() {
    return static_cast<WrongHWIDScreen*>(
        WizardController::default_controller()->GetScreen(
            WrongHWIDScreenView::kScreenId));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerTest, SwitchLanguage) {
  ASSERT_TRUE(WizardController::default_controller() != NULL);
  WizardController::default_controller()->AdvanceToScreen(
      WelcomeView::kScreenId);

  // Checking the default locale. Provided that the profile is cleared in SetUp.
  EXPECT_EQ("en-US", g_browser_process->GetApplicationLocale());
  EXPECT_STREQ("en", icu::Locale::getDefault().getLanguage());
  EXPECT_FALSE(base::i18n::IsRTL());
  const base::string16 en_str =
      l10n_util::GetStringUTF16(IDS_NETWORK_SELECTION_TITLE);

  RunSwitchLanguageTest("fr", "fr", true);
  EXPECT_EQ("fr", g_browser_process->GetApplicationLocale());
  EXPECT_STREQ("fr", icu::Locale::getDefault().getLanguage());
  EXPECT_FALSE(base::i18n::IsRTL());
  const base::string16 fr_str =
      l10n_util::GetStringUTF16(IDS_NETWORK_SELECTION_TITLE);

  EXPECT_NE(en_str, fr_str);

  RunSwitchLanguageTest("ar", "ar", true);
  EXPECT_EQ("ar", g_browser_process->GetApplicationLocale());
  EXPECT_STREQ("ar", icu::Locale::getDefault().getLanguage());
  EXPECT_TRUE(base::i18n::IsRTL());
  const base::string16 ar_str =
      l10n_util::GetStringUTF16(IDS_NETWORK_SELECTION_TITLE);

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

class TimeZoneTestRunner {
 public:
  void OnResolved() { loop_.Quit(); }
  void Run() { loop_.Run(); }

 private:
  base::RunLoop loop_;
};

class WizardControllerFlowTest : public WizardControllerTest {
 protected:
  WizardControllerFlowTest() {}
  // WizardControllerTest:
  void SetUpOnMainThread() override {
    WizardControllerTest::SetUpOnMainThread();

    // Make sure that OOBE is run as an "official" build.
    branded_build_override_ = WizardController::ForceBrandedBuildForTesting();

    WizardController* wizard_controller =
        WizardController::default_controller();
    wizard_controller->SetSharedURLLoaderFactoryForTesting(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));

    // Clear portal list (as it is by default in OOBE).
    NetworkHandler::Get()->network_state_handler()->SetCheckPortalList("");

    // Set up the mocks for all screens.
    mock_welcome_screen_ =
        MockScreenExpectLifecycle(std::make_unique<MockWelcomeScreen>(
            GetOobeUI()->GetView<WelcomeScreenHandler>(),
            base::BindRepeating(&WizardController::OnWelcomeScreenExit,
                                base::Unretained(wizard_controller))));

    mock_demo_preferences_screen_view_ =
        std::make_unique<MockDemoPreferencesScreenView>();
    mock_demo_preferences_screen_ =
        MockScreenExpectLifecycle(std::make_unique<MockDemoPreferencesScreen>(
            mock_demo_preferences_screen_view_.get(),
            base::BindRepeating(&WizardController::OnDemoPreferencesScreenExit,
                                base::Unretained(wizard_controller))));

    mock_arc_terms_of_service_screen_view_ =
        std::make_unique<MockArcTermsOfServiceScreenView>();
    mock_arc_terms_of_service_screen_ =
        MockScreenExpectLifecycle(std::make_unique<MockArcTermsOfServiceScreen>(
            mock_arc_terms_of_service_screen_view_.get(),
            base::BindRepeating(
                &WizardController::OnArcTermsOfServiceScreenExit,
                base::Unretained(wizard_controller))));

    device_disabled_screen_view_ =
        std::make_unique<MockDeviceDisabledScreenView>();
    MockScreen(std::make_unique<DeviceDisabledScreen>(
        device_disabled_screen_view_.get()));
    EXPECT_CALL(*device_disabled_screen_view_, Show()).Times(0);

    mock_network_screen_view_ = std::make_unique<MockNetworkScreenView>();
    mock_network_screen_ =
        MockScreenExpectLifecycle(std::make_unique<MockNetworkScreen>(
            mock_network_screen_view_.get(),
            base::BindRepeating(&WizardController::OnNetworkScreenExit,
                                base::Unretained(wizard_controller))));

    mock_update_view_ = std::make_unique<MockUpdateView>();
    mock_update_screen_ =
        MockScreenExpectLifecycle(std::make_unique<MockUpdateScreen>(
            mock_update_view_.get(), GetErrorScreen(),
            base::BindRepeating(&WizardController::OnUpdateScreenExit,
                                base::Unretained(wizard_controller))));

    mock_eula_view_ = std::make_unique<MockEulaView>();
    mock_eula_screen_ =
        MockScreenExpectLifecycle(std::make_unique<MockEulaScreen>(
            mock_eula_view_.get(),
            base::BindRepeating(&WizardController::OnEulaScreenExit,
                                base::Unretained(wizard_controller))));

    mock_enrollment_screen_view_ = std::make_unique<MockEnrollmentScreenView>();
    mock_enrollment_screen_ =
        MockScreenExpectLifecycle(std::make_unique<MockEnrollmentScreen>(
            mock_enrollment_screen_view_.get(),
            base::BindRepeating(&WizardController::OnEnrollmentScreenExit,
                                base::Unretained(wizard_controller))));

    mock_auto_enrollment_check_screen_view_ =
        std::make_unique<MockAutoEnrollmentCheckScreenView>();
    ExpectSetDelegate(mock_auto_enrollment_check_screen_view_.get());
    mock_auto_enrollment_check_screen_ = MockScreenExpectLifecycle(
        std::make_unique<MockAutoEnrollmentCheckScreen>(
            mock_auto_enrollment_check_screen_view_.get(), GetErrorScreen(),
            base::BindRepeating(
                &WizardController::OnAutoEnrollmentCheckScreenExit,
                base::Unretained(wizard_controller))));

    mock_wrong_hwid_screen_view_ = std::make_unique<MockWrongHWIDScreenView>();
    ExpectSetDelegate(mock_wrong_hwid_screen_view_.get());
    mock_wrong_hwid_screen_ =
        MockScreenExpectLifecycle(std::make_unique<MockWrongHWIDScreen>(
            mock_wrong_hwid_screen_view_.get(),
            base::BindRepeating(&WizardController::OnWrongHWIDScreenExit,
                                base::Unretained(wizard_controller))));

    mock_enable_adb_sideloading_screen_view_ =
        std::make_unique<MockEnableAdbSideloadingScreenView>();
    ExpectBindUnbind(mock_enable_adb_sideloading_screen_view_.get());
    mock_enable_adb_sideloading_screen_ = MockScreenExpectLifecycle(
        std::make_unique<MockEnableAdbSideloadingScreen>(
            mock_enable_adb_sideloading_screen_view_.get(),
            base::BindRepeating(
                &WizardController::OnEnableAdbSideloadingScreenExit,
                base::Unretained(wizard_controller))));

    mock_enable_debugging_screen_view_ =
        std::make_unique<MockEnableDebuggingScreenView>();
    ExpectSetDelegate(mock_enable_debugging_screen_view_.get());
    mock_enable_debugging_screen_ =
        MockScreenExpectLifecycle(std::make_unique<MockEnableDebuggingScreen>(
            mock_enable_debugging_screen_view_.get(),
            base::BindRepeating(&WizardController::OnEnableDebuggingScreenExit,
                                base::Unretained(wizard_controller))));

    mock_demo_setup_screen_view_ = std::make_unique<MockDemoSetupScreenView>();
    ExpectBind(mock_demo_setup_screen_view_.get());
    mock_demo_setup_screen_ =
        MockScreenExpectLifecycle(std::make_unique<MockDemoSetupScreen>(
            mock_demo_setup_screen_view_.get(),
            base::BindRepeating(&WizardController::OnDemoSetupScreenExit,
                                base::Unretained(wizard_controller))));

    mock_demo_preferences_screen_view_ =
        std::make_unique<MockDemoPreferencesScreenView>();
    ExpectBind(mock_demo_preferences_screen_view_.get());
    mock_demo_preferences_screen_ =
        MockScreenExpectLifecycle(std::make_unique<MockDemoPreferencesScreen>(
            mock_demo_preferences_screen_view_.get(),
            base::BindRepeating(&WizardController::OnDemoPreferencesScreenExit,
                                base::Unretained(wizard_controller))));

    mock_arc_terms_of_service_screen_view_ =
        std::make_unique<MockArcTermsOfServiceScreenView>();
    mock_arc_terms_of_service_screen_ =
        MockScreenExpectLifecycle(std::make_unique<MockArcTermsOfServiceScreen>(
            mock_arc_terms_of_service_screen_view_.get(),
            base::BindRepeating(
                &WizardController::OnArcTermsOfServiceScreenExit,
                base::Unretained(wizard_controller))));

    // Switch to the initial screen.
    EXPECT_EQ(NULL, wizard_controller->current_screen());
    EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(NotNull())).Times(1);
    EXPECT_CALL(*mock_welcome_screen_, Show()).Times(1);
    wizard_controller->AdvanceToScreen(WelcomeView::kScreenId);
  }

  void TearDownOnMainThread() override {
    mock_welcome_screen_ = nullptr;
    device_disabled_screen_view_.reset();
    test_url_loader_factory_.ClearResponses();
    WizardControllerTest::TearDownOnMainThread();
  }

  void InitTimezoneResolver() {
    network_portal_detector_ = new NetworkPortalDetectorTestImpl();
    network_portal_detector::InitializeForTesting(network_portal_detector_);

    NetworkPortalDetector::CaptivePortalState online_state;
    online_state.status = NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;
    online_state.response_code = 204;

    // Default detworks happens to be usually "eth1" in tests.
    const NetworkState* default_network =
        NetworkHandler::Get()->network_state_handler()->DefaultNetwork();

    network_portal_detector_->SetDefaultNetworkForTesting(
        default_network->guid());
    network_portal_detector_->SetDetectionResultsForTesting(
        default_network->guid(), online_state);
  }

  chromeos::SimpleGeolocationProvider* GetGeolocationProvider() {
    return WizardController::default_controller()->geolocation_provider_.get();
  }

  void WaitUntilTimezoneResolved() {
    auto runner = std::make_unique<TimeZoneTestRunner>();
    if (!WizardController::default_controller()
             ->SetOnTimeZoneResolvedForTesting(
                 base::Bind(&TimeZoneTestRunner::OnResolved,
                            base::Unretained(runner.get())))) {
      return;
    }

    runner->Run();
  }

  void ResetAutoEnrollmentCheckScreen() {
    WizardController::default_controller()
        ->screen_manager()
        ->DeleteScreenForTesting(AutoEnrollmentCheckScreenView::kScreenId);
  }

  void TestControlFlowMain() {
    CheckCurrentScreen(WelcomeView::kScreenId);

    WaitUntilJSIsReady();

    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          if (base::StartsWith(
                  request.url.spec(),
                  SimpleGeolocationProvider::DefaultGeolocationProviderURL()
                      .spec(),
                  base::CompareCase::SENSITIVE)) {
            test_url_loader_factory_.AddResponse(request.url.spec(),
                                                 kGeolocationResponseBody);
          } else if (base::StartsWith(
                         request.url.spec(),
                         chromeos::DefaultTimezoneProviderURL().spec(),
                         base::CompareCase::SENSITIVE)) {
            test_url_loader_factory_.AddResponse(request.url.spec(),
                                                 kTimezoneResponseBody);
          }
        }));

    ASSERT_TRUE(ash::LoginScreenTestApi::IsLoginShelfShown());

    EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
    EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull())).Times(1);
    EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
    EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
    mock_welcome_screen_->ExitScreen();

    CheckCurrentScreen(NetworkScreenView::kScreenId);
    EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
    mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

    CheckCurrentScreen(EulaView::kScreenId);
    // Login shelf should still be visible.
    EXPECT_TRUE(ash::LoginScreenTestApi::IsLoginShelfShown());

    EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
    EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
    // Enable TimeZone resolve
    InitTimezoneResolver();
    mock_eula_screen_->ExitScreen(
        EulaScreen::Result::ACCEPTED_WITHOUT_USAGE_STATS_REPORTING);
    EXPECT_TRUE(GetGeolocationProvider());

    // Let update screen smooth time process (time = 0ms).
    content::RunAllPendingInMessageLoop();

    CheckCurrentScreen(UpdateView::kScreenId);
    EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
    EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
    mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

    CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
    EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(0);
    EXPECT_CALL(*mock_eula_screen_, Show()).Times(0);
    mock_auto_enrollment_check_screen_->ExitScreen();

    EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
    EXPECT_EQ("ethernet,wifi,cellular", NetworkHandler::Get()
                                            ->network_state_handler()
                                            ->GetCheckPortalListForTest());

    WaitUntilTimezoneResolved();
    EXPECT_EQ(
        "America/Anchorage",
        base::UTF16ToUTF8(chromeos::system::TimezoneSettings::GetInstance()
                              ->GetCurrentTimezoneID()));
  }

  // All of the *Screen types are owned by WizardController. The views are owned
  // by this test class.
  MockWelcomeScreen* mock_welcome_screen_ = nullptr;

  MockNetworkScreen* mock_network_screen_ = nullptr;
  std::unique_ptr<MockNetworkScreenView> mock_network_screen_view_;

  MockUpdateScreen* mock_update_screen_ = nullptr;
  std::unique_ptr<MockUpdateView> mock_update_view_;

  MockEulaScreen* mock_eula_screen_ = nullptr;
  std::unique_ptr<MockEulaView> mock_eula_view_;

  MockEnrollmentScreen* mock_enrollment_screen_ = nullptr;
  std::unique_ptr<MockEnrollmentScreenView> mock_enrollment_screen_view_;

  MockAutoEnrollmentCheckScreen* mock_auto_enrollment_check_screen_ = nullptr;
  std::unique_ptr<MockAutoEnrollmentCheckScreenView>
      mock_auto_enrollment_check_screen_view_;

  MockWrongHWIDScreen* mock_wrong_hwid_screen_ = nullptr;
  std::unique_ptr<MockWrongHWIDScreenView> mock_wrong_hwid_screen_view_;

  MockEnableAdbSideloadingScreen* mock_enable_adb_sideloading_screen_ = nullptr;
  std::unique_ptr<MockEnableAdbSideloadingScreenView>
      mock_enable_adb_sideloading_screen_view_;

  MockEnableDebuggingScreen* mock_enable_debugging_screen_ = nullptr;
  std::unique_ptr<MockEnableDebuggingScreenView>
      mock_enable_debugging_screen_view_;

  MockDemoSetupScreen* mock_demo_setup_screen_ = nullptr;
  std::unique_ptr<MockDemoSetupScreenView> mock_demo_setup_screen_view_;

  MockDemoPreferencesScreen* mock_demo_preferences_screen_ = nullptr;
  std::unique_ptr<MockDemoPreferencesScreenView>
      mock_demo_preferences_screen_view_;

  MockArcTermsOfServiceScreen* mock_arc_terms_of_service_screen_ = nullptr;
  std::unique_ptr<MockArcTermsOfServiceScreenView>
      mock_arc_terms_of_service_screen_view_;

  std::unique_ptr<MockDeviceDisabledScreenView> device_disabled_screen_view_;

 private:
  NetworkPortalDetectorTestImpl* network_portal_detector_ = nullptr;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<base::AutoReset<bool>> branded_build_override_;

  DISALLOW_COPY_AND_ASSIGN(WizardControllerFlowTest);
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
  EXPECT_CALL(*mock_update_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull())).Times(1);
  mock_welcome_screen_->ExitScreen();

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  CheckCurrentScreen(EulaView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  mock_eula_screen_->ExitScreen(
      EulaScreen::Result::ACCEPTED_WITHOUT_USAGE_STATS_REPORTING);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(0);
  mock_auto_enrollment_check_screen_->ExitScreen();

  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
}

// This test verifies that if WizardController fails to apply a critical update
// before the OOBE is marked complete, it goes back the network selection
// screen and thus prevents the user from proceeding to log in.
IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest,
                       ControlFlowErrorUpdateCriticalUpdate) {
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull())).Times(1);
  mock_welcome_screen_->ExitScreen();

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  CheckCurrentScreen(EulaView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  mock_eula_screen_->ExitScreen(
      EulaScreen::Result::ACCEPTED_WITHOUT_USAGE_STATS_REPORTING);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(0);  // last transition
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_ERROR);
  CheckCurrentScreen(NetworkScreenView::kScreenId);
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest, ControlFlowSkipUpdateEnroll) {
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull())).Times(1);
  mock_welcome_screen_->ExitScreen();

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  CheckCurrentScreen(EulaView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(0);
  WizardController::default_controller()->SkipUpdateEnrollAfterEula();
  EXPECT_CALL(*mock_enrollment_screen_view_,
              SetEnrollmentConfig(
                  mock_enrollment_screen_,
                  EnrollmentModeMatches(policy::EnrollmentConfig::MODE_MANUAL)))
      .Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  mock_eula_screen_->ExitScreen(
      EulaScreen::Result::ACCEPTED_WITHOUT_USAGE_STATS_REPORTING);
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Hide()).Times(0);
  mock_auto_enrollment_check_screen_->ExitScreen();
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(EnrollmentScreenView::kScreenId);
  EXPECT_EQ("ethernet,wifi,cellular", NetworkHandler::Get()
                                          ->network_state_handler()
                                          ->GetCheckPortalListForTest());
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest, ControlFlowEulaDeclined) {
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull())).Times(1);
  mock_welcome_screen_->ExitScreen();

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(0);
  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  CheckCurrentScreen(EulaView::kScreenId);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  mock_eula_screen_->ExitScreen(EulaScreen::Result::BACK);

  CheckCurrentScreen(NetworkScreenView::kScreenId);
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest,
                       ControlFlowEnrollmentCompleted) {
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_enrollment_screen_view_,
              SetEnrollmentConfig(
                  mock_enrollment_screen_,
                  EnrollmentModeMatches(policy::EnrollmentConfig::MODE_MANUAL)))
      .Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull())).Times(1);

  WizardController::default_controller()->AdvanceToScreen(
      EnrollmentScreenView::kScreenId);
  CheckCurrentScreen(EnrollmentScreenView::kScreenId);
  mock_enrollment_screen_->ExitScreen(EnrollmentScreen::Result::COMPLETED);

  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest,
                       ControlFlowWrongHWIDScreenFromLogin) {
  CheckCurrentScreen(WelcomeView::kScreenId);

  LoginDisplayHost::default_host()->StartSignInScreen(LoginScreenContext());
  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
  ExistingUserController::current_controller()->ShowWrongHWIDScreen();

  CheckCurrentScreen(WrongHWIDScreenView::kScreenId);

  // After warning is skipped, user returns to sign-in screen.
  // And this destroys WizardController.
  GetWrongHWIDScreen()->OnExit();
  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
}

// This parameterized test class extends WizardControllerFlowTest to verify how
// WizardController behaves if it does not find or fails to apply an update
// after the OOBE is marked complete.
class WizardControllerUpdateAfterCompletedOobeTest
    : public WizardControllerFlowTest,
      public testing::WithParamInterface<UpdateScreen::Result> {
 protected:
  WizardControllerUpdateAfterCompletedOobeTest() = default;

  // WizardControllerFlowTest:
  void SetUpOnMainThread() override {
    StartupUtils::MarkOobeCompleted();  // Pretend OOBE was complete.
    WizardControllerFlowTest::SetUpOnMainThread();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerUpdateAfterCompletedOobeTest);
};

// This test verifies that if WizardController reports any result after the
// OOBE is marked complete, it allows the user to proceed to log in.
IN_PROC_BROWSER_TEST_P(WizardControllerUpdateAfterCompletedOobeTest,
                       ControlFlowErrorUpdate) {
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull())).Times(1);
  mock_welcome_screen_->ExitScreen();

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  CheckCurrentScreen(EulaView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  mock_eula_screen_->ExitScreen(
      EulaScreen::Result::ACCEPTED_WITHOUT_USAGE_STATS_REPORTING);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  mock_update_screen_->RunExit(GetParam());

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(0);
  mock_auto_enrollment_check_screen_->ExitScreen();

  EXPECT_NE(nullptr, ExistingUserController::current_controller());
}

INSTANTIATE_TEST_SUITE_P(
    WizardControllerUpdateAfterCompletedOobe,
    WizardControllerUpdateAfterCompletedOobeTest,
    testing::Values(UpdateScreen::Result::UPDATE_NOT_REQUIRED,
                    UpdateScreen::Result::UPDATE_ERROR));

class WizardControllerDeviceStateTest : public WizardControllerFlowTest {
 protected:
  WizardControllerDeviceStateTest() {
    fake_statistics_provider_.SetMachineStatistic(
        system::kSerialNumberKeyForTest, "test");
    fake_statistics_provider_.SetMachineStatistic(system::kActivateDateKey,
                                                  "2000-01");
  }

  static AutoEnrollmentController* auto_enrollment_controller() {
    return WizardController::default_controller()
        ->GetAutoEnrollmentController();
  }

  static void WaitForAutoEnrollmentState(policy::AutoEnrollmentState state) {
    base::RunLoop loop;
    std::unique_ptr<
        AutoEnrollmentController::ProgressCallbackList::Subscription>
        progress_subscription(
            auto_enrollment_controller()->RegisterProgressCallback(
                base::BindRepeating(&QuitLoopOnAutoEnrollmentProgress, state,
                                    &loop)));
    loop.Run();
  }

  // WizardControllerFlowTest:
  void SetUpInProcessBrowserTestFixture() override {
    WizardControllerFlowTest::SetUpInProcessBrowserTestFixture();

    // We need to initialize some dbus clients here, otherwise this test will
    // timeout in WaitForAutoEnrollmentState on asan builds. TODO(stevenjb):
    // Determine which client(s) need to be created and extract and initialize
    // them. https://crbug.com/949063.
    DBusThreadManager::GetSetterForTesting();
  }

  void SetUpOnMainThread() override {
    WizardControllerFlowTest::SetUpOnMainThread();

    histogram_tester_ = std::make_unique<base::HistogramTester>();

    // Initialize the FakeShillManagerClient. This does not happen
    // automatically because of the |DBusThreadManager::GetSetterForTesting|
    // call in |SetUpInProcessBrowserTestFixture|. See https://crbug.com/847422.
    // TODO(pmarko): Find a way for FakeShillManagerClient to be initialized
    // automatically (https://crbug.com/847422).
    DBusThreadManager::Get()
        ->GetShillManagerClient()
        ->GetTestInterface()
        ->SetupDefaultEnvironment();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WizardControllerFlowTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableForcedReEnrollment,
        chromeos::AutoEnrollmentController::kForcedReEnrollmentAlways);
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

  DISALLOW_COPY_AND_ASSIGN(WizardControllerDeviceStateTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerDeviceStateTest,
                       ControlFlowNoForcedReEnrollmentOnFirstBoot) {
  fake_statistics_provider_.ClearMachineStatistic(system::kActivateDateKey);
  EXPECT_NE(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT,
            auto_enrollment_controller()->state());

  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull())).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  mock_welcome_screen_->ExitScreen();

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  CheckCurrentScreen(EulaView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  mock_eula_screen_->ExitScreen(
      EulaScreen::Result::ACCEPTED_WITHOUT_USAGE_STATS_REPORTING);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  mock_auto_enrollment_check_screen_->RealShow();
  EXPECT_EQ(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT,
            auto_enrollment_controller()->state());
  EXPECT_EQ(1,
            FakeCryptohomeClient::Get()
                ->remove_firmware_management_parameters_from_tpm_call_count());
  EXPECT_EQ(1, FakeSessionManagerClient::Get()
                   ->clear_forced_re_enrollment_vpd_call_count());
}

// TODO(https://crbug.com/911661) Flaky time outs on Linux Chromium OS ASan
// LSan bot.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_ControlFlowDeviceDisabled DISABLED_ControlFlowDeviceDisabled
#else
#define MAYBE_ControlFlowDeviceDisabled ControlFlowDeviceDisabled
#endif
IN_PROC_BROWSER_TEST_F(WizardControllerDeviceStateTest,
                       MAYBE_ControlFlowDeviceDisabled) {
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull())).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  mock_welcome_screen_->ExitScreen();

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  CheckCurrentScreen(EulaView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  mock_eula_screen_->ExitScreen(
      EulaScreen::Result::ACCEPTED_WITHOUT_USAGE_STATS_REPORTING);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  mock_auto_enrollment_check_screen_->RealShow();

  // Wait for auto-enrollment controller to encounter the connection error.
  WaitForAutoEnrollmentState(policy::AUTO_ENROLLMENT_STATE_CONNECTION_ERROR);

  // The error screen shows up if device state could not be retrieved.
  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  EXPECT_EQ(AutoEnrollmentCheckScreenView::kScreenId.AsId(),
            GetErrorScreen()->GetParentScreen());
  base::DictionaryValue device_state;
  device_state.SetString(policy::kDeviceStateMode,
                         policy::kDeviceStateRestoreModeDisabled);
  device_state.SetString(policy::kDeviceStateDisabledMessage, kDisabledMessage);
  g_browser_process->local_state()->Set(prefs::kServerBackedDeviceState,
                                        device_state);
  EXPECT_CALL(*device_disabled_screen_view_, UpdateMessage(kDisabledMessage))
      .Times(1);
  EXPECT_CALL(*device_disabled_screen_view_, Show()).Times(1);
  mock_auto_enrollment_check_screen_->ExitScreen();

  base::RunLoop().RunUntilIdle();
  ResetAutoEnrollmentCheckScreen();

  // Make sure the device disabled screen is shown.
  CheckCurrentScreen(DeviceDisabledScreenView::kScreenId);

  EXPECT_EQ(0,
            FakeCryptohomeClient::Get()
                ->remove_firmware_management_parameters_from_tpm_call_count());
  EXPECT_EQ(0, FakeSessionManagerClient::Get()
                   ->clear_forced_re_enrollment_vpd_call_count());

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
}

// Allows testing different behavior if forced re-enrollment is performed but
// not explicitly required (instantiated with |false|) vs. if forced
// re-enrollment is explicitly required (instantiated with |true|).
class WizardControllerDeviceStateExplicitRequirementTest
    : public WizardControllerDeviceStateTest,
      public testing::WithParamInterface<bool /* fre_explicitly_required */> {
 protected:
  WizardControllerDeviceStateExplicitRequirementTest() {}

  // WizardControllerDeviceStateTest:
  void SetUpOnMainThread() override {
    WizardControllerDeviceStateTest::SetUpOnMainThread();

    if (IsFREExplicitlyRequired()) {
      fake_statistics_provider_.SetMachineStatistic(
          chromeos::system::kCheckEnrollmentKey, "1");
    }
  }

  // Returns true if forced re-enrollment was explicitly required (which
  // corresponds to the check_enrollment VPD value being set to "1").
  bool IsFREExplicitlyRequired() { return GetParam(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerDeviceStateExplicitRequirementTest);
};

// Test the control flow for Forced Re-Enrollment. First, a connection error
// occurs, leading to a network error screen. On the network error screen, the
// test verifies that the user may enter a guest session if FRE was not
// explicitly required, and that the user may not enter a guest session if FRE
// was explicitly required. Then, a retyr is performed and FRE indicates that
// the device should be enrolled.
IN_PROC_BROWSER_TEST_P(WizardControllerDeviceStateExplicitRequirementTest,
                       ControlFlowForcedReEnrollment) {
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull())).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  mock_welcome_screen_->ExitScreen();

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  CheckCurrentScreen(EulaView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  mock_eula_screen_->ExitScreen(
      EulaScreen::Result::ACCEPTED_WITHOUT_USAGE_STATS_REPORTING);

  // Let update screen smooth time process (time = 0ms).
  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  mock_auto_enrollment_check_screen_->RealShow();

  // Wait for auto-enrollment controller to encounter the connection error.
  WaitForAutoEnrollmentState(policy::AUTO_ENROLLMENT_STATE_CONNECTION_ERROR);

  // The error screen shows up if there's no auto-enrollment decision.
  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  EXPECT_EQ(AutoEnrollmentCheckScreenView::kScreenId.AsId(),
            GetErrorScreen()->GetParentScreen());

  WaitUntilJSIsReady();
  constexpr char guest_session_link_display[] =
      "window.getComputedStyle($('error-guest-signin-fix-network')).display";
  if (IsFREExplicitlyRequired()) {
    // Check that guest sign-in is not allowed on the network error screen
    // (because the check_enrollment VPD key was set to "1", making FRE
    // explicitly required).
    EXPECT_EQ("none", JSExecuteStringExpression(guest_session_link_display));
  } else {
    // Check that guest sign-in is allowed if FRE was not explicitly required.
    EXPECT_EQ("block", JSExecuteStringExpression(guest_session_link_display));
  }
  EXPECT_EQ(0,
            FakeCryptohomeClient::Get()
                ->remove_firmware_management_parameters_from_tpm_call_count());
  EXPECT_EQ(0, FakeSessionManagerClient::Get()
                   ->clear_forced_re_enrollment_vpd_call_count());

  base::DictionaryValue device_state;
  device_state.SetString(policy::kDeviceStateMode,
                         policy::kDeviceStateRestoreModeReEnrollmentEnforced);
  g_browser_process->local_state()->Set(prefs::kServerBackedDeviceState,
                                        device_state);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(
      *mock_enrollment_screen_view_,
      SetEnrollmentConfig(
          mock_enrollment_screen_,
          EnrollmentModeMatches(policy::EnrollmentConfig::MODE_SERVER_FORCED)))
      .Times(1);
  mock_auto_enrollment_check_screen_->ExitScreen();

  ResetAutoEnrollmentCheckScreen();

  // Make sure enterprise enrollment page shows up.
  CheckCurrentScreen(EnrollmentScreenView::kScreenId);
  mock_enrollment_screen_->ExitScreen(EnrollmentScreen::Result::COMPLETED);

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());

  histogram_tester()->ExpectTotalCount(
      "Enterprise.InitialEnrollmentRequirement", 0 /* count */);
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
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull())).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  mock_welcome_screen_->ExitScreen();

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  CheckCurrentScreen(EulaView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  mock_eula_screen_->ExitScreen(
      EulaScreen::Result::ACCEPTED_WITHOUT_USAGE_STATS_REPORTING);

  // Let update screen smooth time process (time = 0ms).
  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  mock_auto_enrollment_check_screen_->RealShow();

  policy::FakeAutoEnrollmentClient* fake_auto_enrollment_client =
      fake_auto_enrollment_client_factory.WaitAutoEnrollmentClientCreated();
  if (IsFREExplicitlyRequired()) {
    // Expect that the auto enrollment screen will be hidden, because OOBE is
    // switching to the error screen.
    EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);

    // Make AutoEnrollmentClient notify the controller that a server error
    // occured.
    fake_auto_enrollment_client->SetState(
        policy::AUTO_ENROLLMENT_STATE_SERVER_ERROR);
    base::RunLoop().RunUntilIdle();

    // The error screen shows up.
    EXPECT_FALSE(StartupUtils::IsOobeCompleted());
    CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
    EXPECT_EQ(AutoEnrollmentCheckScreenView::kScreenId.AsId(),
              GetErrorScreen()->GetParentScreen());

    WaitUntilJSIsReady();
    constexpr char guest_session_link_display[] =
        "window.getComputedStyle($('error-guest-signin-fix-network'))."
        "display";
    // Check that guest sign-in is not allowed on the network error screen
    // (because the check_enrollment VPD key was set to "1", making FRE
    // explicitly required).
    EXPECT_EQ("none", JSExecuteStringExpression(guest_session_link_display));

    base::DictionaryValue device_state;
    device_state.SetString(policy::kDeviceStateMode,
                           policy::kDeviceStateRestoreModeReEnrollmentEnforced);
    g_browser_process->local_state()->Set(prefs::kServerBackedDeviceState,
                                          device_state);
    EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
    EXPECT_CALL(
        *mock_enrollment_screen_view_,
        SetEnrollmentConfig(mock_enrollment_screen_,
                            EnrollmentModeMatches(
                                policy::EnrollmentConfig::MODE_SERVER_FORCED)))
        .Times(1);
    fake_auto_enrollment_client->SetState(
        policy::AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
    mock_auto_enrollment_check_screen_->ExitScreen();

    ResetAutoEnrollmentCheckScreen();

    // Make sure enterprise enrollment page shows up.
    CheckCurrentScreen(EnrollmentScreenView::kScreenId);
    mock_enrollment_screen_->ExitScreen(EnrollmentScreen::Result::COMPLETED);

    EXPECT_TRUE(StartupUtils::IsOobeCompleted());
    EXPECT_EQ(
        0, FakeCryptohomeClient::Get()
               ->remove_firmware_management_parameters_from_tpm_call_count());
    EXPECT_EQ(0, FakeSessionManagerClient::Get()
                     ->clear_forced_re_enrollment_vpd_call_count());
  } else {
    // Don't expect that the auto enrollment screen will be hidden, because
    // OOBE is exited from the auto enrollment screen. Instead only expect
    // that the sign-in screen is reached.
    content::WindowedNotificationObserver login_screen_waiter(
        chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
        content::NotificationService::AllSources());

    // Make AutoEnrollmentClient notify the controller that a server error
    // occured.
    fake_auto_enrollment_client->SetState(
        policy::AUTO_ENROLLMENT_STATE_SERVER_ERROR);
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(StartupUtils::IsOobeCompleted());
    login_screen_waiter.Wait();
    EXPECT_EQ(
        0, FakeCryptohomeClient::Get()
               ->remove_firmware_management_parameters_from_tpm_call_count());
    EXPECT_EQ(0, FakeSessionManagerClient::Get()
                     ->clear_forced_re_enrollment_vpd_call_count());
  }
}

INSTANTIATE_TEST_SUITE_P(WizardControllerDeviceStateExplicitRequirement,
                         WizardControllerDeviceStateExplicitRequirementTest,
                         testing::Values(false, true));

class WizardControllerDeviceStateWithInitialEnrollmentTest
    : public WizardControllerDeviceStateTest {
 protected:
  WizardControllerDeviceStateWithInitialEnrollmentTest() {
    fake_statistics_provider_.SetMachineStatistic(
        system::kSerialNumberKeyForTest, "test");
    fake_statistics_provider_.SetMachineStatistic(system::kRlzBrandCodeKey,
                                                  "AABC");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WizardControllerDeviceStateTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableInitialEnrollment,
        chromeos::AutoEnrollmentController::kInitialEnrollmentAlways);
  }

  SystemClockClient::TestInterface* system_clock_client() {
    return SystemClockClient::Get()->GetTestInterface();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(
      WizardControllerDeviceStateWithInitialEnrollmentTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerDeviceStateWithInitialEnrollmentTest,
                       ControlFlowInitialEnrollment) {
  fake_statistics_provider_.ClearMachineStatistic(system::kActivateDateKey);
  fake_statistics_provider_.SetMachineStatistic(
      system::kRlzEmbargoEndDateKey,
      GenerateEmbargoEndDate(-15 /* days_offset */));
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull())).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  mock_welcome_screen_->ExitScreen();

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  CheckCurrentScreen(EulaView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  mock_eula_screen_->ExitScreen(
      EulaScreen::Result::ACCEPTED_WITHOUT_USAGE_STATS_REPORTING);

  // Let update screen smooth time process (time = 0ms).
  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  mock_auto_enrollment_check_screen_->RealShow();

  // Wait for auto-enrollment controller to encounter the connection error.
  WaitForAutoEnrollmentState(policy::AUTO_ENROLLMENT_STATE_CONNECTION_ERROR);

  // The error screen shows up if there's no auto-enrollment decision.
  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_EQ(AutoEnrollmentCheckScreenView::kScreenId.AsId(),
            GetErrorScreen()->GetParentScreen());
  base::DictionaryValue device_state;
  device_state.SetString(policy::kDeviceStateMode,
                         policy::kDeviceStateRestoreModeReEnrollmentEnforced);
  g_browser_process->local_state()->Set(prefs::kServerBackedDeviceState,
                                        device_state);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(
      *mock_enrollment_screen_view_,
      SetEnrollmentConfig(
          mock_enrollment_screen_,
          EnrollmentModeMatches(policy::EnrollmentConfig::MODE_SERVER_FORCED)))
      .Times(1);
  mock_auto_enrollment_check_screen_->ExitScreen();

  ResetAutoEnrollmentCheckScreen();

  // Make sure enterprise enrollment page shows up.
  CheckCurrentScreen(EnrollmentScreenView::kScreenId);
  mock_enrollment_screen_->ExitScreen(EnrollmentScreen::Result::COMPLETED);

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
  histogram_tester()->ExpectUniqueSample(
      "Enterprise.InitialEnrollmentRequirement", 0 /* Required */,
      1 /* count */);
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
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull())).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  mock_welcome_screen_->ExitScreen();

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  CheckCurrentScreen(EulaView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  mock_eula_screen_->ExitScreen(
      EulaScreen::Result::ACCEPTED_WITHOUT_USAGE_STATS_REPORTING);

  // Let update screen smooth time process (time = 0ms).
  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  mock_auto_enrollment_check_screen_->RealShow();

  policy::FakeAutoEnrollmentClient* fake_auto_enrollment_client =
      fake_auto_enrollment_client_factory.WaitAutoEnrollmentClientCreated();

  // Expect that the auto enrollment screen will be hidden, because OOBE is
  // switching to the error screen.
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);

  // Make AutoEnrollmentClient notify the controller that a server error
  // occured.
  fake_auto_enrollment_client->SetState(
      policy::AUTO_ENROLLMENT_STATE_SERVER_ERROR);
  base::RunLoop().RunUntilIdle();

  // The error screen shows up if there's no auto-enrollment decision.
  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_EQ(AutoEnrollmentCheckScreenView::kScreenId.AsId(),
            GetErrorScreen()->GetParentScreen());

  WaitUntilJSIsReady();
  constexpr char guest_session_link_display[] =
      "window.getComputedStyle($('error-guest-signin-fix-network'))."
      "display";
  // Check that guest sign-in is allowed on the network error screen for initial
  // enrollment.
  EXPECT_EQ("block", JSExecuteStringExpression(guest_session_link_display));

  base::DictionaryValue device_state;
  device_state.SetString(policy::kDeviceStateMode,
                         policy::kDeviceStateRestoreModeReEnrollmentEnforced);
  g_browser_process->local_state()->Set(prefs::kServerBackedDeviceState,
                                        device_state);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(
      *mock_enrollment_screen_view_,
      SetEnrollmentConfig(
          mock_enrollment_screen_,
          EnrollmentModeMatches(policy::EnrollmentConfig::MODE_SERVER_FORCED)))
      .Times(1);
  fake_auto_enrollment_client->SetState(
      policy::AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  mock_auto_enrollment_check_screen_->ExitScreen();

  ResetAutoEnrollmentCheckScreen();

  // Make sure enterprise enrollment page shows up.
  CheckCurrentScreen(EnrollmentScreenView::kScreenId);
  mock_enrollment_screen_->ExitScreen(EnrollmentScreen::Result::COMPLETED);

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());

  histogram_tester()->ExpectUniqueSample(
      "Enterprise.InitialEnrollmentRequirement", 0 /* Required */,
      1 /* count */);
}

IN_PROC_BROWSER_TEST_F(WizardControllerDeviceStateWithInitialEnrollmentTest,
                       ControlFlowNoInitialEnrollmentDuringEmbargoPeriod) {
  system_clock_client()->SetNetworkSynchronized(true);
  system_clock_client()->NotifyObserversSystemClockUpdated();

  fake_statistics_provider_.ClearMachineStatistic(system::kActivateDateKey);
  fake_statistics_provider_.SetMachineStatistic(
      system::kRlzEmbargoEndDateKey,
      GenerateEmbargoEndDate(1 /* days_offset */));
  EXPECT_NE(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT,
            auto_enrollment_controller()->state());

  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull())).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  mock_welcome_screen_->ExitScreen();

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  CheckCurrentScreen(EulaView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  mock_eula_screen_->ExitScreen(
      EulaScreen::Result::ACCEPTED_WITHOUT_USAGE_STATS_REPORTING);

  // Let update screen smooth time process (time = 0ms).
  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  mock_auto_enrollment_check_screen_->RealShow();
  EXPECT_EQ(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT,
            auto_enrollment_controller()->state());

  histogram_tester()->ExpectUniqueSample(
      "Enterprise.InitialEnrollmentRequirement",
      5 /* NotRequiredInEmbargoPeriod*/, 1 /* count */);
}

IN_PROC_BROWSER_TEST_F(WizardControllerDeviceStateWithInitialEnrollmentTest,
                       ControlFlowWaitSystemClockSyncThenEmbargoPeriod) {
  fake_statistics_provider_.ClearMachineStatistic(system::kActivateDateKey);
  fake_statistics_provider_.SetMachineStatistic(
      system::kRlzEmbargoEndDateKey,
      GenerateEmbargoEndDate(1 /* days_offset */));
  EXPECT_NE(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT,
            auto_enrollment_controller()->state());

  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull())).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  mock_welcome_screen_->ExitScreen();

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  CheckCurrentScreen(EulaView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  mock_eula_screen_->ExitScreen(
      EulaScreen::Result::ACCEPTED_WITHOUT_USAGE_STATS_REPORTING);

  // Let update screen smooth time process (time = 0ms).
  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  mock_auto_enrollment_check_screen_->RealShow();
  EXPECT_EQ(AutoEnrollmentController::AutoEnrollmentCheckType::kNone,
            auto_enrollment_controller()->auto_enrollment_check_type());

  system_clock_client()->SetNetworkSynchronized(true);
  system_clock_client()->NotifyObserversSystemClockUpdated();

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(AutoEnrollmentController::AutoEnrollmentCheckType::kNone,
            auto_enrollment_controller()->auto_enrollment_check_type());
  EXPECT_EQ(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT,
            auto_enrollment_controller()->state());

  histogram_tester()->ExpectUniqueSample(
      "Enterprise.InitialEnrollmentRequirement",
      5 /* NotRequiredInEmbargoPeriod*/, 1 /* count */);
}

IN_PROC_BROWSER_TEST_F(WizardControllerDeviceStateWithInitialEnrollmentTest,
                       ControlFlowWaitSystemClockSyncTimeout) {
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();

  base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner);
  fake_statistics_provider_.ClearMachineStatistic(system::kActivateDateKey);
  fake_statistics_provider_.SetMachineStatistic(
      system::kRlzEmbargoEndDateKey,
      GenerateEmbargoEndDate(1 /* days_offset */));
  EXPECT_NE(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT,
            auto_enrollment_controller()->state());

  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull())).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  mock_welcome_screen_->ExitScreen();

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  CheckCurrentScreen(EulaView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  mock_eula_screen_->ExitScreen(
      EulaScreen::Result::ACCEPTED_WITHOUT_USAGE_STATS_REPORTING);

  // Let update screen smooth time process (time = 0ms).
  task_runner->RunUntilIdle();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  mock_auto_enrollment_check_screen_->RealShow();
  EXPECT_EQ(AutoEnrollmentController::AutoEnrollmentCheckType::kNone,
            auto_enrollment_controller()->auto_enrollment_check_type());

  // The timeout is 15 seconds, see |auto_enrollment_controller.cc|.
  // Fast-forward by a bit more than that.
  task_runner->FastForwardBy(base::TimeDelta::FromSeconds(15 + 1));

  EXPECT_EQ(AutoEnrollmentController::AutoEnrollmentCheckType::kNone,
            auto_enrollment_controller()->auto_enrollment_check_type());
  EXPECT_EQ(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT,
            auto_enrollment_controller()->state());

  histogram_tester()->ExpectUniqueSample(
      "Enterprise.InitialEnrollmentRequirement",
      6 /* NotRequiredInEmbargoPeriodWithoutSystemClockSync*/, 1 /* count */);
}

IN_PROC_BROWSER_TEST_F(WizardControllerDeviceStateWithInitialEnrollmentTest,
                       ControlFlowWaitSystemClockSyncThenInitialEnrollment) {
  ScopedFakeAutoEnrollmentClientFactory fake_auto_enrollment_client_factory(
      auto_enrollment_controller());

  fake_statistics_provider_.ClearMachineStatistic(system::kActivateDateKey);
  fake_statistics_provider_.SetMachineStatistic(
      system::kRlzEmbargoEndDateKey,
      GenerateEmbargoEndDate(1 /* days_offset */));
  EXPECT_NE(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT,
            auto_enrollment_controller()->state());

  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull())).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  mock_welcome_screen_->ExitScreen();

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  CheckCurrentScreen(EulaView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  mock_eula_screen_->ExitScreen(
      EulaScreen::Result::ACCEPTED_WITHOUT_USAGE_STATS_REPORTING);

  // Let update screen smooth time process (time = 0ms).
  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  mock_auto_enrollment_check_screen_->RealShow();
  EXPECT_EQ(AutoEnrollmentController::AutoEnrollmentCheckType::kNone,
            auto_enrollment_controller()->auto_enrollment_check_type());

  // Simulate that the clock moved forward, passing the embargo period, by
  // moving the embargo period back in time.
  fake_statistics_provider_.SetMachineStatistic(
      system::kRlzEmbargoEndDateKey,
      GenerateEmbargoEndDate(-1 /* days_offset */));
  base::DictionaryValue device_state;
  device_state.SetString(policy::kDeviceStateMode,
                         policy::kDeviceStateRestoreModeReEnrollmentEnforced);
  g_browser_process->local_state()->Set(prefs::kServerBackedDeviceState,
                                        device_state);

  system_clock_client()->SetNetworkSynchronized(true);
  system_clock_client()->NotifyObserversSystemClockUpdated();

  policy::FakeAutoEnrollmentClient* fake_auto_enrollment_client =
      fake_auto_enrollment_client_factory.WaitAutoEnrollmentClientCreated();

  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);

  EXPECT_CALL(
      *mock_enrollment_screen_view_,
      SetEnrollmentConfig(
          mock_enrollment_screen_,
          EnrollmentModeMatches(policy::EnrollmentConfig::MODE_SERVER_FORCED)))
      .Times(1);
  mock_auto_enrollment_check_screen_->ExitScreen();
  ResetAutoEnrollmentCheckScreen();

  fake_auto_enrollment_client->SetState(
      policy::AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);

  // Make sure enterprise enrollment page shows up.
  CheckCurrentScreen(EnrollmentScreenView::kScreenId);
  mock_enrollment_screen_->ExitScreen(EnrollmentScreen::Result::COMPLETED);
  EXPECT_TRUE(StartupUtils::IsOobeCompleted());

  histogram_tester()->ExpectUniqueSample(
      "Enterprise.InitialEnrollmentRequirement", 0 /* Required */,
      1 /* count */);
}

IN_PROC_BROWSER_TEST_F(WizardControllerDeviceStateWithInitialEnrollmentTest,
                       ControlFlowNoInitialEnrollmentIfEnrolledBefore) {
  fake_statistics_provider_.SetMachineStatistic(system::kCheckEnrollmentKey,
                                                "0");
  fake_statistics_provider_.SetMachineStatistic(
      system::kRlzEmbargoEndDateKey,
      GenerateEmbargoEndDate(1 /* days_offset */));
  EXPECT_NE(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT,
            auto_enrollment_controller()->state());

  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull())).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  mock_welcome_screen_->ExitScreen();

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  CheckCurrentScreen(EulaView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  mock_eula_screen_->ExitScreen(
      EulaScreen::Result::ACCEPTED_WITHOUT_USAGE_STATS_REPORTING);

  // Let update screen smooth time process (time = 0ms).
  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  mock_auto_enrollment_check_screen_->RealShow();
  EXPECT_EQ(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT,
            auto_enrollment_controller()->state());
}

class WizardControllerBrokenLocalStateTest : public WizardControllerTest {
 protected:
  WizardControllerBrokenLocalStateTest() = default;
  ~WizardControllerBrokenLocalStateTest() override = default;

  // WizardControllerTest:
  void SetUpOnMainThread() override {
    PrefServiceFactory factory;
    factory.set_user_prefs(base::MakeRefCounted<PrefStoreStub>());
    local_state_ = factory.Create(new PrefRegistrySimple());
    WizardController::set_local_state_for_testing(local_state_.get());

    WizardControllerTest::SetUpOnMainThread();

    // Make sure that OOBE is run as an "official" build.
    branded_build_override_ = WizardController::ForceBrandedBuildForTesting();
  }

 private:
  std::unique_ptr<PrefService> local_state_;
  std::unique_ptr<base::AutoReset<bool>> branded_build_override_;

  DISALLOW_COPY_AND_ASSIGN(WizardControllerBrokenLocalStateTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerBrokenLocalStateTest,
                       LocalStateCorrupted) {
  // Checks that after wizard controller initialization error screen
  // in the proper state is displayed.
  ASSERT_EQ(GetErrorScreen(),
            WizardController::default_controller()->current_screen());
  ASSERT_EQ(NetworkError::UI_STATE_LOCAL_STATE_ERROR,
            GetErrorScreen()->GetUIState());

  WaitUntilJSIsReady();

  // Checks visibility of the error message and powerwash button.
  ASSERT_FALSE(JSExecuteBooleanExpression("$('error-message').hidden"));
  ASSERT_TRUE(JSExecuteBooleanExpression(
      "$('error-message').classList.contains('ui-state-local-state-error')"));

  // Emulates user click on the "Restart and Powerwash" button.
  ASSERT_EQ(0, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
  ASSERT_TRUE(content::ExecuteScript(
      GetWebContents(), "$('error-message-md-powerwash-button').click();"));
  ASSERT_EQ(1, FakeSessionManagerClient::Get()->start_device_wipe_call_count());
}

class WizardControllerProxyAuthOnSigninTest : public WizardControllerTest {
 protected:
  WizardControllerProxyAuthOnSigninTest()
      : proxy_server_(net::SpawnedTestServer::TYPE_BASIC_AUTH_PROXY,
                      base::FilePath()) {}
  ~WizardControllerProxyAuthOnSigninTest() override {}

  // WizardControllerTest:
  void SetUp() override {
    ASSERT_TRUE(proxy_server_.Start());
    WizardControllerTest::SetUp();
  }

  void SetUpOnMainThread() override {
    WizardControllerTest::SetUpOnMainThread();
    WizardController::default_controller()->AdvanceToScreen(
        WelcomeView::kScreenId);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WizardControllerTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(::switches::kProxyServer,
                                    proxy_server_.host_port_pair().ToString());
  }

  net::SpawnedTestServer& proxy_server() { return proxy_server_; }

 private:
  net::SpawnedTestServer proxy_server_;

  DISALLOW_COPY_AND_ASSIGN(WizardControllerProxyAuthOnSigninTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerProxyAuthOnSigninTest,
                       ProxyAuthDialogOnSigninScreen) {
  content::WindowedNotificationObserver auth_needed_waiter(
      chrome::NOTIFICATION_AUTH_NEEDED,
      content::NotificationService::AllSources());

  CheckCurrentScreen(WelcomeView::kScreenId);

  LoginDisplayHost::default_host()->StartSignInScreen(LoginScreenContext());
  auth_needed_waiter.Wait();
}

class WizardControllerKioskFlowTest : public WizardControllerFlowTest {
 protected:
  WizardControllerKioskFlowTest() {}

  // MixinBasedInProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WizardControllerFlowTest::SetUpCommandLine(command_line);
    base::FilePath test_data_dir;
    ASSERT_TRUE(chromeos::test_utils::GetTestDataPath(
        "app_mode", "kiosk_manifest", &test_data_dir));
    command_line->AppendSwitchPath(
        switches::kAppOemManifestFile,
        test_data_dir.AppendASCII("kiosk_manifest.json"));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerKioskFlowTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerKioskFlowTest,
                       ControlFlowKioskForcedEnrollment) {
  EXPECT_CALL(
      *mock_enrollment_screen_view_,
      SetEnrollmentConfig(
          mock_enrollment_screen_,
          EnrollmentModeMatches(policy::EnrollmentConfig::MODE_LOCAL_FORCED)))
      .Times(1);
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull())).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  mock_welcome_screen_->ExitScreen();

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  CheckCurrentScreen(EulaView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  mock_eula_screen_->ExitScreen(
      EulaScreen::Result::ACCEPTED_WITHOUT_USAGE_STATS_REPORTING);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  mock_auto_enrollment_check_screen_->ExitScreen();

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());

  // Make sure enterprise enrollment page shows up right after update screen.
  CheckCurrentScreen(EnrollmentScreenView::kScreenId);
  mock_enrollment_screen_->ExitScreen(EnrollmentScreen::Result::COMPLETED);

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
}

IN_PROC_BROWSER_TEST_F(WizardControllerKioskFlowTest,
                       ControlFlowEnrollmentBack) {
  EXPECT_CALL(
      *mock_enrollment_screen_view_,
      SetEnrollmentConfig(
          mock_enrollment_screen_,
          EnrollmentModeMatches(policy::EnrollmentConfig::MODE_LOCAL_FORCED)))
      .Times(1);

  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull())).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  mock_welcome_screen_->ExitScreen();

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  CheckCurrentScreen(EulaView::kScreenId);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  mock_eula_screen_->ExitScreen(
      EulaScreen::Result::ACCEPTED_WITHOUT_USAGE_STATS_REPORTING);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Hide()).Times(1);
  mock_auto_enrollment_check_screen_->ExitScreen();

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());

  // Make sure enterprise enrollment page shows up right after update screen.
  CheckCurrentScreen(EnrollmentScreenView::kScreenId);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  mock_enrollment_screen_->ExitScreen(EnrollmentScreen::Result::BACK);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
}

class WizardControllerEnableAdbSideloadingTest
    : public WizardControllerFlowTest {
 protected:
  WizardControllerEnableAdbSideloadingTest() = default;

  template <class T>
  void SkipToScreen(OobeScreenId screen, T* screen_mock) {
    EXPECT_CALL(*screen_mock, Show()).Times(1);
    auto* const wizard_controller = WizardController::default_controller();
    wizard_controller->AdvanceToScreen(screen);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerEnableAdbSideloadingTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerEnableAdbSideloadingTest,
                       ShowAndEnableSideloading) {
  CheckCurrentScreen(WelcomeView::kScreenId);
  WaitUntilJSIsReady();

  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull())).Times(1);
  SkipToScreen(EnableAdbSideloadingScreenView::kScreenId,
               mock_enable_adb_sideloading_screen_);
  CheckCurrentScreen(EnableAdbSideloadingScreenView::kScreenId);

  test::OobeJS().ClickOnPath(
      {"adb-sideloading", "enable-adb-sideloading-ok-button"});

  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(EnableAdbSideloadingScreenView::kScreenId);
  EXPECT_CALL(*mock_enable_adb_sideloading_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(NotNull())).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, Show()).Times(1);

  mock_enable_adb_sideloading_screen_->ExitScreen();

  // Let update screen smooth time process (time = 0ms).
  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(WelcomeView::kScreenId);
}

IN_PROC_BROWSER_TEST_F(WizardControllerEnableAdbSideloadingTest,
                       ShowAndDoNotEnableSideloading) {
  CheckCurrentScreen(WelcomeView::kScreenId);
  WaitUntilJSIsReady();

  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull())).Times(1);
  SkipToScreen(EnableAdbSideloadingScreenView::kScreenId,
               mock_enable_adb_sideloading_screen_);
  CheckCurrentScreen(EnableAdbSideloadingScreenView::kScreenId);

  test::OobeJS().ClickOnPath(
      {"adb-sideloading", "enable-adb-sideloading-cancel-button"});

  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(EnableAdbSideloadingScreenView::kScreenId);
  EXPECT_CALL(*mock_enable_adb_sideloading_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(NotNull())).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, Show()).Times(1);

  mock_enable_adb_sideloading_screen_->ExitScreen();

  // Let update screen smooth time process (time = 0ms).
  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(WelcomeView::kScreenId);
}

class WizardControllerEnableDebuggingTest : public WizardControllerFlowTest {
 protected:
  WizardControllerEnableDebuggingTest() {}

  // MixinBasedInProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WizardControllerFlowTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(chromeos::switches::kSystemDevMode);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerEnableDebuggingTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerEnableDebuggingTest,
                       ShowAndCancelEnableDebugging) {
  CheckCurrentScreen(WelcomeView::kScreenId);
  WaitUntilJSIsReady();

  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull())).Times(1);
  EXPECT_CALL(*mock_enable_debugging_screen_, Show()).Times(1);

  // Find the enable debugging link element (in the appropriate shadow root),
  // and click it.
  test::OobeJS().ClickOnPath(
      {"connect", "welcomeScreen", "enableDebuggingLink"});

  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(EnableDebuggingScreenView::kScreenId);
  EXPECT_CALL(*mock_enable_debugging_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(NotNull())).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, Show()).Times(1);

  mock_enable_debugging_screen_->ExitScreen();

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(WelcomeView::kScreenId);
}

class WizardControllerDemoSetupTest : public WizardControllerFlowTest {
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
    EXPECT_CALL(*screen_mock, Show()).Times(1);
    auto* const wizard_controller = WizardController::default_controller();
    wizard_controller->SimulateDemoModeSetupForTesting();
    wizard_controller->AdvanceToScreen(screen);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerDemoSetupTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerDemoSetupTest,
                       OnlineDemoSetupFlowFinished) {
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
  WaitUntilJSIsReady();

  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull())).Times(1);
  EXPECT_CALL(*mock_demo_preferences_screen_, Show()).Times(1);

  WizardController::default_controller()->StartDemoModeSetup();

  CheckCurrentScreen(DemoPreferencesScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_demo_preferences_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);

  mock_demo_preferences_screen_->ExitScreen(
      DemoPreferencesScreen::Result::COMPLETED);

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);

  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  CheckCurrentScreen(EulaView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_arc_terms_of_service_screen_, Show()).Times(1);

  mock_eula_screen_->ExitScreen(
      EulaScreen::Result::ACCEPTED_WITHOUT_USAGE_STATS_REPORTING);

  CheckCurrentScreen(ArcTermsOfServiceScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_arc_terms_of_service_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);

  mock_arc_terms_of_service_screen_->ExitScreen(
      ArcTermsOfServiceScreen::Result::ACCEPTED);

  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);

  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_demo_setup_screen_, Show()).Times(1);

  mock_auto_enrollment_check_screen_->ExitScreen();

  CheckCurrentScreen(DemoSetupScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  mock_demo_setup_screen_->ExitScreen(DemoSetupScreen::Result::COMPLETED);

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
  EXPECT_TRUE(ExistingUserController::current_controller());
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
}

IN_PROC_BROWSER_TEST_F(WizardControllerDemoSetupTest,
                       OfflineDemoSetupFlowFinished) {
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
  WaitUntilJSIsReady();

  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull())).Times(1);
  EXPECT_CALL(*mock_demo_preferences_screen_, Show()).Times(1);

  WizardController::default_controller()->StartDemoModeSetup();

  CheckCurrentScreen(DemoPreferencesScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_demo_preferences_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);

  mock_demo_preferences_screen_->ExitScreen(
      DemoPreferencesScreen::Result::COMPLETED);

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);

  mock_network_screen_->ExitScreen(NetworkScreen::Result::OFFLINE_DEMO_SETUP);

  CheckCurrentScreen(EulaView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_arc_terms_of_service_screen_, Show()).Times(1);

  mock_eula_screen_->ExitScreen(
      EulaScreen::Result::ACCEPTED_WITHOUT_USAGE_STATS_REPORTING);

  CheckCurrentScreen(ArcTermsOfServiceScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_arc_terms_of_service_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_demo_setup_screen_, Show()).Times(1);

  mock_arc_terms_of_service_screen_->ExitScreen(
      ArcTermsOfServiceScreen::Result::ACCEPTED);

  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(DemoSetupScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  mock_demo_setup_screen_->ExitScreen(DemoSetupScreen::Result::COMPLETED);

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
  EXPECT_TRUE(ExistingUserController::current_controller());
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
}

IN_PROC_BROWSER_TEST_F(WizardControllerDemoSetupTest, DemoSetupCanceled) {
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
  WaitUntilJSIsReady();

  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull())).Times(1);
  EXPECT_CALL(*mock_demo_preferences_screen_, Show()).Times(1);

  WizardController::default_controller()->StartDemoModeSetup();

  CheckCurrentScreen(DemoPreferencesScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_demo_preferences_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);

  mock_demo_preferences_screen_->ExitScreen(
      DemoPreferencesScreen::Result::COMPLETED);

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);

  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  CheckCurrentScreen(EulaView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_arc_terms_of_service_screen_, Show()).Times(1);

  mock_eula_screen_->ExitScreen(
      EulaScreen::Result::ACCEPTED_WITHOUT_USAGE_STATS_REPORTING);

  CheckCurrentScreen(ArcTermsOfServiceScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_arc_terms_of_service_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);

  mock_arc_terms_of_service_screen_->ExitScreen(
      ArcTermsOfServiceScreen::Result::ACCEPTED);

  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);

  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_demo_setup_screen_, Show()).Times(1);

  mock_auto_enrollment_check_screen_->ExitScreen();

  CheckCurrentScreen(DemoSetupScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_demo_setup_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(NotNull())).Times(1);

  mock_demo_setup_screen_->ExitScreen(DemoSetupScreen::Result::CANCELED);

  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
}

IN_PROC_BROWSER_TEST_F(WizardControllerDemoSetupTest, DemoPreferencesCanceled) {
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
  WaitUntilJSIsReady();
  SkipToScreen(DemoPreferencesScreenView::kScreenId,
               mock_demo_preferences_screen_);

  CheckCurrentScreen(DemoPreferencesScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_demo_preferences_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, Show()).Times(1);

  mock_demo_preferences_screen_->ExitScreen(
      DemoPreferencesScreen::Result::CANCELED);

  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
}

IN_PROC_BROWSER_TEST_F(WizardControllerDemoSetupTest, NetworkBackPressed) {
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
  WaitUntilJSIsReady();
  SkipToScreen(NetworkScreenView::kScreenId, mock_network_screen_);

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_demo_preferences_screen_, Show()).Times(1);

  mock_network_screen_->ExitScreen(NetworkScreen::Result::BACK);

  CheckCurrentScreen(DemoPreferencesScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
}

IN_PROC_BROWSER_TEST_F(WizardControllerDemoSetupTest, EulaBackPressed) {
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
  WaitUntilJSIsReady();
  SkipToScreen(EulaView::kScreenId, mock_eula_screen_);

  CheckCurrentScreen(EulaView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);

  mock_eula_screen_->ExitScreen(EulaScreen::Result::BACK);

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
}

IN_PROC_BROWSER_TEST_F(WizardControllerDemoSetupTest, ArcTosBackPressed) {
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
  WaitUntilJSIsReady();

  // User cannot go to ARC ToS screen without accepting eula - simulate that.
  StartupUtils::MarkEulaAccepted();
  SkipToScreen(ArcTermsOfServiceScreenView::kScreenId,
               mock_arc_terms_of_service_screen_);

  CheckCurrentScreen(ArcTermsOfServiceScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_arc_terms_of_service_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);

  mock_arc_terms_of_service_screen_->ExitScreen(
      ArcTermsOfServiceScreen::Result::BACK);

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
}

class WizardControllerDemoSetupDeviceDisabledTest
    : public WizardControllerDeviceStateTest {
 protected:
  WizardControllerDemoSetupDeviceDisabledTest() = default;
  ~WizardControllerDemoSetupDeviceDisabledTest() override = default;

  // MixinBasedInProcessBrowserTest:
  void SetUpOnMainThread() override {
    WizardControllerDeviceStateTest::SetUpOnMainThread();
    testing::Mock::VerifyAndClearExpectations(mock_welcome_screen_);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerDemoSetupDeviceDisabledTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerDemoSetupDeviceDisabledTest,
                       OnlineDemoSetup) {
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
  WaitUntilJSIsReady();

  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull())).Times(1);
  EXPECT_CALL(*mock_demo_preferences_screen_, Show()).Times(1);

  WizardController::default_controller()->StartDemoModeSetup();

  CheckCurrentScreen(DemoPreferencesScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_demo_preferences_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);

  mock_demo_preferences_screen_->ExitScreen(
      DemoPreferencesScreen::Result::COMPLETED);

  CheckCurrentScreen(NetworkScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);

  mock_network_screen_->ExitScreen(NetworkScreen::Result::CONNECTED);

  CheckCurrentScreen(EulaView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_arc_terms_of_service_screen_, Show()).Times(1);

  mock_eula_screen_->ExitScreen(
      EulaScreen::Result::ACCEPTED_WITHOUT_USAGE_STATS_REPORTING);

  CheckCurrentScreen(ArcTermsOfServiceScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_arc_terms_of_service_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);

  mock_arc_terms_of_service_screen_->ExitScreen(
      ArcTermsOfServiceScreen::Result::ACCEPTED);

  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(UpdateView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);

  mock_update_screen_->RunExit(UpdateScreen::Result::UPDATE_NOT_REQUIRED);

  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);

  mock_auto_enrollment_check_screen_->RealShow();

  // Wait for auto-enrollment controller to encounter the connection error.
  WaitForAutoEnrollmentState(policy::AUTO_ENROLLMENT_STATE_CONNECTION_ERROR);

  // The error screen shows up if device state could not be retrieved.
  CheckCurrentScreen(AutoEnrollmentCheckScreenView::kScreenId);
  EXPECT_EQ(AutoEnrollmentCheckScreenView::kScreenId.AsId(),
            GetErrorScreen()->GetParentScreen());
  base::DictionaryValue device_state;
  device_state.SetString(policy::kDeviceStateMode,
                         policy::kDeviceStateRestoreModeDisabled);
  device_state.SetString(policy::kDeviceStateDisabledMessage, kDisabledMessage);
  g_browser_process->local_state()->Set(prefs::kServerBackedDeviceState,
                                        device_state);

  EXPECT_CALL(*device_disabled_screen_view_, Show()).Times(1);
  mock_auto_enrollment_check_screen_->ExitScreen();

  base::RunLoop().RunUntilIdle();

  ResetAutoEnrollmentCheckScreen();
  CheckCurrentScreen(DeviceDisabledScreenView::kScreenId);

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
}

class WizardControllerOobeResumeTest : public WizardControllerTest {
 protected:
  WizardControllerOobeResumeTest() {}
  // WizardControllerTest:
  void SetUpOnMainThread() override {
    WizardControllerTest::SetUpOnMainThread();

    // Make sure that OOBE is run as an "official" build.
    branded_build_override_ = WizardController::ForceBrandedBuildForTesting();

    WizardController* wizard_controller =
        WizardController::default_controller();

    // Clear portal list (as it is by default in OOBE).
    NetworkHandler::Get()->network_state_handler()->SetCheckPortalList("");

    // Set up the mocks for all screens.
    mock_welcome_view_ = std::make_unique<MockWelcomeView>();
    ExpectBindUnbind(mock_welcome_view_.get());
    mock_welcome_screen_ =
        MockScreenExpectLifecycle(std::make_unique<MockWelcomeScreen>(
            mock_welcome_view_.get(),
            base::BindRepeating(&WizardController::OnWelcomeScreenExit,
                                base::Unretained(wizard_controller))));

    mock_enrollment_screen_view_ = std::make_unique<MockEnrollmentScreenView>();
    mock_enrollment_screen_ =
        MockScreenExpectLifecycle(std::make_unique<MockEnrollmentScreen>(
            mock_enrollment_screen_view_.get(),
            base::BindRepeating(&WizardController::OnEnrollmentScreenExit,
                                base::Unretained(wizard_controller))));
  }

  OobeScreenId GetFirstScreen() {
    return WizardController::default_controller()->first_screen();
  }

  std::unique_ptr<MockWelcomeView> mock_welcome_view_;
  MockWelcomeScreen* mock_welcome_screen_;

  std::unique_ptr<MockEnrollmentScreenView> mock_enrollment_screen_view_;
  MockEnrollmentScreen* mock_enrollment_screen_;

  std::unique_ptr<base::AutoReset<bool>> branded_build_override_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerOobeResumeTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerOobeResumeTest,
                       PRE_ControlFlowResumeInterruptedOobe) {
  // Switch to the initial screen.
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(NotNull())).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, Show()).Times(1);
  WizardController::default_controller()->AdvanceToScreen(
      WelcomeView::kScreenId);
  CheckCurrentScreen(WelcomeView::kScreenId);
  EXPECT_CALL(*mock_enrollment_screen_view_,
              SetEnrollmentConfig(
                  mock_enrollment_screen_,
                  EnrollmentModeMatches(policy::EnrollmentConfig::MODE_MANUAL)))
      .Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull())).Times(1);

  WizardController::default_controller()->AdvanceToScreen(
      EnrollmentScreenView::kScreenId);
  CheckCurrentScreen(EnrollmentScreenView::kScreenId);
}

IN_PROC_BROWSER_TEST_F(WizardControllerOobeResumeTest,
                       ControlFlowResumeInterruptedOobe) {
  EXPECT_EQ(EnrollmentScreenView::kScreenId.AsId(), GetFirstScreen());
}

class WizardControllerCellularFirstTest : public WizardControllerFlowTest {
 protected:
  WizardControllerCellularFirstTest() {}

  // WizardControllerFlowTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WizardControllerFlowTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kCellularFirst);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerCellularFirstTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerCellularFirstTest, CellularFirstFlow) {
  TestControlFlowMain();
}

class WizardControllerOobeConfigurationTest : public WizardControllerTest {
 protected:
  WizardControllerOobeConfigurationTest() {}

  // WizardControllerTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WizardControllerTest ::SetUpCommandLine(command_line);

    base::FilePath configuration_file;
    ASSERT_TRUE(chromeos::test_utils::GetTestDataPath(
        "oobe_configuration", "non_empty_configuration.json",
        &configuration_file));
    command_line->AppendSwitchPath(chromeos::switches::kFakeOobeConfiguration,
                                   configuration_file);
  }

  // WizardControllerTest:
  void SetUpOnMainThread() override {
    WizardControllerTest::SetUpOnMainThread();

    // Make sure that OOBE is run as an "official" build.
    branded_build_override_ = WizardController::ForceBrandedBuildForTesting();

    // Clear portal list (as it is by default in OOBE).
    NetworkHandler::Get()->network_state_handler()->SetCheckPortalList("");

    mock_welcome_view_ = std::make_unique<MockWelcomeView>();
    mock_welcome_screen_ =
        MockScreenExpectLifecycle(std::make_unique<MockWelcomeScreen>(
            mock_welcome_view_.get(),
            base::BindRepeating(
                &WizardController::OnWelcomeScreenExit,
                base::Unretained(WizardController::default_controller()))));
  }

  void WaitForConfigurationLoaded() {
    base::RunLoop run_loop;
    OOBEConfigurationWaiter waiter;
    const bool ready = waiter.IsConfigurationLoaded(run_loop.QuitClosure());
    if (!ready)
      run_loop.Run();
  }

 protected:
  std::unique_ptr<MockWelcomeView> mock_welcome_view_;
  MockWelcomeScreen* mock_welcome_screen_ = nullptr;
  std::unique_ptr<base::AutoReset<bool>> branded_build_override_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerOobeConfigurationTest);
};

// TODO: fix, test is flaky. https://crbug.com/904841.
IN_PROC_BROWSER_TEST_F(WizardControllerOobeConfigurationTest,
                       DISABLED_ConfigurationIsLoaded) {
  WaitForConfigurationLoaded();
  EXPECT_CALL(*mock_welcome_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(NonEmptyConfiguration()))
      .Times(1);
  WizardController::default_controller()->AdvanceToScreen(
      WelcomeView::kScreenId);
  CheckCurrentScreen(WelcomeView::kScreenId);
}

// TODO(dzhioev): Add test emulating device with wrong HWID.

// TODO(nkostylev): Add test for WebUI accelerators http://crosbug.com/22571

// TODO(merkulova): Add tests for bluetooth HID detection screen variations when
// UI and logic is ready. http://crbug.com/127016

// TODO(khmel): Add tests for ARC OptIn flow.
// http://crbug.com/651144

// TODO(fukino): Add tests for encryption migration UI.
// http://crbug.com/706017

// TODO(updowndota): Add tests for Voice Interaction/Assistant OptIn flow.

// TODO(alemate): Add tests for Sync Consent UI.

// TODO(rsgingerrs): Add tests for Recommend Apps UI.

// TODO(alemate): Add tests for Discover UI.

// TODO(alemate): Add tests for Marketing Opt-In.

// TODO(khorimoto): Add tests for MultiDevice Setup UI.

}  // namespace chromeos
