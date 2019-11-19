// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/assistant_optin_flow_screen.h"

#include <memory>
#include <set>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/assistant_optin_flow_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/services/assistant/assistant_settings_manager.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/services/assistant/public/features.h"
#include "chromeos/services/assistant/public/mojom/settings.mojom.h"
#include "chromeos/services/assistant/public/proto/settings_ui.pb.h"
#include "chromeos/services/assistant/service.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace chromeos {

namespace {

constexpr char kTestUser[] = "test-user1@gmail.com";

constexpr char kAssistantConsentToken[] = "consent_token";
constexpr char kAssistantUiAuditKey[] = "ui_audit_key";

chromeos::OobeUI* GetOobeUI() {
  auto* host = chromeos::LoginDisplayHost::default_host();
  return host ? host->GetOobeUI() : nullptr;
}

class FakeAssistantSettings
    : public chromeos::assistant::AssistantSettingsManager {
 public:
  // Flags to configure GetSettings response.
  static constexpr int CONSENT_UI_FLAGS_NONE = 0;
  static constexpr int CONSENT_UI_FLAG_SKIP_ACTIVITY_CONTROL = 1;
  static constexpr int CONSENT_UI_FLAG_SKIP_THIRD_PARTY_DISCLOSURE = 1 << 1;
  static constexpr int CONSENT_UI_FLAG_ASK_EMAIL_OPT_IN = 1 << 2;
  static constexpr int CONSENT_UI_FLAG_WAA_DISABLED_BY_POLICY = 1 << 3;
  static constexpr int CONSENT_UI_FLAG_ASSISTANT_DISABLED_BY_POLICY = 1 << 4;

  enum class SpeakerIdEnrollmentMode {
    // On speaker enrollment request, the client will be notified that the
    // enrollment is done immediately.
    IMMEDIATE,
    // Speaker enrollment requests will not respond immediately, test will have
    // to run through enrollment responses by calling
    // AdvanceSpeakerIdEnrollment().
    STEP_BY_STEP
  };

  enum class OptIn {
    ACTIVITY_CONTROL,
    EMAIL,
  };

  FakeAssistantSettings() {
    chromeos::assistant::Service::OverrideSettingsManagerForTesting(this);
  }

  ~FakeAssistantSettings() override {
    chromeos::assistant::Service::OverrideSettingsManagerForTesting(nullptr);
  }

  void set_consent_ui_flags(int flags) { consent_ui_flags_ = flags; }

  void set_speaker_id_enrollment_mode(SpeakerIdEnrollmentMode mode) {
    speaker_id_enrollment_mode_ = mode;
  }

  const std::set<OptIn>& collected_optins() const { return collected_optins_; }

  // Advances speaker ID enrollment to the next state.
  // Returns whether the speaker ID enrollment state changed, which amounts to
  // whether the speaker ID enrollment is currently in progress.
  bool AdvanceSpeakerIdEnrollmentState() {
    switch (speaker_id_enrollment_state_) {
      case SpeakerIdEnrollmentState::IDLE:
        return false;
      case SpeakerIdEnrollmentState::REQUESTED:
        speaker_id_enrollment_state_ = SpeakerIdEnrollmentState::LISTENING;
        speaker_id_enrollment_client_->OnListeningHotword();
        return true;
      case SpeakerIdEnrollmentState::LISTENING:
        speaker_id_enrollment_state_ = SpeakerIdEnrollmentState::PROCESSING;
        speaker_id_enrollment_client_->OnProcessingHotword();
        return true;
      case SpeakerIdEnrollmentState::PROCESSING:
        ++processed_hotwords_;
        if (processed_hotwords_ == 4) {
          speaker_id_enrollment_state_ = SpeakerIdEnrollmentState::IDLE;
          speaker_id_enrollment_client_->OnSpeakerIdEnrollmentDone();
        } else {
          speaker_id_enrollment_state_ = SpeakerIdEnrollmentState::LISTENING;
          speaker_id_enrollment_client_->OnListeningHotword();
        }
        return true;
    }
    return false;
  }

  bool IsSpeakerIdEnrollmentActive() const {
    return speaker_id_enrollment_state_ != SpeakerIdEnrollmentState::IDLE;
  }

  void FailSpeakerIdEnrollment() {
    ASSERT_NE(speaker_id_enrollment_state_, SpeakerIdEnrollmentState::IDLE);
    std::move(speaker_id_enrollment_client_)->OnSpeakerIdEnrollmentFailure();
    processed_hotwords_ = 0;
    speaker_id_enrollment_state_ = SpeakerIdEnrollmentState::IDLE;
  }

  void Flush() { receivers_.FlushForTesting(); }

  // chromeos::assistant::AssistantSettingsManager:
  void BindReceiver(mojo::PendingReceiver<
                    chromeos::assistant::mojom::AssistantSettingsManager>
                        receiver) override {
    receivers_.Add(this, std::move(receiver));
  }

  // chromeos::assistant::mojom::AssistantSettingsManager:
  void GetSettings(const std::string& selector,
                   GetSettingsCallback callback) override {
    chromeos::assistant::SettingsUiSelector selector_proto;
    ASSERT_TRUE(selector_proto.ParseFromString(selector));
    EXPECT_FALSE(selector_proto.about_me_settings());
    EXPECT_TRUE(selector_proto.has_consent_flow_ui_selector());
    EXPECT_EQ(assistant::ActivityControlSettingsUiSelector::
                  ASSISTANT_SUW_ONBOARDING_ON_CHROME_OS,
              selector_proto.consent_flow_ui_selector().flow_id());

    chromeos::assistant::SettingsUi settings_ui;
    auto* gaia_user_context_ui = settings_ui.mutable_gaia_user_context_ui();
    gaia_user_context_ui->set_is_gaia_user(true);
    gaia_user_context_ui->set_waa_disabled_by_dasher_domain(
        (consent_ui_flags_ & CONSENT_UI_FLAG_WAA_DISABLED_BY_POLICY));
    gaia_user_context_ui->set_assistant_disabled_by_dasher_domain(
        (consent_ui_flags_ & CONSENT_UI_FLAG_ASSISTANT_DISABLED_BY_POLICY));

    auto* consent_flow_ui = settings_ui.mutable_consent_flow_ui();
    consent_flow_ui->set_consent_status(
        chromeos::assistant::ConsentFlowUi_ConsentStatus_ASK_FOR_CONSENT);
    consent_flow_ui->mutable_consent_ui()->set_accept_button_text("OK");
    consent_flow_ui->mutable_consent_ui()->set_reject_button_text(
        "No, thank you");

    if (!(consent_ui_flags_ & CONSENT_UI_FLAG_SKIP_ACTIVITY_CONTROL)) {
      auto* activity_control_ui =
          consent_flow_ui->mutable_consent_ui()->mutable_activity_control_ui();
      activity_control_ui->set_consent_token(kAssistantConsentToken);
      activity_control_ui->set_ui_audit_key(kAssistantUiAuditKey);
      activity_control_ui->set_title("Title");
      activity_control_ui->set_identity(kTestUser);
      activity_control_ui->add_intro_text_paragraph();
      activity_control_ui->set_intro_text_paragraph(0, "Here's an intro");
      activity_control_ui->add_footer_paragraph();
      activity_control_ui->set_footer_paragraph(0, "A footer");
      auto* setting = activity_control_ui->add_setting_zippy();
      setting->set_title("Cool feature");
      setting->add_description_paragraph();
      setting->set_description_paragraph(0, "But needs consent");
      setting->add_additional_info_paragraph();
      setting->set_additional_info_paragraph(0, "And it's really cool");
      setting->set_icon_uri("assistant_icon");
    }

    if (!(consent_ui_flags_ & CONSENT_UI_FLAG_SKIP_THIRD_PARTY_DISCLOSURE)) {
      auto* third_party_disclosure = consent_flow_ui->mutable_consent_ui()
                                         ->mutable_third_party_disclosure_ui();
      third_party_disclosure->set_title("Third parties");
      third_party_disclosure->set_button_continue("Continue");
      auto* disclosure = third_party_disclosure->add_disclosures();
      disclosure->set_title("Third party org");
      disclosure->add_description_paragraph();
      disclosure->set_description_paragraph(0, "They are not us");
      disclosure->add_additional_info_paragraph();
      disclosure->set_additional_info_paragraph(0, "But work with us");
      disclosure->set_icon_uri("disclosure_icon");
    }

    if (selector_proto.email_opt_in() &&
        (consent_ui_flags_ & CONSENT_UI_FLAG_ASK_EMAIL_OPT_IN)) {
      auto* email_opt_in = settings_ui.mutable_email_opt_in_ui();
      email_opt_in->set_title("Receive email upfates");
      email_opt_in->set_description("It might be useful");
      email_opt_in->set_legal_text("And you can opt out");
      email_opt_in->set_default_enabled(false);
      email_opt_in->set_icon_uri("fake icon url");
      email_opt_in->set_accept_button_text("I'm in");
    }

    std::string message;
    EXPECT_TRUE(settings_ui.SerializeToString(&message));
    std::move(callback).Run(message);
  }

  void UpdateSettings(const std::string& update,
                      UpdateSettingsCallback callback) override {
    chromeos::assistant::SettingsUiUpdate update_proto;
    ASSERT_TRUE(update_proto.ParseFromString(update));
    EXPECT_FALSE(update_proto.has_about_me_settings_update());
    EXPECT_FALSE(update_proto.has_assistant_device_settings_update());

    chromeos::assistant::SettingsUiUpdateResult update_result;
    if (update_proto.has_consent_flow_ui_update()) {
      EXPECT_EQ(kAssistantConsentToken,
                update_proto.consent_flow_ui_update().consent_token());
      EXPECT_FALSE(
          update_proto.consent_flow_ui_update().saw_third_party_disclosure());
      EXPECT_EQ(assistant::ActivityControlSettingsUiSelector::
                    ASSISTANT_SUW_ONBOARDING_ON_CHROME_OS,
                update_proto.consent_flow_ui_update().flow_id());
      collected_optins_.insert(OptIn::ACTIVITY_CONTROL);
      update_result.mutable_consent_flow_update_result()->set_update_status(
          assistant::ConsentFlowUiUpdateResult::SUCCESS);
    }
    if (update_proto.has_email_opt_in_update()) {
      if (update_proto.email_opt_in_update().email_opt_in_update_state() ==
          assistant::EmailOptInUpdate::OPT_IN) {
        collected_optins_.insert(OptIn::EMAIL);
      }

      update_result.mutable_email_opt_in_update_result()->set_update_status(
          assistant::EmailOptInUpdateResult::SUCCESS);
    }

    std::string message;
    EXPECT_TRUE(update_result.SerializeToString(&message));
    std::move(callback).Run(message);
  }

  void StartSpeakerIdEnrollment(
      bool skip_cloud_enrollment,
      mojo::PendingRemote<chromeos::assistant::mojom::SpeakerIdEnrollmentClient>
          client) override {
    if (speaker_id_enrollment_mode_ == SpeakerIdEnrollmentMode::IMMEDIATE) {
      mojo::Remote<chromeos::assistant::mojom::SpeakerIdEnrollmentClient>(
          std::move(client))
          ->OnSpeakerIdEnrollmentDone();
      return;
    }
    ASSERT_FALSE(speaker_id_enrollment_client_);
    processed_hotwords_ = 0;
    speaker_id_enrollment_client_.Bind(std::move(client));
    speaker_id_enrollment_state_ = SpeakerIdEnrollmentState::REQUESTED;
  }

  void StopSpeakerIdEnrollment(
      StopSpeakerIdEnrollmentCallback callback) override {
    processed_hotwords_ = 0;
    speaker_id_enrollment_state_ = SpeakerIdEnrollmentState::IDLE;
    speaker_id_enrollment_client_.reset();
    std::move(callback).Run();
  }

  void SyncSpeakerIdEnrollmentStatus() override {}

 private:
  enum class SpeakerIdEnrollmentState {
    IDLE,
    REQUESTED,
    LISTENING,
    PROCESSING
  };

  mojo::ReceiverSet<chromeos::assistant::mojom::AssistantSettingsManager>
      receivers_;

  // The service test config:
  int consent_ui_flags_ = CONSENT_UI_FLAGS_NONE;
  SpeakerIdEnrollmentMode speaker_id_enrollment_mode_ =
      SpeakerIdEnrollmentMode::IMMEDIATE;

  // Speaker ID enrollment state:
  SpeakerIdEnrollmentState speaker_id_enrollment_state_ =
      SpeakerIdEnrollmentState::IDLE;
  mojo::Remote<assistant::mojom::SpeakerIdEnrollmentClient>
      speaker_id_enrollment_client_;
  int processed_hotwords_ = 0;

  // Set of opt ins given by the user.
  std::set<OptIn> collected_optins_;

  DISALLOW_COPY_AND_ASSIGN(FakeAssistantSettings);
};

}  // namespace

