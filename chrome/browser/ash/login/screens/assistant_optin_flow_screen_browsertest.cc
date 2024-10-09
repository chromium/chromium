// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/assistant_optin_flow_screen.h"

#include <memory>
#include <set>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_base.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/login_wizard.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/sync_consent_screen.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/assistant_optin_flow_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_settings.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/ash/services/assistant/public/proto/activity_control_settings_common.pb.h"
#include "chromeos/ash/services/assistant/public/proto/get_settings_ui.pb.h"
#include "chromeos/ash/services/assistant/public/proto/settings_ui.pb.h"
#include "chromeos/ash/services/assistant/service.h"
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

namespace ash {

namespace {

using ::net::test_server::BasicHttpResponse;
using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;

constexpr char kTestUser[] = "test-user1@gmail.com";

constexpr char kAssistantConsentToken[] = "consent_token";
constexpr char kAssistantUiAuditKey[] = "ui_audit_key";

constexpr char kAssistantOptInId[] = "assistant-optin-flow";
constexpr char kAssistantOptInFlowCard[] = "card";
constexpr char kLoading[] = "loading";
constexpr char kValueProp[] = "valueProp";
constexpr char kRelatedInfo[] = "relatedInfo";
constexpr char kVoiceMatch[] = "voiceMatch";

constexpr char kSettingsZippyTitle[] = "Settings-Zippy-Title";
constexpr char kSettingsZippyDescription[] = "Settings-Zippy-Description";
constexpr char kSettingsZippyAdditionalInfo[] =
    "Settings-Zippy-Additional-Info";
constexpr char kSettingsZippyLearnMoreLink[] = "Learn more";

// &ensp;
constexpr char kEnsp[] = "\xe2\x80\x82";

const test::UIPath kAssistantLoading = {kAssistantOptInId,
                                        kAssistantOptInFlowCard, kLoading};
const test::UIPath kLoadingRetryButton = {
    kAssistantOptInId, kAssistantOptInFlowCard, kLoading, "retry-button"};
const test::UIPath kSettingsZippyTitleFirst = {
    kAssistantOptInId, kAssistantOptInFlowCard, kValueProp, "title-0"};
const test::UIPath kSettingsZippyDescriptionFirst = {
    kAssistantOptInId, kAssistantOptInFlowCard, kValueProp, "description-0"};
const test::UIPath kSettingsZippyAdditionalInfoFirst = {
    kAssistantOptInId, kAssistantOptInFlowCard, kValueProp,
    "additional-info-0"};

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

constexpr char kAssistantOptInScreenExitReason[] =
    "OOBE.StepCompletionTimeByExitReason.Assistant-optin-flow.Next";
constexpr char kAssistantOptInScreenStepCompletionTime[] =
    "OOBE.StepCompletionTime.Assistant-optin-flow";

class ScopedAssistantSettings : public assistant::AssistantSettings {
 public:
  // Flags to configure GetSettings response.
  static constexpr int CONSENT_UI_FLAGS_NONE = 0;
  static constexpr int CONSENT_UI_FLAG_SKIP_ACTIVITY_CONTROL = 1;
  static constexpr int CONSENT_UI_FLAG_WAA_DISABLED_BY_POLICY = 1 << 1;
  static constexpr int CONSENT_UI_FLAG_ASSISTANT_DISABLED_BY_POLICY = 1 << 2;

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

  ScopedAssistantSettings(const ScopedAssistantSettings&) = delete;
  ScopedAssistantSettings& operator=(const ScopedAssistantSettings&) = delete;

  ~ScopedAssistantSettings() override = default;

  void set_consent_ui_flags(int flags) { consent_ui_flags_ = flags; }

  void set_speaker_id_enrollment_mode(SpeakerIdEnrollmentMode mode) {
    speaker_id_enrollment_mode_ = mode;
  }

  void set_setting_zippy_size(int size) { setting_zippy_size_ = size; }

