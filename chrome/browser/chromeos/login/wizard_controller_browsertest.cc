// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/wizard_controller.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
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
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/device_disabled_screen.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/screens/hid_detection_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_arc_terms_of_service_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_demo_preferences_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_demo_setup_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_device_disabled_screen_view.h"
#include "chrome/browser/chromeos/login/screens/mock_enable_debugging_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_eula_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_network_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_update_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_welcome_screen.h"
#include "chrome/browser/chromeos/login/screens/mock_wrong_hwid_screen.h"
#include "chrome/browser/chromeos/login/screens/reset_screen.h"
#include "chrome/browser/chromeos/login/screens/user_image_screen.h"
#include "chrome/browser/chromeos/login/screens/welcome_screen.h"
#include "chrome/browser/chromeos/login/screens/wrong_hwid_screen.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/test/oobe_configuration_waiter.h"
#include "chrome/browser/chromeos/login/test/wizard_in_process_browser_test.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"
#include "chrome/browser/chromeos/policy/auto_enrollment_client.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "chrome/browser/chromeos/policy/fake_auto_enrollment_client.h"
#include "chrome/browser/chromeos/policy/server_backed_device_state.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/stub_install_attributes.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/chromeos_test_utils.h"
#include "chromeos/dbus/dbus_switches.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/dbus/fake_shill_manager_client.h"
#include "chromeos/dbus/fake_system_clock_client.h"
#include "chromeos/geolocation/simple_geolocation_provider.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/settings/timezone_settings.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/system/statistics_provider.h"
#include "chromeos/timezone/timezone_request.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/testing_pref_store.h"
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
using ::testing::Exactly;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::IsNull;
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

}  // namespace

using ::testing::_;

template <class T, class H>
class MockOutShowHide : public T {
 public:
  template <class P>
  explicit MockOutShowHide(P p) : T(p) {}
  template <class P>
  MockOutShowHide(P p, H* view) : T(p, view), view_(view) {}
  template <class P, class Q>
  MockOutShowHide(P p, Q q, H* view) : T(p, q, view), view_(view) {}

  H* view() const { return view_.get(); }

  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());

  void RealShow() { T::Show(); }

  void RealHide() { T::Hide(); }

 private:
  std::unique_ptr<H> view_;
};

#define MOCK(mock_var, screen_name, mocked_class, view_class)  \
  mock_var = new MockOutShowHide<mocked_class, view_class>(    \
      WizardController::default_controller(), new view_class); \
  WizardController::default_controller()                       \
      ->screen_manager()                                       \
      ->screens_[screen_name] = base::WrapUnique(mock_var);    \
  EXPECT_CALL(*mock_var, Show()).Times(0);                     \
  EXPECT_CALL(*mock_var, Hide()).Times(0);

#define MOCK_WITH_DELEGATE(mock_var, screen_name, mocked_class, view_class) \
  mock_var = new MockOutShowHide<mocked_class, view_class>(                 \
      WizardController::default_controller(),                               \
      WizardController::default_controller(), new view_class);              \
  WizardController::default_controller()                                    \
      ->screen_manager()                                                    \
      ->screens_[screen_name] = base::WrapUnique(mock_var);                 \
  EXPECT_CALL(*mock_var, Show()).Times(0);                                  \
  EXPECT_CALL(*mock_var, Hide()).Times(0);

class WizardControllerTest : public WizardInProcessBrowserTest {
 protected:
  WizardControllerTest()
      : WizardInProcessBrowserTest(OobeScreen::SCREEN_TEST_NO_WINDOW) {}
  ~WizardControllerTest() override {}

  void SetUpOnMainThread() override {
    AccessibilityManager::Get()->SetProfileForTest(
        ProfileHelper::GetSigninProfile());
    WizardInProcessBrowserTest::SetUpOnMainThread();
  }

  ErrorScreen* GetErrorScreen() {
    return static_cast<BaseScreenDelegate*>(
               WizardController::default_controller())
        ->GetErrorScreen();
  }

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

  void CheckCurrentScreen(OobeScreen screen) {
    EXPECT_EQ(WizardController::default_controller()->GetScreen(screen),
              WizardController::default_controller()->current_screen());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerTest, SwitchLanguage) {
  ASSERT_TRUE(WizardController::default_controller() != NULL);
  WizardController::default_controller()->AdvanceToScreen(
      OobeScreen::SCREEN_OOBE_WELCOME);

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
  // Overriden from InProcessBrowserTest:
  void SetUpOnMainThread() override {
    WizardControllerTest::SetUpOnMainThread();

    // Make sure that OOBE is run as an "official" build.
    WizardController* wizard_controller =
        WizardController::default_controller();
    wizard_controller->is_official_build_ = true;
    wizard_controller->SetSharedURLLoaderFactoryForTesting(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_));

    // Clear portal list (as it is by default in OOBE).
    NetworkHandler::Get()->network_state_handler()->SetCheckPortalList("");

    // Set up the mocks for all screens.
    mock_welcome_screen_ = new MockWelcomeScreen(
        WizardController::default_controller(),
        WizardController::default_controller(), GetOobeUI()->GetWelcomeView());
    WizardController::default_controller()
        ->screen_manager()
        ->screens_[OobeScreen::SCREEN_OOBE_WELCOME]
        .reset(mock_welcome_screen_);
    EXPECT_CALL(*mock_welcome_screen_, Show()).Times(0);
    EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(0);

    MOCK(mock_network_screen_, OobeScreen::SCREEN_OOBE_NETWORK,
         MockNetworkScreen, MockNetworkScreenView);
    MOCK(mock_update_screen_, OobeScreen::SCREEN_OOBE_UPDATE, MockUpdateScreen,
         MockUpdateView);
    MOCK_WITH_DELEGATE(mock_eula_screen_, OobeScreen::SCREEN_OOBE_EULA,
                       MockEulaScreen, MockEulaView);
    MOCK(mock_enrollment_screen_, OobeScreen::SCREEN_OOBE_ENROLLMENT,
         MockEnrollmentScreen, MockEnrollmentScreenView);
    MOCK(mock_auto_enrollment_check_screen_,
         OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK,
         MockAutoEnrollmentCheckScreen, MockAutoEnrollmentCheckScreenView);
    MOCK(mock_wrong_hwid_screen_, OobeScreen::SCREEN_WRONG_HWID,
         MockWrongHWIDScreen, MockWrongHWIDScreenView);
    MOCK(mock_enable_debugging_screen_,
         OobeScreen::SCREEN_OOBE_ENABLE_DEBUGGING, MockEnableDebuggingScreen,
         MockEnableDebuggingScreenView);
    MOCK(mock_demo_setup_screen_, OobeScreen::SCREEN_OOBE_DEMO_SETUP,
         MockDemoSetupScreen, MockDemoSetupScreenView);
    MOCK(mock_demo_preferences_screen_,
         OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES, MockDemoPreferencesScreen,
         MockDemoPreferencesScreenView);
    MOCK(mock_arc_terms_of_service_screen_,
         OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE, MockArcTermsOfServiceScreen,
         MockArcTermsOfServiceScreenView);
    device_disabled_screen_view_.reset(new MockDeviceDisabledScreenView);
    wizard_controller->screen_manager()
        ->screens_[OobeScreen::SCREEN_DEVICE_DISABLED] =
        std::make_unique<DeviceDisabledScreen>(
            wizard_controller, device_disabled_screen_view_.get());
    EXPECT_CALL(*device_disabled_screen_view_, Show()).Times(0);

    // Switch to the initial screen.
    EXPECT_EQ(NULL, wizard_controller->current_screen());
    EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(NotNull(), _)).Times(1);
    EXPECT_CALL(*mock_welcome_screen_, Show()).Times(1);
    wizard_controller->AdvanceToScreen(OobeScreen::SCREEN_OOBE_WELCOME);
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