class AssistantOptInFlowTest : public MixinBasedInProcessBrowserTest {
 public:
  AssistantOptInFlowTest() = default;
  ~AssistantOptInFlowTest() override = default;

  void SetUp() override {
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    https_server_.ServeFilesFromDirectory(test_data_dir);

    https_server_.RegisterRequestHandler(base::BindRepeating(
        &AssistantOptInFlowTest::HandleRequest, base::Unretained(this)));

    // Don't spin up the IO thread yet since no threads are allowed while
    // spawning sandbox host process. See crbug.com/322732.
    ASSERT_TRUE(https_server_.InitializeAndListen());

    MixinBasedInProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
    // This prevents assistant setup flow dialog popping up immediately on user
    // start - the test will show a different dialog once the setup is done.
    command_line->AppendSwitch(switches::kOobeSkipPostLogin);
  }
  void SetUpOnMainThread() override {
    https_server_.StartAcceptingConnections();

    login_manager_.LoginAndWaitForActiveSession(
        LoginManagerMixin::CreateDefaultUserContext(test_user_));

    assistant_settings_ = std::make_unique<FakeAssistantSettings>();

    ShowLoginWizard(OobeScreen::SCREEN_TEST_NO_WINDOW);

    WizardController::default_controller()
        ->screen_manager()
        ->DeleteScreenForTesting(AssistantOptInFlowScreenView::kScreenId);
    auto assistant_optin_flow_screen =
        std::make_unique<AssistantOptInFlowScreen>(
            GetOobeUI()->GetView<AssistantOptInFlowScreenHandler>(),
            base::BindRepeating(&AssistantOptInFlowTest::HandleScreenExit,
                                base::Unretained(this)));
    assistant_optin_flow_screen_ = assistant_optin_flow_screen.get();
    WizardController::default_controller()
        ->screen_manager()
        ->SetScreenForTesting(std::move(assistant_optin_flow_screen));

    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(https_server_.ShutdownAndWaitUntilComplete());
    assistant_settings_.reset();
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

  // Waits for the OOBE UI to complete initialization, and overrides:
  // *   the assistant value prop webview URL with the one provided by embedded
  //     https proxy.
  // *   the timeout delay for sending done user action from voice match screen.
  void SetUpAssistantScreensForTest() {
    base::RunLoop oobe_ready_waiter;
    if (!GetOobeUI()->IsJSReady(oobe_ready_waiter.QuitClosure())) {
      oobe_ready_waiter.Run();
    }

    std::string url_template =
        https_server_.GetURL("/test_assistant/$/value_prop.html").spec();
    test::OobeJS().Evaluate(
        test::GetOobeElementPath({"assistant-optin-flow-card", "value-prop"}) +
        ".setUrlTemplateForTesting('" + url_template + "')");
    test::OobeJS().Evaluate(
        test::GetOobeElementPath({"assistant-optin-flow-card", "voice-match"}) +
        ".setDoneActionDelayForTesting(0)");
  }

  // Waits for the button specified by IDs in |button_path| to become enabled,
  // and then taps it.
  void TapWhenEnabled(std::initializer_list<base::StringPiece> button_path) {
    test::OobeJS().CreateEnabledWaiter(true, button_path)->Wait();
    test::OobeJS().TapOnPath(button_path);
  }

  void WaitForAssistantScreen(const std::string& screen) {
    test::OobeJS()
        .CreateVisibilityWaiter(true, {"assistant-optin-flow-card", screen})
        ->Wait();
  }

  bool ElementHasAttribute(std::initializer_list<base::StringPiece> element,
                           const std::string& attribute) {
    return test::OobeJS().GetBool(test::GetOobeElementPath(element) +
                                  ".getAttribute('" + attribute + "')");
  }

  void WaitForElementAttribute(std::initializer_list<base::StringPiece> element,
                               const std::string& attribute) {
    test::OobeJS()
        .CreateWaiter(test::GetOobeElementPath(element) + ".getAttribute('" +
                      attribute + "')")
        ->Wait();
  }

  void ExpectCollectedOptIns(
      const std::set<FakeAssistantSettings::OptIn>& opt_ins) {
    assistant_settings_->Flush();
    EXPECT_EQ(opt_ins, assistant_settings_->collected_optins());
  }

  void WaitForScreenExit() {
    if (screen_exited_)
      return;

    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  AssistantOptInFlowScreen* assistant_optin_flow_screen_;

  std::unique_ptr<FakeAssistantSettings> assistant_settings_;

  // If set, HandleRequest will return an error for the next value prop URL
  // request..
  bool fail_next_value_prop_url_request_ = false;

 private:
  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    auto response = std::make_unique<BasicHttpResponse>();
    if (request.relative_url != "/test_assistant/en_us/value_prop.html" ||
        fail_next_value_prop_url_request_) {
      fail_next_value_prop_url_request_ = false;
      response->set_code(net::HTTP_NOT_FOUND);
    } else {
      response->set_code(net::HTTP_OK);
      response->set_content("Test content");
      response->set_content_type("text/plain");
    }
    return std::move(response);
  }

  void HandleScreenExit() {
    ASSERT_FALSE(screen_exited_);
    screen_exited_ = true;
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  bool screen_exited_ = false;
  base::OnceClosure screen_exit_callback_;

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};

  const LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId(kTestUser, kTestUser)};
  LoginManagerMixin login_manager_{&mixin_host_, {test_user_}};
};