  void set_is_minor_user(bool is_minor_user) { is_minor_user_ = is_minor_user; }

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

  // assistant::AssistantSettings:
  void GetSettings(const std::string& selector,
                   GetSettingsCallback callback) override {}

  void GetSettingsWithHeader(const std::string& selector,
                             GetSettingsCallback callback) override {
    assistant::SettingsUiSelector selector_proto;
    ASSERT_TRUE(selector_proto.ParseFromString(selector));
    EXPECT_FALSE(selector_proto.about_me_settings());
    EXPECT_TRUE(selector_proto.has_consent_flow_ui_selector());
    EXPECT_EQ(assistant::ActivityControlSettingsUiSelector::
                  ASSISTANT_SUW_ONBOARDING_ON_CHROME_OS,
              selector_proto.consent_flow_ui_selector().flow_id());

    assistant::GetSettingsUiResponse response;

    if (is_minor_user_) {
      auto* header = response.mutable_header();
      header->set_footer_button_layout(
          assistant::SettingsResponseHeader_AcceptRejectLayout_EQUAL_WEIGHT);
    }

    auto* settings_ui = response.mutable_settings();
    auto* gaia_user_context_ui = settings_ui->mutable_gaia_user_context_ui();
    gaia_user_context_ui->set_is_gaia_user(true);
    gaia_user_context_ui->set_waa_disabled_by_dasher_domain(
        (consent_ui_flags_ & CONSENT_UI_FLAG_WAA_DISABLED_BY_POLICY));
    gaia_user_context_ui->set_assistant_disabled_by_dasher_domain(
        (consent_ui_flags_ & CONSENT_UI_FLAG_ASSISTANT_DISABLED_BY_POLICY));

    auto* consent_flow_ui = settings_ui->mutable_consent_flow_ui();
    consent_flow_ui->set_consent_status(
        assistant::ConsentFlowUi_ConsentStatus_ASK_FOR_CONSENT);
    consent_flow_ui->mutable_consent_ui()->set_accept_button_text("OK");
    consent_flow_ui->mutable_consent_ui()->set_reject_button_text(
        "No, thank you");

    if (!(consent_ui_flags_ & CONSENT_UI_FLAG_SKIP_ACTIVITY_CONTROL)) {
      PopulateActivityControlData(consent_flow_ui->mutable_consent_ui());
      for (int i = 0; i < setting_zippy_size_; i++) {
        auto* multi_consent_ui = consent_flow_ui->add_multi_consent_ui();
        PopulateActivityControlData(multi_consent_ui);
      }
    }

    std::string message;
    EXPECT_TRUE(response.SerializeToString(&message));
    std::move(callback).Run(message);
  }

  void PopulateActivityControlData(
      assistant::ConsentFlowUi_ConsentUi* consent_ui) {
    auto* activity_control_ui = consent_ui->mutable_activity_control_ui();
    activity_control_ui->set_consent_token(kAssistantConsentToken);
    activity_control_ui->set_ui_audit_key(kAssistantUiAuditKey);
    activity_control_ui->set_title("Title");
    activity_control_ui->set_identity(kTestUser);
    activity_control_ui->add_intro_text_paragraph();
    activity_control_ui->set_intro_text_paragraph(0, "Here's an intro");
    activity_control_ui->add_footer_paragraph();
    activity_control_ui->set_footer_paragraph(0, "A footer");
    auto* setting = activity_control_ui->add_setting_zippy();
    setting->set_title(kSettingsZippyTitle);
    setting->add_description_paragraph();
    setting->set_description_paragraph(0, kSettingsZippyDescription);
    setting->add_additional_info_paragraph();
    setting->set_additional_info_paragraph(0, kSettingsZippyAdditionalInfo);
    setting->set_icon_uri("assistant_icon");
    setting->set_setting_set_id(assistant::SettingSetId::WAA);
  }