  void OnExit(ScreenExitCode exit_code) {
    WizardController::default_controller()->OnExit(exit_code);
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
    WizardController::default_controller()->screen_manager()->screens_.erase(
        OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  }

  void TestControlFlowMain() {
    CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);

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

    // Check visibility of the header bar.
    ASSERT_FALSE(JSExecuteBooleanExpression("$('login-header-bar').hidden"));

    EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
    EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull(), _)).Times(1);
    EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
    OnExit(ScreenExitCode::NETWORK_CONNECTED);

    CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);

    // Header bar should still be visible.
    ASSERT_FALSE(JSExecuteBooleanExpression("$('login-header-bar').hidden"));

    EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
    EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
    EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
    // Enable TimeZone resolve
    InitTimezoneResolver();
    OnExit(ScreenExitCode::EULA_ACCEPTED);
    EXPECT_TRUE(GetGeolocationProvider());

    // Let update screen smooth time process (time = 0ms).
    content::RunAllPendingInMessageLoop();

    CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
    EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
    EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
    OnExit(ScreenExitCode::UPDATE_INSTALLED);

    CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
    EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(0);
    EXPECT_CALL(*mock_eula_screen_, Show()).Times(0);
    OnExit(ScreenExitCode::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

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

  MockWelcomeScreen* mock_welcome_screen_;  // Unowned ptr.
  MockOutShowHide<MockNetworkScreen, MockNetworkScreenView>*
      mock_network_screen_;
  MockOutShowHide<MockUpdateScreen, MockUpdateView>* mock_update_screen_;
  MockOutShowHide<MockEulaScreen, MockEulaView>* mock_eula_screen_;
  MockOutShowHide<MockEnrollmentScreen, MockEnrollmentScreenView>*
      mock_enrollment_screen_;
  MockOutShowHide<MockAutoEnrollmentCheckScreen,
                  MockAutoEnrollmentCheckScreenView>*
      mock_auto_enrollment_check_screen_;
  MockOutShowHide<MockWrongHWIDScreen, MockWrongHWIDScreenView>*
      mock_wrong_hwid_screen_;
  MockOutShowHide<MockEnableDebuggingScreen, MockEnableDebuggingScreenView>*
      mock_enable_debugging_screen_;
  MockOutShowHide<MockDemoSetupScreen, MockDemoSetupScreenView>*
      mock_demo_setup_screen_;
  MockOutShowHide<MockDemoPreferencesScreen, MockDemoPreferencesScreenView>*
      mock_demo_preferences_screen_;

  MockOutShowHide<MockArcTermsOfServiceScreen, MockArcTermsOfServiceScreenView>*
      mock_arc_terms_of_service_screen_;

  std::unique_ptr<MockDeviceDisabledScreenView> device_disabled_screen_view_;

 private:
  NetworkPortalDetectorTestImpl* network_portal_detector_;

  network::TestURLLoaderFactory test_url_loader_factory_;

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
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(0);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull(), _)).Times(1);
  OnExit(ScreenExitCode::WELCOME_CONTINUED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::UPDATE_ERROR_UPDATING);

  CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(0);
  OnExit(ScreenExitCode::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
}

// This test verifies that if WizardController fails to apply a critical update
// before the OOBE is marked complete, it goes back the network selection
// screen and thus prevents the user from proceeding to log in.
IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest,
                       ControlFlowErrorUpdateCriticalUpdate) {
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(0);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull(), _)).Times(1);
  OnExit(ScreenExitCode::WELCOME_CONTINUED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(0);  // last transition
  OnExit(ScreenExitCode::UPDATE_ERROR_UPDATING_CRITICAL_UPDATE);
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
}