// Test times out in debug builds. crbug.com/1022021
#if !defined(NDEBUG)
#define MAYBE_AssistantOptInFlowTest DISABLED_AssistantOptInFlowTest
class DISABLED_AssistantOptInFlowTest : public AssistantOptInFlowTest {};
#else
#define MAYBE_AssistantOptInFlowTest AssistantOptInFlowTest
#endif

IN_PROC_BROWSER_TEST_F(MAYBE_AssistantOptInFlowTest, Basic) {
  ash::AssistantState::Get()->NotifyStatusChanged(
      ash::mojom::AssistantState::READY);

  SetUpAssistantScreensForTest();
  assistant_optin_flow_screen_->Show();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  WaitForAssistantScreen("value-prop");
  TapWhenEnabled({"assistant-optin-flow-card", "value-prop", "next-button"});

  WaitForAssistantScreen("third-party");
  TapWhenEnabled({"assistant-optin-flow-card", "third-party", "next-button"});

  WaitForAssistantScreen("voice-match");
  TapWhenEnabled({"assistant-optin-flow-card", "voice-match", "agree-button"});

  WaitForAssistantScreen("get-more");
  test::OobeJS().ExpectVisiblePath(
      {"assistant-optin-flow-card", "get-more", "toggle-context"});

  TapWhenEnabled({"assistant-optin-flow-card", "get-more", "next-button"});

  WaitForScreenExit();

  ExpectCollectedOptIns({FakeAssistantSettings::OptIn::ACTIVITY_CONTROL});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_EQ(assistant::prefs::ConsentStatus::kActivityControlAccepted,
            prefs->GetInteger(assistant::prefs::kAssistantConsentStatus));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
}