  void UpdateSettings(const std::string& update,
                      UpdateSettingsCallback callback) override {
    assistant::SettingsUiUpdate update_proto;
    ASSERT_TRUE(update_proto.ParseFromString(update));
    EXPECT_FALSE(update_proto.has_about_me_settings_update());
    EXPECT_FALSE(update_proto.has_assistant_device_settings_update());

    assistant::SettingsUiUpdateResult update_result;
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
    std::string message;
    EXPECT_TRUE(update_result.SerializeToString(&message));
    std::move(callback).Run(message);
  }

  void StartSpeakerIdEnrollment(
      bool skip_cloud_enrollment,
      base::WeakPtr<assistant::SpeakerIdEnrollmentClient> client) override {
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

  int setting_zippy_size_ = 1;
  bool is_minor_user_ = false;
};

class AssistantOptInFlowBaseTest : public OobeBaseTest {
 public:
  AssistantOptInFlowBaseTest() = default;
  ~AssistantOptInFlowBaseTest() override = default;

  void RegisterAdditionalRequestHandlers() override {
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &AssistantOptInFlowBaseTest::HandleRequest, base::Unretained(this)));
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
        base::BindRepeating(&AssistantOptInFlowBaseTest::HandleScreenExit,
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
  void TapWhenEnabled(std::initializer_list<std::string_view> button_path) {
    test::OobeJS().CreateEnabledWaiter(true, button_path)->Wait();
    test::OobeJS().TapOnPath(button_path);
  }

  bool ElementHasAttribute(std::initializer_list<std::string_view> element,
                           const std::string& attribute) {
    return test::OobeJS().GetBool(test::GetOobeElementPath(element) +
                                  ".getAttribute('" + attribute + "')");
  }

  void WaitForElementAttribute(std::initializer_list<std::string_view> element,
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
    if (screen_exited_) {
      return;
    }
    base::test::TestFuture<void> waiter;
    screen_exit_callback_ = waiter.GetCallback();
    EXPECT_TRUE(waiter.Wait());
  }

  std::unique_ptr<ScopedAssistantSettings> assistant_settings_;

  std::optional<AssistantOptInFlowScreen::Result> screen_result_;
  base::HistogramTester histogram_tester_;

  // If set, HandleRequest will return an error for the next value prop URL
  // request..
  bool fail_next_value_prop_url_request_ = false;

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

class AssistantOptInFlowTest : public AssistantOptInFlowBaseTest {
 public:
  AssistantOptInFlowTest() {
    scoped_feature_list_.InitAndDisableFeature(
        ash::features::kOobeSkipAssistant);
  }
  ~AssistantOptInFlowTest() override = default;
};

IN_PROC_BROWSER_TEST_F(AssistantOptInFlowTest, Basic) {
  auto force_lib_assistant_enabled =
      AssistantOptInFlowScreen::ForceLibAssistantEnabledForTesting(true);
  AssistantState::Get()->NotifyStatusChanged(assistant::AssistantStatus::READY);

  SetUpAssistantScreensForTest();
  ShowAssistantOptInFlowScreen();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantValueProp)->Wait();
  EXPECT_TRUE(test::OobeJS().GetAttributeBool("inverse", kValuePropNextButton));
  TapWhenEnabled(kValuePropNextButton);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantRelatedInfo)->Wait();
  EXPECT_TRUE(
      test::OobeJS().GetAttributeBool("inverse", kRelatedInfoNextButton));
  TapWhenEnabled(kRelatedInfoNextButton);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantVoiceMatch)->Wait();
  EXPECT_TRUE(
      test::OobeJS().GetAttributeBool("inverse", kVoiceMatchAgreeButton));
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
  AssistantState::Get()->NotifyStatusChanged(assistant::AssistantStatus::READY);

  SetUpAssistantScreensForTest();
  ShowAssistantOptInFlowScreen();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
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
  screen_waiter.Wait();

