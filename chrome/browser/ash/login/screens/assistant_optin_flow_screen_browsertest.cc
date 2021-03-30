// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/assistant_optin_flow_screen.h"

#include <memory>
#include <set>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/ash/login/login_wizard.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/sync_consent_screen.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/assistant_optin_flow_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/services/assistant/public/cpp/assistant_settings.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/assistant/public/proto/settings_ui.pb.h"
#include "chromeos/services/assistant/service.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
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

constexpr char kAssistantOptInId[] = "assistant-optin-flow";
constexpr char kAssistantOptInFlowCard[] = "card";
constexpr char kLoading[] = "loading";
constexpr char kValueProp[] = "valueProp";
constexpr char kRelatedInfo[] = "relatedInfo";
constexpr char kVoiceMatch[] = "voiceMatch";
constexpr char kThirdParty[] = "thirdParty";
constexpr char kGetMore[] = "getMore";

const test::UIPath kAssistantLoading = {kAssistantOptInId,
                                        kAssistantOptInFlowCard, kLoading};
const test::UIPath kLoadingRetryButton = {
    kAssistantOptInId, kAssistantOptInFlowCard, kLoading, "retry-button"};

const test::UIPath kAssistantValueProp = {kAssistantOptInId,
                                          kAssistantOptInFlowCard, kValueProp};
const test::UIPath kValuePropNextButton = {
    kAssistantOptInId, kAssistantOptInFlowCard, kValueProp, "next-button"};
const test::UIPath kValuePropSkipButton = {
    kAssistantOptInId, kAssistantOptInFlowCard, kValueProp, "skip-button"};

const test::UIPath kAssistantRelatedInfo = {
    kAssistantOptInId, kAssistantOptInFlowCard, kRelatedInfo};
const test::UIPath kRelatedInfoNextButton = {
    kAssistantOptInId, kAssistantOptInFlowCard, kRelatedInfo, "next-button"};
const test::UIPath kRelatedInfoSkipButton = {
    kAssistantOptInId, kAssistantOptInFlowCard, kRelatedInfo, "skip-button"};

const test::UIPath kAssistantVoiceMatch = {
    kAssistantOptInId, kAssistantOptInFlowCard, kVoiceMatch};
const test::UIPath kVoiceMatchAgreeButton = {
    kAssistantOptInId, kAssistantOptInFlowCard, kVoiceMatch, "agree-button"};
const test::UIPath kVoiceMatchLaterButton = {
    kAssistantOptInId, kAssistantOptInFlowCard, kVoiceMatch, "later-button"};
const test::UIPath kVoiceMatchEntry0 = {
    kAssistantOptInId, kAssistantOptInFlowCard, kVoiceMatch, "voice-entry-0"};
const test::UIPath kVoiceMatchEntry1 = {
    kAssistantOptInId, kAssistantOptInFlowCard, kVoiceMatch, "voice-entry-1"};
const test::UIPath kVoiceMatchEntry2 = {
    kAssistantOptInId, kAssistantOptInFlowCard, kVoiceMatch, "voice-entry-2"};
const test::UIPath kVoiceMatchEntry3 = {
    kAssistantOptInId, kAssistantOptInFlowCard, kVoiceMatch, "voice-entry-3"};

const test::UIPath kAssistantThirdParty = {
    kAssistantOptInId, kAssistantOptInFlowCard, kThirdParty};
const test::UIPath kThirdPartyNextButton = {
    kAssistantOptInId, kAssistantOptInFlowCard, kThirdParty, "next-button"};

const test::UIPath kAssistantGetMore = {kAssistantOptInId,
                                        kAssistantOptInFlowCard, kGetMore};
const test::UIPath kGetMoreNextButton = {
    kAssistantOptInId, kAssistantOptInFlowCard, kGetMore, "next-button"};
const test::UIPath kGetMoreToggleEmail = {
    kAssistantOptInId, kAssistantOptInFlowCard, kGetMore, "toggle-email"};

constexpr char kAssistantOptInScreenExitReason[] =
    "OOBE.StepCompletionTimeByExitReason.Assistant-optin-flow.Next";