// Tests that WizardController goes back to network selection if the user
// declined to accept update over a cellular network.
IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest,
                       ControlFlowErrorUpdateRejectedOverCellular) {
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(0);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull(), _)).Times(1);
  OnExit(ScreenExitCode::WELCOME_CONTINUED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(0);  // last transition
  OnExit(ScreenExitCode::UPDATE_REJECT_OVER_CELLULAR);
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest, ControlFlowSkipUpdateEnroll) {
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(0);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull(), _)).Times(1);
  OnExit(ScreenExitCode::WELCOME_CONTINUED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(0);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(0);
  WizardController::default_controller()->SkipUpdateEnrollAfterEula();
  EXPECT_CALL(*mock_enrollment_screen_->view(),
              SetParameters(
                  mock_enrollment_screen_,
                  EnrollmentModeMatches(policy::EnrollmentConfig::MODE_MANUAL)))
      .Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::EULA_ACCEPTED);
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Hide()).Times(0);
  OnExit(ScreenExitCode::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_ENROLLMENT);
  EXPECT_EQ("ethernet,wifi,cellular", NetworkHandler::Get()
                                          ->network_state_handler()
                                          ->GetCheckPortalListForTest());
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest, ControlFlowEulaDeclined) {
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull(), _)).Times(1);
  OnExit(ScreenExitCode::WELCOME_CONTINUED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(0);
  OnExit(ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  OnExit(ScreenExitCode::EULA_BACK);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest,
                       ControlFlowEnrollmentCompleted) {
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(0);
  EXPECT_CALL(*mock_enrollment_screen_->view(),
              SetParameters(
                  mock_enrollment_screen_,
                  EnrollmentModeMatches(policy::EnrollmentConfig::MODE_MANUAL)))
      .Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull(), _)).Times(1);

  WizardController::default_controller()->AdvanceToScreen(
      OobeScreen::SCREEN_OOBE_ENROLLMENT);
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_ENROLLMENT);
  OnExit(ScreenExitCode::ENTERPRISE_ENROLLMENT_COMPLETED);

  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
}

IN_PROC_BROWSER_TEST_F(WizardControllerFlowTest,
                       ControlFlowWrongHWIDScreenFromLogin) {
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);

  LoginDisplayHost::default_host()->StartSignInScreen(LoginScreenContext());
  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
  ExistingUserController::current_controller()->ShowWrongHWIDScreen();

  CheckCurrentScreen(OobeScreen::SCREEN_WRONG_HWID);

  // After warning is skipped, user returns to sign-in screen.
  // And this destroys WizardController.
  OnExit(ScreenExitCode::WRONG_HWID_WARNING_SKIPPED);
  EXPECT_FALSE(ExistingUserController::current_controller() == NULL);
}

// This parameterized test class extends WizardControllerFlowTest to verify how
// WizardController behaves if it fails to apply an update after the OOBE is
// marked complete.
class WizardControllerErrorUpdateAfterCompletedOobeTest
    : public WizardControllerFlowTest,
      public testing::WithParamInterface<ScreenExitCode> {
 protected:
  WizardControllerErrorUpdateAfterCompletedOobeTest() = default;

  void SetUpOnMainThread() override {
    StartupUtils::MarkOobeCompleted();  // Pretend OOBE was complete.
    WizardControllerFlowTest::SetUpOnMainThread();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerErrorUpdateAfterCompletedOobeTest);
};

// This test verifies that if WizardController fails to apply an update, either
// critical or non-critical, after the OOBE is marked complete, it allows the
// user to proceed to log in.
IN_PROC_BROWSER_TEST_P(WizardControllerErrorUpdateAfterCompletedOobeTest,
                       ControlFlowErrorUpdate) {
  const ScreenExitCode update_screen_exit_code = GetParam();
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(0);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(0);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull(), _)).Times(1);
  OnExit(ScreenExitCode::WELCOME_CONTINUED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(update_screen_exit_code);

  CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(0);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(0);
  OnExit(ScreenExitCode::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

  EXPECT_NE(nullptr, ExistingUserController::current_controller());
}

INSTANTIATE_TEST_CASE_P(
    WizardControllerErrorUpdateAfterCompletedOobe,
    WizardControllerErrorUpdateAfterCompletedOobeTest,
    testing::Values(ScreenExitCode::UPDATE_ERROR_UPDATING,
                    ScreenExitCode::UPDATE_ERROR_UPDATING_CRITICAL_UPDATE));

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

  void SetUpOnMainThread() override {
    WizardControllerFlowTest::SetUpOnMainThread();

    histogram_tester_ = std::make_unique<base::HistogramTester>();
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
  ScopedStubInstallAttributes test_install_attributes_{
      StubInstallAttributes::CreateUnset()};

  std::unique_ptr<base::HistogramTester> histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(WizardControllerDeviceStateTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerDeviceStateTest,
                       ControlFlowNoForcedReEnrollmentOnFirstBoot) {
  fake_statistics_provider_.ClearMachineStatistic(system::kActivateDateKey);
  EXPECT_NE(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT,
            auto_enrollment_controller()->state());

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull(), _)).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::WELCOME_CONTINUED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::UPDATE_INSTALLED);

  CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  mock_auto_enrollment_check_screen_->RealShow();
  EXPECT_EQ(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT,
            auto_enrollment_controller()->state());
}