IN_PROC_BROWSER_TEST_F(MAYBE_AssistantOptInFlowTest, DisableScreenContext) {
  ash::AssistantState::Get()->NotifyStatusChanged(
      ash::mojom::AssistantState::READY);

  SetUpAssistantScreensForTest();
  assistant_optin_flow_screen_->Show();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  WaitForAssistantScreen("value-prop");
  TapWhenEnabled({"assistant-optin-flow-card", "value-prop", "next-button"});

  WaitForAssistantScreen("third-party");
  TapWhenEnabled({"assistant-optin-flow-card", "third-party", "next-button"});

  WaitForAssistantScreen("voice-match");
  TapWhenEnabled({"assistant-optin-flow-card", "voice-match", "agree-button"});

  WaitForAssistantScreen("get-more");

  std::initializer_list<base::StringPiece> context_toggle = {
      "assistant-optin-flow-card", "get-more", "toggle-context"};
  test::OobeJS().ExpectVisiblePath(context_toggle);
  test::OobeJS().Evaluate(test::GetOobeElementPath(context_toggle) +
                          ".click()");

  TapWhenEnabled({"assistant-optin-flow-card", "get-more", "next-button"});

  WaitForScreenExit();

  ExpectCollectedOptIns({FakeAssistantSettings::OptIn::ACTIVITY_CONTROL});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_EQ(assistant::prefs::ConsentStatus::kActivityControlAccepted,
            prefs->GetInteger(assistant::prefs::kAssistantConsentStatus));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_FALSE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
}