constexpr char kAssistantOptInScreenStepCompletionTime[] =
    "OOBE.StepCompletionTime.Assistant-optin-flow";

class ScopedAssistantSettings : public chromeos::assistant::AssistantSettings {
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

  ScopedAssistantSettings() = default;

  ~ScopedAssistantSettings() override = default;

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
    speaker_id_enrollment_client_->OnSpeakerIdEnrollmentFailure();
    processed_hotwords_ = 0;
    speaker_id_enrollment_state_ = SpeakerIdEnrollmentState::IDLE;
  }

  // chromeos::assistant::AssistantSettings:
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
      base::WeakPtr<chromeos::assistant::SpeakerIdEnrollmentClient> client)
      override {
    if (speaker_id_enrollment_mode_ == SpeakerIdEnrollmentMode::IMMEDIATE) {
      client->OnSpeakerIdEnrollmentDone();
      return;
    }
    ASSERT_FALSE(speaker_id_enrollment_client_);
    processed_hotwords_ = 0;
    speaker_id_enrollment_client_ = std::move(client);
    speaker_id_enrollment_state_ = SpeakerIdEnrollmentState::REQUESTED;
  }

  void StopSpeakerIdEnrollment() override {
    processed_hotwords_ = 0;
    speaker_id_enrollment_state_ = SpeakerIdEnrollmentState::IDLE;
    speaker_id_enrollment_client_.reset();
  }

  void SyncSpeakerIdEnrollmentStatus() override {}

 private:
  enum class SpeakerIdEnrollmentState {
    IDLE,
    REQUESTED,
    LISTENING,
    PROCESSING
  };

  // The service test config:
  int consent_ui_flags_ = CONSENT_UI_FLAGS_NONE;
  SpeakerIdEnrollmentMode speaker_id_enrollment_mode_ =
      SpeakerIdEnrollmentMode::IMMEDIATE;

  // Speaker ID enrollment state:
  SpeakerIdEnrollmentState speaker_id_enrollment_state_ =
      SpeakerIdEnrollmentState::IDLE;
  base::WeakPtr<assistant::SpeakerIdEnrollmentClient>
      speaker_id_enrollment_client_;
  int processed_hotwords_ = 0;

  // Set of opt ins given by the user.
  std::set<OptIn> collected_optins_;

  DISALLOW_COPY_AND_ASSIGN(ScopedAssistantSettings);
};

}  // namespace

class AssistantOptInFlowTest : public OobeBaseTest {
 public:
  AssistantOptInFlowTest() {
    scoped_feature_list_.InitAndEnableFeature(
        assistant::features::kEnableBetterAssistant);
  }
  ~AssistantOptInFlowTest() override = default;