IN_PROC_BROWSER_TEST_F(WizardControllerDeviceStateTest,
                       ControlFlowDeviceDisabled) {
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull(), _)).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::WELCOME_CONTINUED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::UPDATE_INSTALLED);

  CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  mock_auto_enrollment_check_screen_->RealShow();

  // Wait for auto-enrollment controller to encounter the connection error.
  WaitForAutoEnrollmentState(policy::AUTO_ENROLLMENT_STATE_CONNECTION_ERROR);

  // The error screen shows up if device state could not be retrieved.
  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_EQ(GetErrorScreen(),
            WizardController::default_controller()->current_screen());
  base::DictionaryValue device_state;
  device_state.SetString(policy::kDeviceStateRestoreMode,
                         policy::kDeviceStateRestoreModeDisabled);
  device_state.SetString(policy::kDeviceStateDisabledMessage, kDisabledMessage);
  g_browser_process->local_state()->Set(prefs::kServerBackedDeviceState,
                                        device_state);
  EXPECT_CALL(*device_disabled_screen_view_, UpdateMessage(kDisabledMessage))
      .Times(1);
  EXPECT_CALL(*device_disabled_screen_view_, Show()).Times(1);
  OnExit(ScreenExitCode::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

  ResetAutoEnrollmentCheckScreen();

  // Make sure the device disabled screen is shown.
  CheckCurrentScreen(OobeScreen::SCREEN_DEVICE_DISABLED);

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

// Tets the control flow for Forced Re-Enrollment. First, a connection error
// occurs, leading to a network error screen. On the network error screen, the
// test verifies that the user may enter a guest session if FRE was not
// explicitly required, and that the user may not enter a guest session if FRE
// was explicitly required. Then, a retyr is performed and FRE indicates that
// the device should be enrolled.
IN_PROC_BROWSER_TEST_P(WizardControllerDeviceStateExplicitRequirementTest,
                       ControlFlowForcedReEnrollment) {
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull(), _)).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::WELCOME_CONTINUED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::UPDATE_INSTALLED);

  CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  mock_auto_enrollment_check_screen_->RealShow();

  // Wait for auto-enrollment controller to encounter the connection error.
  WaitForAutoEnrollmentState(policy::AUTO_ENROLLMENT_STATE_CONNECTION_ERROR);

  // The error screen shows up if there's no auto-enrollment decision.
  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_EQ(GetErrorScreen(),
            WizardController::default_controller()->current_screen());

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

  base::DictionaryValue device_state;
  device_state.SetString(policy::kDeviceStateRestoreMode,
                         policy::kDeviceStateRestoreModeReEnrollmentEnforced);
  g_browser_process->local_state()->Set(prefs::kServerBackedDeviceState,
                                        device_state);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_->view(),
              SetParameters(mock_enrollment_screen_,
                            EnrollmentModeMatches(
                                policy::EnrollmentConfig::MODE_SERVER_FORCED)))
      .Times(1);
  OnExit(ScreenExitCode::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

  ResetAutoEnrollmentCheckScreen();

  // Make sure enterprise enrollment page shows up.
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_ENROLLMENT);
  OnExit(ScreenExitCode::ENTERPRISE_ENROLLMENT_COMPLETED);

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

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull(), _)).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::WELCOME_CONTINUED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::UPDATE_INSTALLED);

  CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
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
    EXPECT_EQ(GetErrorScreen(),
              WizardController::default_controller()->current_screen());

    WaitUntilJSIsReady();
    constexpr char guest_session_link_display[] =
        "window.getComputedStyle($('error-guest-signin-fix-network'))."
        "display";
    // Check that guest sign-in is not allowed on the network error screen
    // (because the check_enrollment VPD key was set to "1", making FRE
    // explicitly required).
    EXPECT_EQ("none", JSExecuteStringExpression(guest_session_link_display));

    base::DictionaryValue device_state;
    device_state.SetString(policy::kDeviceStateRestoreMode,
                           policy::kDeviceStateRestoreModeReEnrollmentEnforced);
    g_browser_process->local_state()->Set(prefs::kServerBackedDeviceState,
                                          device_state);
    EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
    EXPECT_CALL(
        *mock_enrollment_screen_->view(),
        SetParameters(mock_enrollment_screen_,
                      EnrollmentModeMatches(
                          policy::EnrollmentConfig::MODE_SERVER_FORCED)))
        .Times(1);
    fake_auto_enrollment_client->SetState(
        policy::AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
    OnExit(ScreenExitCode::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

    ResetAutoEnrollmentCheckScreen();

    // Make sure enterprise enrollment page shows up.
    CheckCurrentScreen(OobeScreen::SCREEN_OOBE_ENROLLMENT);
    OnExit(ScreenExitCode::ENTERPRISE_ENROLLMENT_COMPLETED);

    EXPECT_TRUE(StartupUtils::IsOobeCompleted());
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
  }
}

INSTANTIATE_TEST_CASE_P(WizardControllerDeviceStateExplicitRequirement,
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

  void SetUpInProcessBrowserTestFixture() override {
    auto system_clock_client = std::make_unique<FakeSystemClockClient>();
    system_clock_client_ = system_clock_client.get();
    DBusThreadManager::GetSetterForTesting()->SetSystemClockClient(
        std::move(system_clock_client));
  }

  void SetUpOnMainThread() override {
    WizardControllerDeviceStateTest::SetUpOnMainThread();

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
    WizardControllerDeviceStateTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        switches::kEnterpriseEnableInitialEnrollment,
        chromeos::AutoEnrollmentController::kInitialEnrollmentAlways);
  }

  FakeSystemClockClient* system_clock_client() { return system_clock_client_; }

 private:
  // Unowned pointer - owned by DBusThreadManager.
  FakeSystemClockClient* system_clock_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(
      WizardControllerDeviceStateWithInitialEnrollmentTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerDeviceStateWithInitialEnrollmentTest,
                       ControlFlowInitialEnrollment) {
  fake_statistics_provider_.ClearMachineStatistic(system::kActivateDateKey);
  fake_statistics_provider_.SetMachineStatistic(
      system::kRlzEmbargoEndDateKey,
      GenerateEmbargoEndDate(-15 /* days_offset */));
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull(), _)).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::WELCOME_CONTINUED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::UPDATE_INSTALLED);

  CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  mock_auto_enrollment_check_screen_->RealShow();

  // Wait for auto-enrollment controller to encounter the connection error.
  WaitForAutoEnrollmentState(policy::AUTO_ENROLLMENT_STATE_CONNECTION_ERROR);

  // The error screen shows up if there's no auto-enrollment decision.
  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_EQ(GetErrorScreen(),
            WizardController::default_controller()->current_screen());
  base::DictionaryValue device_state;
  device_state.SetString(policy::kDeviceStateRestoreMode,
                         policy::kDeviceStateRestoreModeReEnrollmentEnforced);
  g_browser_process->local_state()->Set(prefs::kServerBackedDeviceState,
                                        device_state);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_->view(),
              SetParameters(mock_enrollment_screen_,
                            EnrollmentModeMatches(
                                policy::EnrollmentConfig::MODE_SERVER_FORCED)))
      .Times(1);
  OnExit(ScreenExitCode::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

  ResetAutoEnrollmentCheckScreen();

  // Make sure enterprise enrollment page shows up.
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_ENROLLMENT);
  OnExit(ScreenExitCode::ENTERPRISE_ENROLLMENT_COMPLETED);

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
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull(), _)).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::WELCOME_CONTINUED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::UPDATE_INSTALLED);

  CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
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
  EXPECT_EQ(GetErrorScreen(),
            WizardController::default_controller()->current_screen());

  WaitUntilJSIsReady();
  constexpr char guest_session_link_display[] =
      "window.getComputedStyle($('error-guest-signin-fix-network'))."
      "display";
  // Check that guest sign-in is allowed on the network error screen for initial
  // enrollment.
  EXPECT_EQ("block", JSExecuteStringExpression(guest_session_link_display));

  base::DictionaryValue device_state;
  device_state.SetString(policy::kDeviceStateRestoreMode,
                         policy::kDeviceStateRestoreModeReEnrollmentEnforced);
  g_browser_process->local_state()->Set(prefs::kServerBackedDeviceState,
                                        device_state);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_->view(),
              SetParameters(mock_enrollment_screen_,
                            EnrollmentModeMatches(
                                policy::EnrollmentConfig::MODE_SERVER_FORCED)))
      .Times(1);
  fake_auto_enrollment_client->SetState(
      policy::AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
  OnExit(ScreenExitCode::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

  ResetAutoEnrollmentCheckScreen();

  // Make sure enterprise enrollment page shows up.
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_ENROLLMENT);
  OnExit(ScreenExitCode::ENTERPRISE_ENROLLMENT_COMPLETED);

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());

  histogram_tester()->ExpectUniqueSample(
      "Enterprise.InitialEnrollmentRequirement", 0 /* Required */,
      1 /* count */);
}