IN_PROC_BROWSER_TEST_F(MAYBE_AssistantOptInFlowTest,
                       AssistantStateUpdateAfterShow) {
  SetUpAssistantScreensForTest();
  assistant_optin_flow_screen_->Show();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  // Value prop screen will not be sohwn until it receives assistant settings
  // config, which is blocked on the Assistant state becomes READY state.
  test::OobeJS().ExpectHiddenPath({"assistant-optin-flow-card", "value-prop"});
  test::OobeJS().ExpectVisiblePath({"assistant-optin-flow-card", "loading"});

  ash::AssistantState::Get()->NotifyStatusChanged(
      ash::mojom::AssistantState::READY);

  WaitForAssistantScreen("value-prop");
  TapWhenEnabled({"assistant-optin-flow-card", "value-prop", "next-button"});

  WaitForAssistantScreen("third-party");
  TapWhenEnabled({"assistant-optin-flow-card", "third-party", "next-button"});

  WaitForAssistantScreen("voice-match");
  TapWhenEnabled({"assistant-optin-flow-card", "voice-match", "agree-button"});

  WaitForAssistantScreen("get-more");
  TapWhenEnabled({"assistant-optin-flow-card", "get-more", "next-button"});

  WaitForScreenExit();

  ExpectCollectedOptIns({FakeAssistantSettings::OptIn::ACTIVITY_CONTROL});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_EQ(assistant::prefs::ConsentStatus::kActivityControlAccepted,
            prefs->GetInteger(assistant::prefs::kAssistantConsentStatus));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
}

IN_PROC_BROWSER_TEST_F(MAYBE_AssistantOptInFlowTest, RetryOnWebviewLoadFail) {
  SetUpAssistantScreensForTest();
  fail_next_value_prop_url_request_ = true;

  assistant_optin_flow_screen_->Show();

  ash::AssistantState::Get()->NotifyStatusChanged(
      ash::mojom::AssistantState::READY);

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  // Value prop webview requests are set to fail - loading screen should display
  // an error and an option to retry the request.
  WaitForAssistantScreen("loading");
  TapWhenEnabled({"assistant-optin-flow-card", "loading", "retry-button"});

  WaitForAssistantScreen("value-prop");
  TapWhenEnabled({"assistant-optin-flow-card", "value-prop", "next-button"});

  WaitForAssistantScreen("third-party");
  TapWhenEnabled({"assistant-optin-flow-card", "third-party", "next-button"});

  WaitForAssistantScreen("voice-match");
  TapWhenEnabled({"assistant-optin-flow-card", "voice-match", "agree-button"});

  WaitForAssistantScreen("get-more");
  TapWhenEnabled({"assistant-optin-flow-card", "get-more", "next-button"});

  WaitForScreenExit();

  ExpectCollectedOptIns({FakeAssistantSettings::OptIn::ACTIVITY_CONTROL});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_EQ(assistant::prefs::ConsentStatus::kActivityControlAccepted,
            prefs->GetInteger(assistant::prefs::kAssistantConsentStatus));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
}

IN_PROC_BROWSER_TEST_F(MAYBE_AssistantOptInFlowTest, RejectValueProp) {
  SetUpAssistantScreensForTest();
  ash::AssistantState::Get()->NotifyStatusChanged(
      ash::mojom::AssistantState::READY);

  assistant_optin_flow_screen_->Show();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  WaitForAssistantScreen("value-prop");
  TapWhenEnabled({"assistant-optin-flow-card", "value-prop", "skip-button"});

  WaitForScreenExit();

  ExpectCollectedOptIns({});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_EQ(assistant::prefs::ConsentStatus::kUnknown,
            prefs->GetInteger(assistant::prefs::kAssistantConsentStatus));
  EXPECT_FALSE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_FALSE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
}