  void RegisterAdditionalRequestHandlers() override {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &AssistantOptInFlowTest::HandleRequest, base::Unretained(this)));
  }

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    assistant_settings_ = std::make_unique<ScopedAssistantSettings>();

    AssistantOptInFlowScreen* assistant_optin_flow_screen =
        WizardController::default_controller()
            ->GetScreen<AssistantOptInFlowScreen>();
    original_callback_ =
        assistant_optin_flow_screen->get_exit_callback_for_testing();
    assistant_optin_flow_screen->set_exit_callback_for_testing(
        base::BindRepeating(&AssistantOptInFlowTest::HandleScreenExit,
                            base::Unretained(this)));
  }

  void TearDownOnMainThread() override {
    assistant_settings_.reset();
    OobeBaseTest::TearDownOnMainThread();
  }

  void ShowAssistantOptInFlowScreen() {
    login_manager_.LoginAsNewRegularUser();
    OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
    if (!screen_exited_) {
      LoginDisplayHost::default_host()->StartWizard(
          AssistantOptInFlowScreenView::kScreenId);
    }
  }

  // Waits for the OOBE UI to complete initialization, and overrides:
  // *   the assistant value prop webview URL with the one provided by embedded
  //     https proxy.
  // *   the timeout delay for sending done user action from voice match screen.
  void SetUpAssistantScreensForTest() {
    std::string url_template = embedded_test_server()
                                   ->GetURL("/test_assistant/$/value_prop.html")
                                   .spec();
    test::OobeJS().Evaluate(test::GetOobeElementPath(kAssistantValueProp) +
                            ".setUrlTemplateForTesting('" + url_template +
                            "')");
    test::OobeJS().Evaluate(test::GetOobeElementPath(kAssistantRelatedInfo) +
                            ".setUrlTemplateForTesting('" + url_template +
                            "')");
    test::OobeJS().Evaluate(test::GetOobeElementPath(kAssistantVoiceMatch) +
                            ".setDoneActionDelayForTesting(0)");
  }

  // Waits for the button specified by IDs in `button_path` to become enabled,
  // and then taps it.
  void TapWhenEnabled(std::initializer_list<base::StringPiece> button_path) {
    test::OobeJS().CreateEnabledWaiter(true, button_path)->Wait();
    test::OobeJS().TapOnPath(button_path);
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
      const std::set<ScopedAssistantSettings::OptIn>& opt_ins) {
    EXPECT_EQ(opt_ins, assistant_settings_->collected_optins());
  }

  void WaitForScreenExit() {
    if (screen_exited_)
      return;

    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  std::unique_ptr<ScopedAssistantSettings> assistant_settings_;

  base::Optional<AssistantOptInFlowScreen::Result> screen_result_;
  base::HistogramTester histogram_tester_;

  // If set, HandleRequest will return an error for the next value prop URL
  // request..
  bool fail_next_value_prop_url_request_ = false;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

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

  void HandleScreenExit(AssistantOptInFlowScreen::Result result) {
    ASSERT_FALSE(screen_exited_);
    screen_exited_ = true;
    screen_result_ = result;
    original_callback_.Run(result);
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  bool screen_exited_ = false;
  base::OnceClosure screen_exit_callback_;
  AssistantOptInFlowScreen::ScreenExitCallback original_callback_;

  LoginManagerMixin login_manager_{&mixin_host_};
};

class AssistantOptInFlowNewLayoutDisabledTest : public AssistantOptInFlowTest {
 public:
  AssistantOptInFlowNewLayoutDisabledTest() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndDisableFeature(
        assistant::features::kEnableBetterAssistant);
  }
};

IN_PROC_BROWSER_TEST_F(AssistantOptInFlowTest, Basic) {
  auto force_lib_assistant_enabled =
      AssistantOptInFlowScreen::ForceLibAssistantEnabledForTesting(true);
  ash::AssistantState::Get()->NotifyStatusChanged(
      chromeos::assistant::AssistantStatus::READY);

  SetUpAssistantScreensForTest();
  ShowAssistantOptInFlowScreen();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantValueProp)->Wait();
  TapWhenEnabled(kValuePropNextButton);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantRelatedInfo)->Wait();
  TapWhenEnabled(kRelatedInfoNextButton);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantVoiceMatch)->Wait();
  TapWhenEnabled(kVoiceMatchAgreeButton);

  WaitForScreenExit();

  ExpectCollectedOptIns({ScopedAssistantSettings::OptIn::ACTIVITY_CONTROL});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_EQ(assistant::prefs::ConsentStatus::kActivityControlAccepted,
            prefs->GetInteger(assistant::prefs::kAssistantConsentStatus));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
  EXPECT_EQ(screen_result_.value(), AssistantOptInFlowScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenExitReason, 1);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenStepCompletionTime,
                                     1);
}

IN_PROC_BROWSER_TEST_F(AssistantOptInFlowTest, DisableScreenContext) {
  auto force_lib_assistant_enabled =
      AssistantOptInFlowScreen::ForceLibAssistantEnabledForTesting(true);
  ash::AssistantState::Get()->NotifyStatusChanged(
      chromeos::assistant::AssistantStatus::READY);

  SetUpAssistantScreensForTest();
  ShowAssistantOptInFlowScreen();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantValueProp)->Wait();
  TapWhenEnabled(kValuePropNextButton);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantRelatedInfo)->Wait();
  TapWhenEnabled(kRelatedInfoSkipButton);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantVoiceMatch)->Wait();
  TapWhenEnabled(kVoiceMatchAgreeButton);

  WaitForScreenExit();

  ExpectCollectedOptIns({ScopedAssistantSettings::OptIn::ACTIVITY_CONTROL});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_EQ(assistant::prefs::ConsentStatus::kActivityControlAccepted,
            prefs->GetInteger(assistant::prefs::kAssistantConsentStatus));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_FALSE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
  EXPECT_EQ(screen_result_.value(), AssistantOptInFlowScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenExitReason, 1);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenStepCompletionTime,
                                     1);
}