IN_PROC_BROWSER_TEST_F(WizardControllerDeviceStateWithInitialEnrollmentTest,
                       ControlFlowNoInitialEnrollmentDuringEmbargoPeriod) {
  system_clock_client()->set_network_synchronized(true);
  system_clock_client()->NotifyObserversSystemClockUpdated();

  fake_statistics_provider_.ClearMachineStatistic(system::kActivateDateKey);
  fake_statistics_provider_.SetMachineStatistic(
      system::kRlzEmbargoEndDateKey,
      GenerateEmbargoEndDate(1 /* days_offset */));
  EXPECT_NE(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT,
            auto_enrollment_controller()->state());

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull(), _)).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::WELCOME_CONTINUED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::UPDATE_INSTALLED);

  CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
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

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull(), _)).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::WELCOME_CONTINUED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::UPDATE_INSTALLED);

  CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  mock_auto_enrollment_check_screen_->RealShow();
  EXPECT_EQ(AutoEnrollmentController::AutoEnrollmentCheckType::kNone,
            auto_enrollment_controller()->auto_enrollment_check_type());

  system_clock_client()->set_network_synchronized(true);
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

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull(), _)).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::WELCOME_CONTINUED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  task_runner->RunUntilIdle();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::UPDATE_INSTALLED);

  CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
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

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull(), _)).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::WELCOME_CONTINUED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::UPDATE_INSTALLED);

  CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  mock_auto_enrollment_check_screen_->RealShow();
  EXPECT_EQ(AutoEnrollmentController::AutoEnrollmentCheckType::kNone,
            auto_enrollment_controller()->auto_enrollment_check_type());

  // Simulate that the clock moved forward, passing the embargo period, by
  // moving the embargo period back in time.
  fake_statistics_provider_.SetMachineStatistic(
      system::kRlzEmbargoEndDateKey,
      GenerateEmbargoEndDate(-1 /* days_offset */));
  base::DictionaryValue device_state;
  device_state.SetString(policy::kDeviceStateRestoreMode,
                         policy::kDeviceStateRestoreModeReEnrollmentEnforced);
  g_browser_process->local_state()->Set(prefs::kServerBackedDeviceState,
                                        device_state);

  system_clock_client()->set_network_synchronized(true);
  system_clock_client()->NotifyObserversSystemClockUpdated();

  policy::FakeAutoEnrollmentClient* fake_auto_enrollment_client =
      fake_auto_enrollment_client_factory.WaitAutoEnrollmentClientCreated();

  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);

  EXPECT_CALL(*mock_enrollment_screen_->view(),
              SetParameters(mock_enrollment_screen_,
                            EnrollmentModeMatches(
                                policy::EnrollmentConfig::MODE_SERVER_FORCED)))
      .Times(1);
  OnExit(ScreenExitCode::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);
  ResetAutoEnrollmentCheckScreen();

  fake_auto_enrollment_client->SetState(
      policy::AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);

  // Make sure enterprise enrollment page shows up.
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_ENROLLMENT);
  OnExit(ScreenExitCode::ENTERPRISE_ENROLLMENT_COMPLETED);
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

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull(), _)).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::WELCOME_CONTINUED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::UPDATE_INSTALLED);

  CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  mock_auto_enrollment_check_screen_->RealShow();
  EXPECT_EQ(policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT,
            auto_enrollment_controller()->state());
}

class WizardControllerBrokenLocalStateTest : public WizardControllerTest {
 protected:
  WizardControllerBrokenLocalStateTest() : fake_session_manager_client_(NULL) {}

  ~WizardControllerBrokenLocalStateTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    WizardControllerTest::SetUpInProcessBrowserTestFixture();

    fake_session_manager_client_ = new FakeSessionManagerClient;
    DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        std::unique_ptr<SessionManagerClient>(fake_session_manager_client_));
  }

  void SetUpOnMainThread() override {
    PrefServiceFactory factory;
    factory.set_user_prefs(base::MakeRefCounted<PrefStoreStub>());
    local_state_ = factory.Create(new PrefRegistrySimple());
    WizardController::set_local_state_for_testing(local_state_.get());

    WizardControllerTest::SetUpOnMainThread();

    // Make sure that OOBE is run as an "official" build.
    WizardController::default_controller()->is_official_build_ = true;
  }

  FakeSessionManagerClient* fake_session_manager_client() const {
    return fake_session_manager_client_;
  }

 private:
  std::unique_ptr<PrefService> local_state_;
  FakeSessionManagerClient* fake_session_manager_client_;

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
  ASSERT_TRUE(JSExecuteBooleanExpression("$('login-header-bar').hidden"));

  // Emulates user click on the "Restart and Powerwash" button.
  ASSERT_EQ(0, fake_session_manager_client()->start_device_wipe_call_count());
  ASSERT_TRUE(content::ExecuteScript(
      GetWebContents(), "$('error-message-md-powerwash-button').click();"));
  ASSERT_EQ(1, fake_session_manager_client()->start_device_wipe_call_count());
}

class WizardControllerProxyAuthOnSigninTest : public WizardControllerTest {
 protected:
  WizardControllerProxyAuthOnSigninTest()
      : proxy_server_(net::SpawnedTestServer::TYPE_BASIC_AUTH_PROXY,
                      base::FilePath()) {}
  ~WizardControllerProxyAuthOnSigninTest() override {}