IN_PROC_BROWSER_TEST_F(MAYBE_AssistantOptInFlowTest, AskEmailOptIn_NotChecked) {
  assistant_settings_->set_consent_ui_flags(
      FakeAssistantSettings::CONSENT_UI_FLAG_ASK_EMAIL_OPT_IN);
  ash::AssistantState::Get()->NotifyStatusChanged(
      ash::mojom::AssistantState::READY);

  SetUpAssistantScreensForTest();
  assistant_optin_flow_screen_->Show();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  WaitForAssistantScreen("value-prop");
  TapWhenEnabled({"assistant-optin-flow-card", "value-prop", "next-button"});

  WaitForAssistantScreen("third-party");
  TapWhenEnabled({"assistant-optin-flow-card", "third-party", "next-button"});

  WaitForAssistantScreen("voice-match");
  TapWhenEnabled({"assistant-optin-flow-card", "voice-match", "agree-button"});

  WaitForAssistantScreen("get-more");
  test::OobeJS().ExpectVisiblePath(
      {"assistant-optin-flow-card", "get-more", "toggle-email"});
  test::OobeJS().ExpectEnabledPath(
      {"assistant-optin-flow-card", "get-more", "toggle-email"});

  // Complete flow without checking the email opt-in toggle.
  TapWhenEnabled({"assistant-optin-flow-card", "get-more", "next-button"});

  WaitForScreenExit();

  ExpectCollectedOptIns({FakeAssistantSettings::OptIn::ACTIVITY_CONTROL});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_EQ(assistant::prefs::ConsentStatus::kActivityControlAccepted,
            prefs->GetInteger(assistant::prefs::kAssistantConsentStatus));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
}

IN_PROC_BROWSER_TEST_F(MAYBE_AssistantOptInFlowTest, AskEmailOptIn_Accepted) {
  assistant_settings_->set_consent_ui_flags(
      FakeAssistantSettings::CONSENT_UI_FLAG_ASK_EMAIL_OPT_IN);
  ash::AssistantState::Get()->NotifyStatusChanged(
      ash::mojom::AssistantState::READY);

  SetUpAssistantScreensForTest();
  assistant_optin_flow_screen_->Show();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  WaitForAssistantScreen("value-prop");
  TapWhenEnabled({"assistant-optin-flow-card", "value-prop", "next-button"});

  WaitForAssistantScreen("third-party");
  TapWhenEnabled({"assistant-optin-flow-card", "third-party", "next-button"});

  WaitForAssistantScreen("voice-match");
  TapWhenEnabled({"assistant-optin-flow-card", "voice-match", "agree-button"});

  WaitForAssistantScreen("get-more");
  test::OobeJS().ExpectVisiblePath(
      {"assistant-optin-flow-card", "get-more", "toggle-email"});

  std::initializer_list<base::StringPiece> email_toggle = {
      "assistant-optin-flow-card", "get-more", "toggle-email"};
  test::OobeJS().ExpectVisiblePath(email_toggle);
  test::OobeJS().Evaluate(test::GetOobeElementPath(email_toggle) + ".click()");

  TapWhenEnabled({"assistant-optin-flow-card", "get-more", "next-button"});

  WaitForScreenExit();

  ExpectCollectedOptIns({FakeAssistantSettings::OptIn::ACTIVITY_CONTROL,
                         FakeAssistantSettings::OptIn::EMAIL});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_EQ(assistant::prefs::ConsentStatus::kActivityControlAccepted,
            prefs->GetInteger(assistant::prefs::kAssistantConsentStatus));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
}

IN_PROC_BROWSER_TEST_F(MAYBE_AssistantOptInFlowTest, SkipShowingValueProp) {
  assistant_settings_->set_consent_ui_flags(
      FakeAssistantSettings::CONSENT_UI_FLAG_SKIP_ACTIVITY_CONTROL);

  SetUpAssistantScreensForTest();
  ash::AssistantState::Get()->NotifyStatusChanged(
      ash::mojom::AssistantState::READY);

  assistant_optin_flow_screen_->Show();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  WaitForAssistantScreen("third-party");
  TapWhenEnabled({"assistant-optin-flow-card", "third-party", "next-button"});

  WaitForAssistantScreen("voice-match");
  TapWhenEnabled({"assistant-optin-flow-card", "voice-match", "agree-button"});

  WaitForAssistantScreen("get-more");
  TapWhenEnabled({"assistant-optin-flow-card", "get-more", "next-button"});

  WaitForScreenExit();

  ExpectCollectedOptIns({});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_EQ(assistant::prefs::ConsentStatus::kActivityControlAccepted,
            prefs->GetInteger(assistant::prefs::kAssistantConsentStatus));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
}

IN_PROC_BROWSER_TEST_F(MAYBE_AssistantOptInFlowTest,
                       SkipShowingValuePropAndThirdPartyDisclosure) {
  assistant_settings_->set_consent_ui_flags(
      FakeAssistantSettings::CONSENT_UI_FLAG_SKIP_ACTIVITY_CONTROL |
      FakeAssistantSettings::CONSENT_UI_FLAG_SKIP_THIRD_PARTY_DISCLOSURE);

  SetUpAssistantScreensForTest();
  ash::AssistantState::Get()->NotifyStatusChanged(
      ash::mojom::AssistantState::READY);

  assistant_optin_flow_screen_->Show();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  WaitForAssistantScreen("voice-match");
  TapWhenEnabled({"assistant-optin-flow-card", "voice-match", "agree-button"});

  WaitForAssistantScreen("get-more");
  TapWhenEnabled({"assistant-optin-flow-card", "get-more", "next-button"});

  WaitForScreenExit();

  ExpectCollectedOptIns({});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_EQ(assistant::prefs::ConsentStatus::kActivityControlAccepted,
            prefs->GetInteger(assistant::prefs::kAssistantConsentStatus));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
}