IN_PROC_BROWSER_TEST_F(AssistantOptInFlowTest, AssistantStateUpdateAfterShow) {
  auto force_lib_assistant_enabled =
      AssistantOptInFlowScreen::ForceLibAssistantEnabledForTesting(true);
  SetUpAssistantScreensForTest();
  ShowAssistantOptInFlowScreen();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  ash::AssistantState::Get()->NotifyStatusChanged(
      chromeos::assistant::AssistantStatus::READY);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantValueProp)->Wait();
  TapWhenEnabled(kValuePropNextButton);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantRelatedInfo)->Wait();
  TapWhenEnabled(kRelatedInfoNextButton);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantVoiceMatch)->Wait();
  TapWhenEnabled(kVoiceMatchAgreeButton);

  WaitForScreenExit();

  ExpectCollectedOptIns({ScopedAssistantSettings::OptIn::ACTIVITY_CONTROL});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_EQ(assistant::prefs::ConsentStatus::kActivityControlAccepted,
            prefs->GetInteger(assistant::prefs::kAssistantConsentStatus));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
  EXPECT_EQ(screen_result_.value(), AssistantOptInFlowScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenExitReason, 1);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenStepCompletionTime,
                                     1);
}

IN_PROC_BROWSER_TEST_F(AssistantOptInFlowTest, RetryOnWebviewLoadFail) {
  auto force_lib_assistant_enabled =
      AssistantOptInFlowScreen::ForceLibAssistantEnabledForTesting(true);
  SetUpAssistantScreensForTest();
  fail_next_value_prop_url_request_ = true;

  ShowAssistantOptInFlowScreen();

  ash::AssistantState::Get()->NotifyStatusChanged(
      chromeos::assistant::AssistantStatus::READY);

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  // Value prop webview requests are set to fail - loading screen should display
  // an error and an option to retry the request.
  test::OobeJS().CreateVisibilityWaiter(true, kAssistantLoading)->Wait();
  TapWhenEnabled(kLoadingRetryButton);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantValueProp)->Wait();
  TapWhenEnabled(kValuePropNextButton);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantRelatedInfo)->Wait();
  TapWhenEnabled(kRelatedInfoNextButton);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantVoiceMatch)->Wait();
  TapWhenEnabled(kVoiceMatchAgreeButton);

  WaitForScreenExit();

  ExpectCollectedOptIns({ScopedAssistantSettings::OptIn::ACTIVITY_CONTROL});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_EQ(assistant::prefs::ConsentStatus::kActivityControlAccepted,
            prefs->GetInteger(assistant::prefs::kAssistantConsentStatus));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
  EXPECT_EQ(screen_result_.value(), AssistantOptInFlowScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenExitReason, 1);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenStepCompletionTime,
                                     1);
}

IN_PROC_BROWSER_TEST_F(AssistantOptInFlowTest, RejectValueProp) {
  auto force_lib_assistant_enabled =
      AssistantOptInFlowScreen::ForceLibAssistantEnabledForTesting(true);
  SetUpAssistantScreensForTest();
  ash::AssistantState::Get()->NotifyStatusChanged(
      chromeos::assistant::AssistantStatus::READY);

  ShowAssistantOptInFlowScreen();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantValueProp)->Wait();
  TapWhenEnabled(kValuePropSkipButton);

  WaitForScreenExit();

  ExpectCollectedOptIns({});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_EQ(assistant::prefs::ConsentStatus::kUnknown,
            prefs->GetInteger(assistant::prefs::kAssistantConsentStatus));
  EXPECT_FALSE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_FALSE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
  EXPECT_EQ(screen_result_.value(), AssistantOptInFlowScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenExitReason, 1);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenStepCompletionTime,
                                     1);
}