  // Overridden from WizardControllerTest:
  void SetUp() override {
    ASSERT_TRUE(proxy_server_.Start());
    WizardControllerTest::SetUp();
  }

  void SetUpOnMainThread() override {
    WizardControllerTest::SetUpOnMainThread();
    WizardController::default_controller()->AdvanceToScreen(
        OobeScreen::SCREEN_OOBE_WELCOME);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(::switches::kProxyServer,
                                    proxy_server_.host_port_pair().ToString());
  }

  net::SpawnedTestServer& proxy_server() { return proxy_server_; }

 private:
  net::SpawnedTestServer proxy_server_;

  DISALLOW_COPY_AND_ASSIGN(WizardControllerProxyAuthOnSigninTest);
};

// Disabled, see https://crbug.com/504928.
IN_PROC_BROWSER_TEST_F(WizardControllerProxyAuthOnSigninTest,
                       DISABLED_ProxyAuthDialogOnSigninScreen) {
  content::WindowedNotificationObserver auth_needed_waiter(
      chrome::NOTIFICATION_AUTH_NEEDED,
      content::NotificationService::AllSources());

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);

  LoginDisplayHost::default_host()->StartSignInScreen(LoginScreenContext());
  auth_needed_waiter.Wait();
}

class WizardControllerKioskFlowTest : public WizardControllerFlowTest {
 protected:
  WizardControllerKioskFlowTest() {}

  // Overridden from InProcessBrowserTest:
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
  EXPECT_CALL(*mock_enrollment_screen_->view(),
              SetParameters(mock_enrollment_screen_,
                            EnrollmentModeMatches(
                                policy::EnrollmentConfig::MODE_LOCAL_FORCED)))
      .Times(1);
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull(), _)).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::WELCOME_CONTINUED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::UPDATE_INSTALLED);

  CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());

  // Make sure enterprise enrollment page shows up right after update screen.
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_ENROLLMENT);
  OnExit(ScreenExitCode::ENTERPRISE_ENROLLMENT_COMPLETED);

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
}

IN_PROC_BROWSER_TEST_F(WizardControllerKioskFlowTest,
                       ControlFlowEnrollmentBack) {
  EXPECT_CALL(*mock_enrollment_screen_->view(),
              SetParameters(mock_enrollment_screen_,
                            EnrollmentModeMatches(
                                policy::EnrollmentConfig::MODE_LOCAL_FORCED)))
      .Times(1);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull(), _)).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::WELCOME_CONTINUED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  OnExit(ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, StartNetworkCheck()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::EULA_ACCEPTED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::UPDATE_INSTALLED);

  CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Hide()).Times(1);
  OnExit(ScreenExitCode::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());

  // Make sure enterprise enrollment page shows up right after update screen.
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_ENROLLMENT);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);
  OnExit(ScreenExitCode::ENTERPRISE_ENROLLMENT_BACK);

  CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
}

class WizardControllerEnableDebuggingTest : public WizardControllerFlowTest {
 protected:
  WizardControllerEnableDebuggingTest() {}

  // Overridden from InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WizardControllerFlowTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(chromeos::switches::kSystemDevMode);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerEnableDebuggingTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerEnableDebuggingTest,
                       ShowAndCancelEnableDebugging) {
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  WaitUntilJSIsReady();

  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull(), _)).Times(1);
  EXPECT_CALL(*mock_enable_debugging_screen_, Show()).Times(1);

  ASSERT_TRUE(
      JSExecute("chrome.send('login.WelcomeScreen.userActed', "
                "['connect-debugging-features']);"));

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_ENABLE_DEBUGGING);
  EXPECT_CALL(*mock_enable_debugging_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(NotNull(), _)).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, Show()).Times(1);

  OnExit(ScreenExitCode::ENABLE_DEBUGGING_CANCELED);

  // Let update screen smooth time process (time = 0ms).
  content::RunAllPendingInMessageLoop();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
}

class WizardControllerDemoSetupTest : public WizardControllerFlowTest {
 protected:
  WizardControllerDemoSetupTest() = default;
  ~WizardControllerDemoSetupTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    WizardControllerFlowTest::SetUpOnMainThread();
    testing::Mock::VerifyAndClearExpectations(mock_welcome_screen_);
  }

  template <class T, class H>
  void SkipToScreen(OobeScreen screen, MockOutShowHide<T, H>* screen_mock) {
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
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
  WaitUntilJSIsReady();

  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull(), _)).Times(1);
  EXPECT_CALL(*mock_demo_preferences_screen_, Show()).Times(1);

  WizardController::default_controller()->StartDemoModeSetup();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_demo_preferences_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);

  OnExit(ScreenExitCode::DEMO_MODE_PREFERENCES_CONTINUED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);

  OnExit(ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_arc_terms_of_service_screen_, Show()).Times(1);

  OnExit(ScreenExitCode::EULA_ACCEPTED);

  CheckCurrentScreen(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_arc_terms_of_service_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);

  OnExit(ScreenExitCode::ARC_TERMS_OF_SERVICE_ACCEPTED);

  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);

  OnExit(ScreenExitCode::UPDATE_INSTALLED);

  CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_demo_setup_screen_, Show()).Times(1);

  OnExit(ScreenExitCode::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_DEMO_SETUP);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  OnExit(ScreenExitCode::DEMO_MODE_SETUP_FINISHED);

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
  EXPECT_TRUE(ExistingUserController::current_controller());
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
}

IN_PROC_BROWSER_TEST_F(WizardControllerDemoSetupTest,
                       OfflineDemoSetupFlowFinished) {
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
  WaitUntilJSIsReady();

  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull(), _)).Times(1);
  EXPECT_CALL(*mock_demo_preferences_screen_, Show()).Times(1);

  WizardController::default_controller()->StartDemoModeSetup();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_demo_preferences_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);

  OnExit(ScreenExitCode::DEMO_MODE_PREFERENCES_CONTINUED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);

  OnExit(ScreenExitCode::NETWORK_OFFLINE_DEMO_SETUP);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_arc_terms_of_service_screen_, Show()).Times(1);

  OnExit(ScreenExitCode::EULA_ACCEPTED);

  CheckCurrentScreen(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_arc_terms_of_service_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_demo_setup_screen_, Show()).Times(1);

  OnExit(ScreenExitCode::ARC_TERMS_OF_SERVICE_ACCEPTED);

  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_DEMO_SETUP);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  OnExit(ScreenExitCode::DEMO_MODE_SETUP_FINISHED);

  EXPECT_TRUE(StartupUtils::IsOobeCompleted());
  EXPECT_TRUE(ExistingUserController::current_controller());
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
}