  AssistantState::Get()->NotifyStatusChanged(assistant::AssistantStatus::READY);

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

// TODO(crbug.com/41486294): Flaky on ChromeOS.
IN_PROC_BROWSER_TEST_F(AssistantOptInFlowTest,
                       DISABLED_RetryOnWebviewLoadFail) {
  auto force_lib_assistant_enabled =
      AssistantOptInFlowScreen::ForceLibAssistantEnabledForTesting(true);
  SetUpAssistantScreensForTest();
  fail_next_value_prop_url_request_ = true;

  ShowAssistantOptInFlowScreen();

  AssistantState::Get()->NotifyStatusChanged(assistant::AssistantStatus::READY);

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
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
  AssistantState::Get()->NotifyStatusChanged(assistant::AssistantStatus::READY);

  ShowAssistantOptInFlowScreen();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
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

// TODO(crbug.com/40917081): Flaky on ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_SkipShowingValueProp DISABLED_SkipShowingValueProp
#else
#define MAYBE_SkipShowingValueProp SkipShowingValueProp
#endif
IN_PROC_BROWSER_TEST_F(AssistantOptInFlowTest, MAYBE_SkipShowingValueProp) {
  auto force_lib_assistant_enabled =
      AssistantOptInFlowScreen::ForceLibAssistantEnabledForTesting(true);
  assistant_settings_->set_consent_ui_flags(
      ScopedAssistantSettings::CONSENT_UI_FLAG_SKIP_ACTIVITY_CONTROL);

  SetUpAssistantScreensForTest();
  AssistantState::Get()->NotifyStatusChanged(assistant::AssistantStatus::READY);

  ShowAssistantOptInFlowScreen();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
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

IN_PROC_BROWSER_TEST_F(AssistantOptInFlowTest, SpeakerIdEnrollment) {
  auto force_lib_assistant_enabled =
      AssistantOptInFlowScreen::ForceLibAssistantEnabledForTesting(true);
  assistant_settings_->set_consent_ui_flags(
      ScopedAssistantSettings::CONSENT_UI_FLAG_SKIP_ACTIVITY_CONTROL);
  assistant_settings_->set_speaker_id_enrollment_mode(
      ScopedAssistantSettings::SpeakerIdEnrollmentMode::STEP_BY_STEP);

  SetUpAssistantScreensForTest();
  AssistantState::Get()->NotifyStatusChanged(assistant::AssistantStatus::READY);

  ShowAssistantOptInFlowScreen();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
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

  // This should finish the enrollment.
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
                       FeatureDisabledDuringSpeakerIdEnrollment) {
  auto force_lib_assistant_enabled =
      AssistantOptInFlowScreen::ForceLibAssistantEnabledForTesting(true);
  assistant_settings_->set_consent_ui_flags(
      ScopedAssistantSettings::CONSENT_UI_FLAG_SKIP_ACTIVITY_CONTROL);
  assistant_settings_->set_speaker_id_enrollment_mode(
      ScopedAssistantSettings::SpeakerIdEnrollmentMode::STEP_BY_STEP);

  SetUpAssistantScreensForTest();
  AssistantState::Get()->NotifyStatusChanged(assistant::AssistantStatus::READY);

  ShowAssistantOptInFlowScreen();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantRelatedInfo)->Wait();
  TapWhenEnabled(kRelatedInfoNextButton);

  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  prefs->SetBoolean(assistant::prefs::kAssistantEnabled, false);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantVoiceMatch)->Wait();
  TapWhenEnabled(kVoiceMatchAgreeButton);

  EXPECT_FALSE(assistant_settings_->IsSpeakerIdEnrollmentActive());

  WaitForScreenExit();

  ExpectCollectedOptIns({});
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
                       BailOutDuringSpeakerIdEnrollment) {
  auto force_lib_assistant_enabled =
      AssistantOptInFlowScreen::ForceLibAssistantEnabledForTesting(true);
  assistant_settings_->set_consent_ui_flags(
      ScopedAssistantSettings::CONSENT_UI_FLAG_SKIP_ACTIVITY_CONTROL);
  assistant_settings_->set_speaker_id_enrollment_mode(
      ScopedAssistantSettings::SpeakerIdEnrollmentMode::STEP_BY_STEP);

  SetUpAssistantScreensForTest();
  AssistantState::Get()->NotifyStatusChanged(assistant::AssistantStatus::READY);

  ShowAssistantOptInFlowScreen();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
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
  AssistantState::Get()->NotifyStatusChanged(assistant::AssistantStatus::READY);

  ShowAssistantOptInFlowScreen();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
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

  AssistantState::Get()->NotifyStatusChanged(assistant::AssistantStatus::READY);
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

  AssistantState::Get()->NotifyStatusChanged(assistant::AssistantStatus::READY);
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
  AssistantState::Get()->NotifyStatusChanged(assistant::AssistantStatus::READY);
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

class AssistantOptInFlowMinorModeTest : public AssistantOptInFlowTest {
 public:
  AssistantOptInFlowMinorModeTest() = default;

  void SetUpOnMainThread() override {
    AssistantOptInFlowTest::SetUpOnMainThread();
    assistant_settings_->set_is_minor_user(true);
  }
};

IN_PROC_BROWSER_TEST_F(AssistantOptInFlowMinorModeTest,
                       AcceptMultipleValuePropConsentsForMinors) {
  auto force_lib_assistant_enabled =
      AssistantOptInFlowScreen::ForceLibAssistantEnabledForTesting(true);
  assistant_settings_->set_setting_zippy_size(2);
  AssistantState::Get()->NotifyStatusChanged(assistant::AssistantStatus::READY);

  SetUpAssistantScreensForTest();
  ShowAssistantOptInFlowScreen();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantValueProp)->Wait();
  EXPECT_FALSE(
      test::OobeJS().GetAttributeBool("inverse", kValuePropNextButton));
  test::OobeJS().ExpectElementText(kSettingsZippyTitle,
                                   kSettingsZippyTitleFirst);
  test::OobeJS().ExpectElementText(
      base::StrCat(
          {kSettingsZippyDescription, kEnsp, kSettingsZippyLearnMoreLink}),
      kSettingsZippyDescriptionFirst);
  test::OobeJS().ExpectElementText(kSettingsZippyAdditionalInfo,
                                   kSettingsZippyAdditionalInfoFirst);
  TapWhenEnabled(kValuePropNextButton);
  EXPECT_FALSE(
      test::OobeJS().GetAttributeBool("inverse", kValuePropNextButton));
  TapWhenEnabled(kValuePropNextButton);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantRelatedInfo)->Wait();
  EXPECT_FALSE(
      test::OobeJS().GetAttributeBool("inverse", kRelatedInfoNextButton));
  TapWhenEnabled(kRelatedInfoNextButton);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantVoiceMatch)->Wait();
  EXPECT_FALSE(
      test::OobeJS().GetAttributeBool("inverse", kVoiceMatchAgreeButton));
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

IN_PROC_BROWSER_TEST_F(AssistantOptInFlowMinorModeTest,
                       DeclineMultipleValuePropConsentsForMinors) {
  auto force_lib_assistant_enabled =
      AssistantOptInFlowScreen::ForceLibAssistantEnabledForTesting(true);
  assistant_settings_->set_setting_zippy_size(2);
  AssistantState::Get()->NotifyStatusChanged(assistant::AssistantStatus::READY);

  SetUpAssistantScreensForTest();
  ShowAssistantOptInFlowScreen();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantValueProp)->Wait();
  TapWhenEnabled(kValuePropSkipButton);
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

IN_PROC_BROWSER_TEST_F(AssistantOptInFlowMinorModeTest,
                       AcceptFirstAndDeclineSecondValuePropConsentsForMinors) {
  auto force_lib_assistant_enabled =
      AssistantOptInFlowScreen::ForceLibAssistantEnabledForTesting(true);
  assistant_settings_->set_setting_zippy_size(2);
  AssistantState::Get()->NotifyStatusChanged(assistant::AssistantStatus::READY);

  SetUpAssistantScreensForTest();
  ShowAssistantOptInFlowScreen();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantValueProp)->Wait();
  TapWhenEnabled(kValuePropNextButton);
  TapWhenEnabled(kValuePropSkipButton);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantRelatedInfo)->Wait();
  TapWhenEnabled(kRelatedInfoNextButton);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantVoiceMatch)->Wait();
  TapWhenEnabled(kVoiceMatchAgreeButton);

  WaitForScreenExit();

  ExpectCollectedOptIns({ScopedAssistantSettings::OptIn::ACTIVITY_CONTROL});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_EQ(assistant::prefs::ConsentStatus::kUnknown,
            prefs->GetInteger(assistant::prefs::kAssistantConsentStatus));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
  EXPECT_EQ(screen_result_.value(), AssistantOptInFlowScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenExitReason, 1);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenStepCompletionTime,
                                     1);
}