IN_PROC_BROWSER_TEST_F(AssistantOptInFlowNewLayoutDisabledTest,
                       AskEmailOptIn_NotChecked) {
  auto force_lib_assistant_enabled =
      AssistantOptInFlowScreen::ForceLibAssistantEnabledForTesting(true);
  assistant_settings_->set_consent_ui_flags(
      ScopedAssistantSettings::CONSENT_UI_FLAG_ASK_EMAIL_OPT_IN);
  ash::AssistantState::Get()->NotifyStatusChanged(
      chromeos::assistant::AssistantStatus::READY);

  SetUpAssistantScreensForTest();
  ShowAssistantOptInFlowScreen();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantValueProp)->Wait();
  TapWhenEnabled(kValuePropNextButton);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantThirdParty)->Wait();
  TapWhenEnabled(kThirdPartyNextButton);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantVoiceMatch)->Wait();
  TapWhenEnabled(kVoiceMatchAgreeButton);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantGetMore)->Wait();
  test::OobeJS().ExpectVisiblePath(kGetMoreToggleEmail);
  test::OobeJS().ExpectEnabledPath(kGetMoreToggleEmail);

  // Complete flow without checking the email opt-in toggle.
  TapWhenEnabled(kGetMoreNextButton);

  WaitForScreenExit();

  ExpectCollectedOptIns({ScopedAssistantSettings::OptIn::ACTIVITY_CONTROL});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_EQ(assistant::prefs::ConsentStatus::kActivityControlAccepted,
            prefs->GetInteger(assistant::prefs::kAssistantConsentStatus));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
  EXPECT_EQ(screen_result_.value(), AssistantOptInFlowScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenExitReason, 1);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenStepCompletionTime,
                                     1);
}

IN_PROC_BROWSER_TEST_F(AssistantOptInFlowNewLayoutDisabledTest,
                       AskEmailOptIn_Accepted) {
  auto force_lib_assistant_enabled =
      AssistantOptInFlowScreen::ForceLibAssistantEnabledForTesting(true);
  assistant_settings_->set_consent_ui_flags(
      ScopedAssistantSettings::CONSENT_UI_FLAG_ASK_EMAIL_OPT_IN);
  ash::AssistantState::Get()->NotifyStatusChanged(
      chromeos::assistant::AssistantStatus::READY);

  SetUpAssistantScreensForTest();
  ShowAssistantOptInFlowScreen();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantValueProp)->Wait();
  TapWhenEnabled(kValuePropNextButton);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantThirdParty)->Wait();
  TapWhenEnabled(kThirdPartyNextButton);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantVoiceMatch)->Wait();
  TapWhenEnabled(kVoiceMatchAgreeButton);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantGetMore)->Wait();
  test::OobeJS().ExpectVisiblePath(kGetMoreToggleEmail);
  test::OobeJS().ClickOnPath(kGetMoreToggleEmail);

  TapWhenEnabled(kGetMoreNextButton);

  WaitForScreenExit();

  ExpectCollectedOptIns({ScopedAssistantSettings::OptIn::ACTIVITY_CONTROL,
                         ScopedAssistantSettings::OptIn::EMAIL});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_EQ(assistant::prefs::ConsentStatus::kActivityControlAccepted,
            prefs->GetInteger(assistant::prefs::kAssistantConsentStatus));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
  EXPECT_EQ(screen_result_.value(), AssistantOptInFlowScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenExitReason, 1);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenStepCompletionTime,
                                     1);
}

IN_PROC_BROWSER_TEST_F(AssistantOptInFlowTest, SkipShowingValueProp) {
  auto force_lib_assistant_enabled =
      AssistantOptInFlowScreen::ForceLibAssistantEnabledForTesting(true);
  assistant_settings_->set_consent_ui_flags(
      ScopedAssistantSettings::CONSENT_UI_FLAG_SKIP_ACTIVITY_CONTROL);

  SetUpAssistantScreensForTest();
  ash::AssistantState::Get()->NotifyStatusChanged(
      chromeos::assistant::AssistantStatus::READY);

  ShowAssistantOptInFlowScreen();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantRelatedInfo)->Wait();
  TapWhenEnabled(kRelatedInfoNextButton);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantVoiceMatch)->Wait();
  TapWhenEnabled(kVoiceMatchAgreeButton);

  WaitForScreenExit();

  ExpectCollectedOptIns({});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_EQ(assistant::prefs::ConsentStatus::kActivityControlAccepted,
            prefs->GetInteger(assistant::prefs::kAssistantConsentStatus));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
  EXPECT_EQ(screen_result_.value(), AssistantOptInFlowScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenExitReason, 1);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenStepCompletionTime,
                                     1);
}