IN_PROC_BROWSER_TEST_F(WizardControllerDemoSetupTest, DemoSetupCanceled) {
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
  WaitUntilJSIsReady();

  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull(), _)).Times(1);
  EXPECT_CALL(*mock_demo_preferences_screen_, Show()).Times(1);

  WizardController::default_controller()->StartDemoModeSetup();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_demo_preferences_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);

  OnExit(ScreenExitCode::DEMO_MODE_PREFERENCES_CONTINUED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);

  OnExit(ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_arc_terms_of_service_screen_, Show()).Times(1);

  OnExit(ScreenExitCode::EULA_ACCEPTED);

  CheckCurrentScreen(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_arc_terms_of_service_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);

  OnExit(ScreenExitCode::ARC_TERMS_OF_SERVICE_ACCEPTED);

  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);

  OnExit(ScreenExitCode::UPDATE_INSTALLED);

  CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_demo_setup_screen_, Show()).Times(1);

  OnExit(ScreenExitCode::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_DEMO_SETUP);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_demo_setup_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(NotNull(), _)).Times(1);

  OnExit(ScreenExitCode::DEMO_MODE_SETUP_CANCELED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
}

IN_PROC_BROWSER_TEST_F(WizardControllerDemoSetupTest, DemoPreferencesCanceled) {
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
  WaitUntilJSIsReady();
  SkipToScreen(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES,
               mock_demo_preferences_screen_);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_demo_preferences_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, Show()).Times(1);

  OnExit(ScreenExitCode::DEMO_MODE_PREFERENCES_CANCELED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
}

IN_PROC_BROWSER_TEST_F(WizardControllerDemoSetupTest, NetworkBackPressed) {
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
  WaitUntilJSIsReady();
  SkipToScreen(OobeScreen::SCREEN_OOBE_NETWORK, mock_network_screen_);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_demo_preferences_screen_, Show()).Times(1);

  OnExit(ScreenExitCode::NETWORK_BACK);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
}

IN_PROC_BROWSER_TEST_F(WizardControllerDemoSetupTest, EulaBackPressed) {
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
  WaitUntilJSIsReady();
  SkipToScreen(OobeScreen::SCREEN_OOBE_EULA, mock_eula_screen_);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);

  OnExit(ScreenExitCode::EULA_BACK);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
}

IN_PROC_BROWSER_TEST_F(WizardControllerDemoSetupTest, ArcTosBackPressed) {
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
  WaitUntilJSIsReady();

  // User cannot go to ARC ToS screen without accepting eula - simulate that.
  StartupUtils::MarkEulaAccepted();
  SkipToScreen(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE,
               mock_arc_terms_of_service_screen_);

  CheckCurrentScreen(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_arc_terms_of_service_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);

  OnExit(ScreenExitCode::ARC_TERMS_OF_SERVICE_BACK);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
}

class WizardControllerDemoSetupDeviceDisabledTest
    : public WizardControllerDeviceStateTest {
 protected:
  WizardControllerDemoSetupDeviceDisabledTest() = default;
  ~WizardControllerDemoSetupDeviceDisabledTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    WizardControllerDeviceStateTest::SetUpOnMainThread();
    testing::Mock::VerifyAndClearExpectations(mock_welcome_screen_);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerDemoSetupDeviceDisabledTest);
};

// Flaky https://crbug.com/894384.
IN_PROC_BROWSER_TEST_F(WizardControllerDemoSetupDeviceDisabledTest,
                       DISABLED_OnlineDemoSetup) {
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
  WaitUntilJSIsReady();

  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull(), _)).Times(1);
  EXPECT_CALL(*mock_demo_preferences_screen_, Show()).Times(1);

  WizardController::default_controller()->StartDemoModeSetup();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_demo_preferences_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_network_screen_, Show()).Times(1);

  OnExit(ScreenExitCode::DEMO_MODE_PREFERENCES_CONTINUED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_NETWORK);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_network_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_eula_screen_, Show()).Times(1);

  OnExit(ScreenExitCode::NETWORK_CONNECTED);

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_EULA);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_eula_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_arc_terms_of_service_screen_, Show()).Times(1);

  OnExit(ScreenExitCode::EULA_ACCEPTED);

  CheckCurrentScreen(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_arc_terms_of_service_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_update_screen_, Show()).Times(1);

  OnExit(ScreenExitCode::ARC_TERMS_OF_SERVICE_ACCEPTED);

  base::RunLoop().RunUntilIdle();

  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_UPDATE);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_update_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Show()).Times(1);

  OnExit(ScreenExitCode::UPDATE_INSTALLED);

  CheckCurrentScreen(OobeScreen::SCREEN_AUTO_ENROLLMENT_CHECK);
  EXPECT_TRUE(DemoSetupController::IsOobeDemoSetupFlowInProgress());

  EXPECT_CALL(*mock_auto_enrollment_check_screen_, Hide()).Times(1);

  mock_auto_enrollment_check_screen_->RealShow();

  // Wait for auto-enrollment controller to encounter the connection error.
  WaitForAutoEnrollmentState(policy::AUTO_ENROLLMENT_STATE_CONNECTION_ERROR);

  // The error screen shows up if device state could not be retrieved.
  EXPECT_EQ(GetErrorScreen(),
            WizardController::default_controller()->current_screen());
  base::DictionaryValue device_state;
  device_state.SetString(policy::kDeviceStateRestoreMode,
                         policy::kDeviceStateRestoreModeDisabled);
  device_state.SetString(policy::kDeviceStateDisabledMessage, kDisabledMessage);
  g_browser_process->local_state()->Set(prefs::kServerBackedDeviceState,
                                        device_state);

  EXPECT_CALL(*device_disabled_screen_view_, Show()).Times(1);
  OnExit(ScreenExitCode::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);

  ResetAutoEnrollmentCheckScreen();
  CheckCurrentScreen(OobeScreen::SCREEN_DEVICE_DISABLED);

  EXPECT_FALSE(StartupUtils::IsOobeCompleted());
  EXPECT_FALSE(DemoSetupController::IsOobeDemoSetupFlowInProgress());
}