IN_PROC_BROWSER_TEST_F(AssistantOptInFlowMinorModeTest,
                       DeclineFirstAndAcceptSecondValuePropConsentsForMinors) {
  auto force_lib_assistant_enabled =
      AssistantOptInFlowScreen::ForceLibAssistantEnabledForTesting(true);
  assistant_settings_->set_setting_zippy_size(2);
  AssistantState::Get()->NotifyStatusChanged(assistant::AssistantStatus::READY);

  SetUpAssistantScreensForTest();
  ShowAssistantOptInFlowScreen();

  OobeScreenWaiter screen_waiter(AssistantOptInFlowScreenView::kScreenId);
  screen_waiter.Wait();

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantValueProp)->Wait();
  TapWhenEnabled(kValuePropSkipButton);
  TapWhenEnabled(kValuePropNextButton);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantRelatedInfo)->Wait();
  TapWhenEnabled(kRelatedInfoNextButton);

  test::OobeJS().CreateVisibilityWaiter(true, kAssistantVoiceMatch)->Wait();
  TapWhenEnabled(kVoiceMatchAgreeButton);

  WaitForScreenExit();

  ExpectCollectedOptIns({ScopedAssistantSettings::OptIn::ACTIVITY_CONTROL});
  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_EQ(assistant::prefs::ConsentStatus::kUnknown,
            prefs->GetInteger(assistant::prefs::kAssistantConsentStatus));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_TRUE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
  EXPECT_EQ(screen_result_.value(), AssistantOptInFlowScreen::Result::NEXT);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenExitReason, 1);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenStepCompletionTime,
                                     1);
}

class AssistantOptInFlowSkipFeatureTest : public AssistantOptInFlowBaseTest {
 public:
  AssistantOptInFlowSkipFeatureTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kOobeSkipAssistant);
  }
};

IN_PROC_BROWSER_TEST_F(AssistantOptInFlowSkipFeatureTest, AssistantSkipped) {
  AssistantState::Get()->NotifyStatusChanged(assistant::AssistantStatus::READY);
  ShowAssistantOptInFlowScreen();
  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(),
            AssistantOptInFlowScreen::Result::NOT_APPLICABLE);

  ExpectCollectedOptIns({});
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenExitReason, 0);
  histogram_tester_.ExpectTotalCount(kAssistantOptInScreenStepCompletionTime,
                                     0);

  PrefService* const prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  EXPECT_FALSE(prefs->GetBoolean(assistant::prefs::kAssistantHotwordEnabled));
  EXPECT_FALSE(prefs->GetBoolean(assistant::prefs::kAssistantContextEnabled));
}

}  // namespace
}  // namespace ash