IN_PROC_BROWSER_TEST_F(AssistantOptInFlowNewLayoutDisabledTest,
                       SkipShowingValuePropAndThirdPartyDisclosure) {
  auto force_lib_assistant_enabled =
      AssistantOptInFlowScreen::ForceLibAssistantEnabledForTesting(true);
  assistant_settings_->set_consent_ui_flags(
      ScopedAssistantSettings::CONSENT_UI_FLAG_SKIP_ACTIVITY_CONTROL |
      ScopedAssistantSettings::CONSENT_UI_FLAG_SKIP_THIRD_PARTY_DISCLOSURE);

  SetUpAssistantScreensForTest();
  ash::AssistantState::Get()->NotifyStatusChanged(
      chromeos::assistant::AssistantStatus::READY);

  ShowAssistantOptInFlowScreen();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantVoiceMatch)->Wait();
  TapWhenEnabled(kVoiceMatchAgreeButton);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantGetMore)->Wait();
  TapWhenEnabled(kGetMoreNextButton);

  WaitForScreenExit();

  ExpectCollectedOptIns({});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_EQ(assistant::prefs::ConsentStatus::kActivityControlAccepted,
            prefs->GetInteger(assistant::prefs::kAssistantConsentStatus));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
  EXPECT_EQ(screen_result_.value(), AssistantOptInFlowScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenExitReason, 1);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenStepCompletionTime,
                                     1);
}

IN_PROC_BROWSER_TEST_F(AssistantOptInFlowTest, SpeakerIdEnrollment) {
  auto force_lib_assistant_enabled =
      AssistantOptInFlowScreen::ForceLibAssistantEnabledForTesting(true);
  assistant_settings_->set_consent_ui_flags(
      ScopedAssistantSettings::CONSENT_UI_FLAG_SKIP_ACTIVITY_CONTROL);
  assistant_settings_->set_speaker_id_enrollment_mode(
      ScopedAssistantSettings::SpeakerIdEnrollmentMode::STEP_BY_STEP);

  SetUpAssistantScreensForTest();
  ash::AssistantState::Get()->NotifyStatusChanged(
      chromeos::assistant::AssistantStatus::READY);

  ShowAssistantOptInFlowScreen();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantRelatedInfo)->Wait();
  TapWhenEnabled(kRelatedInfoNextButton);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantVoiceMatch)->Wait();
  TapWhenEnabled(kVoiceMatchAgreeButton);

  ASSERT_TRUE(assistant_settings_->AdvanceSpeakerIdEnrollmentState());
  WaitForElementAttribute(kVoiceMatchEntry0, "active");
  test::OobeJS().ExpectVisiblePath(kVoiceMatchLaterButton);

  ASSERT_TRUE(assistant_settings_->AdvanceSpeakerIdEnrollmentState());
  WaitForElementAttribute(kVoiceMatchEntry0, "completed");
  test::OobeJS().ExpectVisiblePath(kVoiceMatchLaterButton);

  ASSERT_TRUE(assistant_settings_->AdvanceSpeakerIdEnrollmentState());
  WaitForElementAttribute(kVoiceMatchEntry1, "active");
  test::OobeJS().ExpectVisiblePath(kVoiceMatchLaterButton);

  ASSERT_TRUE(assistant_settings_->AdvanceSpeakerIdEnrollmentState());
  WaitForElementAttribute(kVoiceMatchEntry1, "completed");

  ASSERT_TRUE(assistant_settings_->AdvanceSpeakerIdEnrollmentState());
  WaitForElementAttribute(kVoiceMatchEntry2, "active");

  ASSERT_TRUE(assistant_settings_->AdvanceSpeakerIdEnrollmentState());
  WaitForElementAttribute(kVoiceMatchEntry2, "completed");

  ASSERT_TRUE(assistant_settings_->AdvanceSpeakerIdEnrollmentState());
  WaitForElementAttribute(kVoiceMatchEntry3, "active");

  ASSERT_TRUE(assistant_settings_->AdvanceSpeakerIdEnrollmentState());
  WaitForElementAttribute(kVoiceMatchEntry3, "completed");
  test::OobeJS().ExpectHiddenPath(kVoiceMatchLaterButton);

  // This should finish the enrollment, and move the UI to get-more screen.
  ASSERT_TRUE(assistant_settings_->AdvanceSpeakerIdEnrollmentState());
  EXPECT_FALSE(assistant_settings_->IsSpeakerIdEnrollmentActive());

  WaitForScreenExit();

  ExpectCollectedOptIns({});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_EQ(assistant::prefs::ConsentStatus::kActivityControlAccepted,
            prefs->GetInteger(assistant::prefs::kAssistantConsentStatus));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
  EXPECT_EQ(screen_result_.value(), AssistantOptInFlowScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenExitReason, 1);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenStepCompletionTime,
                                     1);
}