class WizardControllerOobeResumeTest : public WizardControllerTest {
 protected:
  WizardControllerOobeResumeTest() {}
  // Overriden from InProcessBrowserTest:
  void SetUpOnMainThread() override {
    WizardControllerTest::SetUpOnMainThread();

    // Make sure that OOBE is run as an "official" build.
    WizardController::default_controller()->is_official_build_ = true;

    // Clear portal list (as it is by default in OOBE).
    NetworkHandler::Get()->network_state_handler()->SetCheckPortalList("");

    // Set up the mocks for all screens.
    MOCK_WITH_DELEGATE(mock_welcome_screen_, OobeScreen::SCREEN_OOBE_WELCOME,
                       MockWelcomeScreen, MockWelcomeView);
    MOCK(mock_enrollment_screen_, OobeScreen::SCREEN_OOBE_ENROLLMENT,
         MockEnrollmentScreen, MockEnrollmentScreenView);
  }

  void OnExit(ScreenExitCode exit_code) {
    WizardController::default_controller()->OnExit(exit_code);
  }

  OobeScreen GetFirstScreen() {
    return WizardController::default_controller()->first_screen();
  }

  MockOutShowHide<MockWelcomeScreen, MockWelcomeView>* mock_welcome_screen_;
  MockOutShowHide<MockEnrollmentScreen, MockEnrollmentScreenView>*
      mock_enrollment_screen_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerOobeResumeTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerOobeResumeTest,
                       PRE_ControlFlowResumeInterruptedOobe) {
  // Switch to the initial screen.
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(NotNull(), _)).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, Show()).Times(1);
  WizardController::default_controller()->AdvanceToScreen(
      OobeScreen::SCREEN_OOBE_WELCOME);
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
  EXPECT_CALL(*mock_enrollment_screen_->view(),
              SetParameters(
                  mock_enrollment_screen_,
                  EnrollmentModeMatches(policy::EnrollmentConfig::MODE_MANUAL)))
      .Times(1);
  EXPECT_CALL(*mock_enrollment_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, Hide()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_, SetConfiguration(IsNull(), _)).Times(1);

  WizardController::default_controller()->AdvanceToScreen(
      OobeScreen::SCREEN_OOBE_ENROLLMENT);
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_ENROLLMENT);
}

IN_PROC_BROWSER_TEST_F(WizardControllerOobeResumeTest,
                       ControlFlowResumeInterruptedOobe) {
  EXPECT_EQ(OobeScreen::SCREEN_OOBE_ENROLLMENT, GetFirstScreen());
}

class WizardControllerCellularFirstTest : public WizardControllerFlowTest {
 protected:
  WizardControllerCellularFirstTest() {}

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

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WizardControllerTest ::SetUpCommandLine(command_line);

    base::FilePath configuration_file;
    ASSERT_TRUE(chromeos::test_utils::GetTestDataPath(
        "oobe_configuration", "non_empty_configuration.json",
        &configuration_file));
    command_line->AppendSwitchPath(chromeos::switches::kFakeOobeConfiguration,
                                   configuration_file);
  }

  // Overriden from InProcessBrowserTest:
  void SetUpOnMainThread() override {
    WizardControllerTest::SetUpOnMainThread();

    // Make sure that OOBE is run as an "official" build.
    WizardController* wizard_controller =
        WizardController::default_controller();
    wizard_controller->is_official_build_ = true;

    // Clear portal list (as it is by default in OOBE).
    NetworkHandler::Get()->network_state_handler()->SetCheckPortalList("");

    MOCK_WITH_DELEGATE(mock_welcome_screen_, OobeScreen::SCREEN_OOBE_WELCOME,
                       MockWelcomeScreen, MockWelcomeView);
  }

  void WaitForConfigurationLoaded() {
    base::RunLoop run_loop;
    OOBEConfigurationWaiter waiter;
    const bool ready = waiter.IsConfigurationLoaded(run_loop.QuitClosure());
    if (!ready)
      run_loop.Run();
  }

 protected:
  MockOutShowHide<MockWelcomeScreen, MockWelcomeView>* mock_welcome_screen_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WizardControllerOobeConfigurationTest);
};

IN_PROC_BROWSER_TEST_F(WizardControllerOobeConfigurationTest,
                       ConfigurationIsLoaded) {
  WaitForConfigurationLoaded();
  EXPECT_CALL(*mock_welcome_screen_, Show()).Times(1);
  EXPECT_CALL(*mock_welcome_screen_,
              SetConfiguration(NonEmptyConfiguration(), _))
      .Times(1);
  WizardController::default_controller()->AdvanceToScreen(
      OobeScreen::SCREEN_OOBE_WELCOME);
  CheckCurrentScreen(OobeScreen::SCREEN_OOBE_WELCOME);
}

// TODO(dzhioev): Add test emulating device with wrong HWID.

// TODO(nkostylev): Add test for WebUI accelerators http://crosbug.com/22571

// TODO(merkulova): Add tests for bluetooth HID detection screen variations when
// UI and logic is ready. http://crbug.com/127016

// TODO(dzhioev): Add tests for controller/host pairing flow.
// http://crbug.com/375191

// TODO(khmel): Add tests for ARC OptIn flow.
// http://crbug.com/651144

// TODO(fukino): Add tests for encryption migration UI.
// http://crbug.com/706017

// TODO(updowndota): Add tests for Voice Interaction/Assistant OptIn flow.

// TODO(alemate): Add tests for Sync Consent UI.

// TODO(rsgingerrs): Add tests for Recommend Apps UI.

// TODO(alemate): Add tests for Discover UI.

// TODO(xiaoyinh): Add tests for Fingerprint Setup UI.

// TODO(alemate): Add tests for Marketing Opt-In.

// TODO(khorimoto): Add tests for MultiDevice Setup UI.

static_assert(static_cast<int>(ScreenExitCode::EXIT_CODES_COUNT) == 50,
              "tests for new control flow are missing");

}  // namespace chromeos