IN_PROC_BROWSER_TEST_F(MAYBE_AssistantOptInFlowTest, SpeakerIdEnrollment) {
  assistant_settings_->set_consent_ui_flags(
      FakeAssistantSettings::CONSENT_UI_FLAG_SKIP_ACTIVITY_CONTROL |
      FakeAssistantSettings::CONSENT_UI_FLAG_SKIP_THIRD_PARTY_DISCLOSURE);
  assistant_settings_->set_speaker_id_enrollment_mode(
      FakeAssistantSettings::SpeakerIdEnrollmentMode::STEP_BY_STEP);

  SetUpAssistantScreensForTest();
  ash::AssistantState::Get()->NotifyStatusChanged(
      ash::mojom::AssistantState::READY);

  assistant_optin_flow_screen_->Show();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  WaitForAssistantScreen("voice-match");
  TapWhenEnabled({"assistant-optin-flow-card", "voice-match", "agree-button"});

  assistant_settings_->Flush();
  ASSERT_TRUE(assistant_settings_->AdvanceSpeakerIdEnrollmentState());
  WaitForElementAttribute(
      {"assistant-optin-flow-card", "voice-match", "voice-entry-0"}, "active");
  test::OobeJS().ExpectVisiblePath(
      {"assistant-optin-flow-card", "voice-match", "later-button"});

  assistant_settings_->Flush();
  ASSERT_TRUE(assistant_settings_->AdvanceSpeakerIdEnrollmentState());
  WaitForElementAttribute(
      {"assistant-optin-flow-card", "voice-match", "voice-entry-0"},
      "completed");
  test::OobeJS().ExpectVisiblePath(
      {"assistant-optin-flow-card", "voice-match", "later-button"});

  assistant_settings_->Flush();
  ASSERT_TRUE(assistant_settings_->AdvanceSpeakerIdEnrollmentState());
  WaitForElementAttribute(
      {"assistant-optin-flow-card", "voice-match", "voice-entry-1"}, "active");
  test::OobeJS().ExpectVisiblePath(
      {"assistant-optin-flow-card", "voice-match", "later-button"});

  ASSERT_TRUE(assistant_settings_->AdvanceSpeakerIdEnrollmentState());
  WaitForElementAttribute(
      {"assistant-optin-flow-card", "voice-match", "voice-entry-1"},
      "completed");

  assistant_settings_->Flush();
  ASSERT_TRUE(assistant_settings_->AdvanceSpeakerIdEnrollmentState());
  WaitForElementAttribute(
      {"assistant-optin-flow-card", "voice-match", "voice-entry-2"}, "active");

  assistant_settings_->Flush();
  ASSERT_TRUE(assistant_settings_->AdvanceSpeakerIdEnrollmentState());
  WaitForElementAttribute(
      {"assistant-optin-flow-card", "voice-match", "voice-entry-2"},
      "completed");

  assistant_settings_->Flush();
  ASSERT_TRUE(assistant_settings_->AdvanceSpeakerIdEnrollmentState());
  WaitForElementAttribute(
      {"assistant-optin-flow-card", "voice-match", "voice-entry-3"}, "active");

  assistant_settings_->Flush();
  ASSERT_TRUE(assistant_settings_->AdvanceSpeakerIdEnrollmentState());
  WaitForElementAttribute(
      {"assistant-optin-flow-card", "voice-match", "voice-entry-3"},
      "completed");
  test::OobeJS().ExpectHiddenPath(
      {"assistant-optin-flow-card", "voice-match", "later-button"});

  // This should finish the enrollment, and move the UI to get-more screen.
  assistant_settings_->Flush();
  ASSERT_TRUE(assistant_settings_->AdvanceSpeakerIdEnrollmentState());
  EXPECT_FALSE(assistant_settings_->IsSpeakerIdEnrollmentActive());

  WaitForAssistantScreen("get-more");
  TapWhenEnabled({"assistant-optin-flow-card", "get-more", "next-button"});

  WaitForScreenExit();

  ExpectCollectedOptIns({});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_EQ(assistant::prefs::ConsentStatus::kActivityControlAccepted,
            prefs->GetInteger(assistant::prefs::kAssistantConsentStatus));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
}