IN_PROC_BROWSER_TEST_F(AssistantOptInFlowTest,
                       BailOutDuringSpeakerIdEnrollment) {
  auto force_lib_assistant_enabled =
      AssistantOptInFlowScreen::ForceLibAssistantEnabledForTesting(true);
  assistant_settings_->set_consent_ui_flags(
      ScopedAssistantSettings::CONSENT_UI_FLAG_SKIP_ACTIVITY_CONTROL);
  assistant_settings_->set_speaker_id_enrollment_mode(
      ScopedAssistantSettings::SpeakerIdEnrollmentMode::STEP_BY_STEP);

  SetUpAssistantScreensForTest();
  ash::AssistantState::Get()->NotifyStatusChanged(
      chromeos::assistant::AssistantStatus::READY);

  ShowAssistantOptInFlowScreen();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantRelatedInfo)->Wait();
  TapWhenEnabled(kRelatedInfoNextButton);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantVoiceMatch)->Wait();
  TapWhenEnabled(kVoiceMatchAgreeButton);

  ASSERT_TRUE(assistant_settings_->AdvanceSpeakerIdEnrollmentState());
  WaitForElementAttribute(kVoiceMatchEntry0, "active");
  test::OobeJS().ExpectVisiblePath(kVoiceMatchLaterButton);

  ASSERT_TRUE(assistant_settings_->AdvanceSpeakerIdEnrollmentState());
  WaitForElementAttribute(kVoiceMatchEntry0, "completed");

  test::OobeJS().TapOnPath(kVoiceMatchLaterButton);
  EXPECT_FALSE(assistant_settings_->IsSpeakerIdEnrollmentActive());

  WaitForScreenExit();

  ExpectCollectedOptIns({});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_EQ(assistant::prefs::ConsentStatus::kActivityControlAccepted,
            prefs->GetInteger(assistant::prefs::kAssistantConsentStatus));
  EXPECT_FALSE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
  EXPECT_EQ(screen_result_.value(), AssistantOptInFlowScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenExitReason, 1);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenStepCompletionTime,
                                     1);
}

IN_PROC_BROWSER_TEST_F(AssistantOptInFlowTest,
                       SpeakerIdEnrollmentFailureAndRetry) {
  auto force_lib_assistant_enabled =
      AssistantOptInFlowScreen::ForceLibAssistantEnabledForTesting(true);
  assistant_settings_->set_consent_ui_flags(
      ScopedAssistantSettings::CONSENT_UI_FLAG_SKIP_ACTIVITY_CONTROL);
  assistant_settings_->set_speaker_id_enrollment_mode(
      ScopedAssistantSettings::SpeakerIdEnrollmentMode::STEP_BY_STEP);

  SetUpAssistantScreensForTest();
  ash::AssistantState::Get()->NotifyStatusChanged(
      chromeos::assistant::AssistantStatus::READY);

  ShowAssistantOptInFlowScreen();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.set_assert_next_screen();
  screen_waiter.Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantRelatedInfo)->Wait();
  TapWhenEnabled(kRelatedInfoNextButton);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantVoiceMatch)->Wait();
  TapWhenEnabled(kVoiceMatchAgreeButton);

  ASSERT_TRUE(assistant_settings_->AdvanceSpeakerIdEnrollmentState());
  WaitForElementAttribute(kVoiceMatchEntry0, "active");
  test::OobeJS().ExpectVisiblePath(kVoiceMatchLaterButton);

  assistant_settings_->FailSpeakerIdEnrollment();

  // Failure should cause an error screen to be shown, with retry button
  // available.
  test::OobeJS().CreateVisibilityWaiter(true, kAssistantLoading)->Wait();

  // Make enrollment succeed immediately next time.
  assistant_settings_->set_speaker_id_enrollment_mode(
      ScopedAssistantSettings::SpeakerIdEnrollmentMode::IMMEDIATE);

  TapWhenEnabled(kLoadingRetryButton);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantVoiceMatch)->Wait();
  TapWhenEnabled(kVoiceMatchAgreeButton);

  WaitForScreenExit();

  ExpectCollectedOptIns({});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_EQ(assistant::prefs::ConsentStatus::kActivityControlAccepted,
            prefs->GetInteger(assistant::prefs::kAssistantConsentStatus));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
  EXPECT_EQ(screen_result_.value(), AssistantOptInFlowScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenExitReason, 1);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenStepCompletionTime,
                                     1);
}

IN_PROC_BROWSER_TEST_F(AssistantOptInFlowTest, WAADisabledByPolicy) {
  auto force_lib_assistant_enabled =
      AssistantOptInFlowScreen::ForceLibAssistantEnabledForTesting(true);
  assistant_settings_->set_consent_ui_flags(
      ScopedAssistantSettings::CONSENT_UI_FLAG_WAA_DISABLED_BY_POLICY);

  ash::AssistantState::Get()->NotifyStatusChanged(
      chromeos::assistant::AssistantStatus::READY);
  SetUpAssistantScreensForTest();
  ShowAssistantOptInFlowScreen();

  WaitForScreenExit();

  ExpectCollectedOptIns({});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantEnabled));
  EXPECT_FALSE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_FALSE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
  EXPECT_EQ(screen_result_.value(), AssistantOptInFlowScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenExitReason, 1);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenStepCompletionTime,
                                     1);
}

IN_PROC_BROWSER_TEST_F(AssistantOptInFlowTest, AssistantDisabledByPolicy) {
  auto force_lib_assistant_enabled =
      AssistantOptInFlowScreen::ForceLibAssistantEnabledForTesting(true);
  assistant_settings_->set_consent_ui_flags(
      ScopedAssistantSettings::CONSENT_UI_FLAG_ASSISTANT_DISABLED_BY_POLICY);

  ash::AssistantState::Get()->NotifyStatusChanged(
      chromeos::assistant::AssistantStatus::READY);
  SetUpAssistantScreensForTest();
  ShowAssistantOptInFlowScreen();

  WaitForScreenExit();

  ExpectCollectedOptIns({});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantDisabledByPolicy));
  EXPECT_FALSE(prefs->GetBoolean(assistant::prefs::kAssistantEnabled));
  EXPECT_FALSE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_FALSE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
  EXPECT_EQ(screen_result_.value(), AssistantOptInFlowScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenExitReason, 1);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenStepCompletionTime,
                                     1);
}

IN_PROC_BROWSER_TEST_F(AssistantOptInFlowTest, AssistantSkippedNoLib) {
  auto force_lib_assistant_disabled =
      AssistantOptInFlowScreen::ForceLibAssistantEnabledForTesting(false);
  ash::AssistantState::Get()->NotifyStatusChanged(
      chromeos::assistant::AssistantStatus::READY);
  SetUpAssistantScreensForTest();
  ShowAssistantOptInFlowScreen();

  WaitForScreenExit();

  ExpectCollectedOptIns({});
  EXPECT_EQ(screen_result_.value(),
            AssistantOptInFlowScreen::Result::NOT_APPLICABLE);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenExitReason, 0);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenStepCompletionTime,
                                     0);
}

}  // namespace chromeos