IN_PROC_BROWSER_TEST_F(MAYBE_AssistantOptInFlowTest,
                       BailOutDuringSpeakerIdEnrollment) {
  assistant_settings_->set_consent_ui_flags(
      FakeAssistantSettings::CONSENT_UI_FLAG_SKIP_ACTIVITY_CONTROL |
      FakeAssistantSettings::CONSENT_UI_FLAG_SKIP_THIRD_PARTY_DISCLOSURE);
  assistant_settings_->set_speaker_id_enrollment_mode(
      FakeAssistantSettings::SpeakerIdEnrollmentMode::STEP_BY_STEP);

  SetUpAssistantScreensForTest();
  ash::AssistantState::Get()->NotifyStatusChanged(
      ash::mojom::AssistantState::READY);

  assistant_optin_flow_screen_->Show();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  WaitForAssistantScreen("voice-match");
  TapWhenEnabled({"assistant-optin-flow-card", "voice-match", "agree-button"});

  assistant_settings_->Flush();
  ASSERT_TRUE(assistant_settings_->AdvanceSpeakerIdEnrollmentState());
  WaitForElementAttribute(
      {"assistant-optin-flow-card", "voice-match", "voice-entry-0"}, "active");
  test::OobeJS().ExpectVisiblePath(
      {"assistant-optin-flow-card", "voice-match", "later-button"});

  assistant_settings_->Flush();
  ASSERT_TRUE(assistant_settings_->AdvanceSpeakerIdEnrollmentState());
  WaitForElementAttribute(
      {"assistant-optin-flow-card", "voice-match", "voice-entry-0"},
      "completed");

  test::OobeJS().TapOnPath(
      {"assistant-optin-flow-card", "voice-match", "later-button"});
  assistant_settings_->Flush();
  EXPECT_FALSE(assistant_settings_->IsSpeakerIdEnrollmentActive());

  WaitForAssistantScreen("get-more");
  TapWhenEnabled({"assistant-optin-flow-card", "get-more", "next-button"});

  WaitForScreenExit();

  ExpectCollectedOptIns({});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_EQ(assistant::prefs::ConsentStatus::kActivityControlAccepted,
            prefs->GetInteger(assistant::prefs::kAssistantConsentStatus));
  EXPECT_FALSE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
}

IN_PROC_BROWSER_TEST_F(MAYBE_AssistantOptInFlowTest,
                       SpeakerIdEnrollmentFailureAndRetry) {
  assistant_settings_->set_consent_ui_flags(
      FakeAssistantSettings::CONSENT_UI_FLAG_SKIP_ACTIVITY_CONTROL |
      FakeAssistantSettings::CONSENT_UI_FLAG_SKIP_THIRD_PARTY_DISCLOSURE);
  assistant_settings_->set_speaker_id_enrollment_mode(
      FakeAssistantSettings::SpeakerIdEnrollmentMode::STEP_BY_STEP);

  SetUpAssistantScreensForTest();
  ash::AssistantState::Get()->NotifyStatusChanged(
      ash::mojom::AssistantState::READY);

  assistant_optin_flow_screen_->Show();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  TapWhenEnabled({"assistant-optin-flow-card", "voice-match", "agree-button"});

  assistant_settings_->Flush();
  ASSERT_TRUE(assistant_settings_->AdvanceSpeakerIdEnrollmentState());
  WaitForElementAttribute(
      {"assistant-optin-flow-card", "voice-match", "voice-entry-0"}, "active");
  test::OobeJS().ExpectVisiblePath(
      {"assistant-optin-flow-card", "voice-match", "later-button"});

  assistant_settings_->Flush();
  assistant_settings_->FailSpeakerIdEnrollment();

  // Failure should cause an error screen to be shown, with retry button
  // available.
  WaitForAssistantScreen("loading");

  // Make enrollment succeed immediately next time.
  assistant_settings_->set_speaker_id_enrollment_mode(
      FakeAssistantSettings::SpeakerIdEnrollmentMode::IMMEDIATE);

  TapWhenEnabled({"assistant-optin-flow-card", "loading", "retry-button"});

  WaitForAssistantScreen("voice-match");
  TapWhenEnabled({"assistant-optin-flow-card", "voice-match", "agree-button"});

  WaitForAssistantScreen("get-more");
  TapWhenEnabled({"assistant-optin-flow-card", "get-more", "next-button"});

  WaitForScreenExit();

  ExpectCollectedOptIns({});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_EQ(assistant::prefs::ConsentStatus::kActivityControlAccepted,
            prefs->GetInteger(assistant::prefs::kAssistantConsentStatus));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
}

IN_PROC_BROWSER_TEST_F(MAYBE_AssistantOptInFlowTest, WAADisabledByPolicy) {
  assistant_settings_->set_consent_ui_flags(
      FakeAssistantSettings::CONSENT_UI_FLAG_WAA_DISABLED_BY_POLICY);

  ash::AssistantState::Get()->NotifyStatusChanged(
      ash::mojom::AssistantState::READY);
  SetUpAssistantScreensForTest();
  assistant_optin_flow_screen_->Show();

  WaitForScreenExit();

  ExpectCollectedOptIns({});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantEnabled));
  EXPECT_FALSE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_FALSE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
}

IN_PROC_BROWSER_TEST_F(MAYBE_AssistantOptInFlowTest,
                       AssistantDisabledByPolicy) {
  assistant_settings_->set_consent_ui_flags(
      FakeAssistantSettings::CONSENT_UI_FLAG_ASSISTANT_DISABLED_BY_POLICY);

  ash::AssistantState::Get()->NotifyStatusChanged(
      ash::mojom::AssistantState::READY);
  SetUpAssistantScreensForTest();
  assistant_optin_flow_screen_->Show();

  WaitForScreenExit();

  ExpectCollectedOptIns({});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantDisabledByPolicy));
  EXPECT_FALSE(prefs->GetBoolean(assistant::prefs::kAssistantEnabled));
  EXPECT_FALSE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_FALSE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
}

}  // namespace chromeos
