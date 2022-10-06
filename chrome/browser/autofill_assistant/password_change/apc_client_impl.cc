// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_assistant/password_change/apc_client_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/autofill_assistant/common_dependencies_chrome.h"
#include "chrome/browser/autofill_assistant/password_change/apc_external_action_delegate.h"
#include "chrome/browser/autofill_assistant/password_change/apc_onboarding_coordinator.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill_assistant/password_change/apc_scrim_manager.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_display_delegate.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_stopped_bubble_coordinator.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/channel_info.h"
#include "components/autofill_assistant/browser/public/autofill_assistant_factory.h"
#include "components/autofill_assistant/browser/public/headless_script_controller.h"
#include "components/autofill_assistant/browser/public/password_change/website_login_manager_impl.h"
#include "components/autofill_assistant/browser/public/prefs.h"
#include "components/autofill_assistant/browser/public/public_script_parameters.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace {
constexpr char kIntent[] = "PASSWORD_CHANGE";
constexpr char kTrue[] = "true";
constexpr char kFalse[] = "false";

constexpr int kInChromeCaller = 7;
constexpr int kSourcePasswordChangeLeakWarning = 10;
constexpr int kSourcePasswordChangeSettings = 11;

// The command line switch for specifying a custom server URL.
constexpr char kAutofillAssistantUrl[] = "autofill-assistant-url";
}  // namespace

ApcClientImpl::ApcClientImpl(content::WebContents* web_contents)
    : content::WebContentsUserData<ApcClientImpl>(*web_contents) {}

ApcClientImpl::~ApcClientImpl() = default;

void ApcClientImpl::Start(
    const GURL& url,
    const std::string& username,
    bool skip_login,
    ResultCallback callback,
    absl::optional<DebugRunInformation> debug_run_information) {
  // If the unified side panel is not enabled, trying to register an entry in it
  // later on will crash.
  if (!base::FeatureList::IsEnabled(features::kUnifiedSidePanel)) {
    DVLOG(0) << "Unified side panel disabled, stopping APC.";
    std::move(callback).Run(false);
    return;
  }

  // If Autofill Assistant is disabled, do not start.
  PrefService* pref_service =
      Profile::FromBrowserContext(GetWebContents().GetBrowserContext())
          ->GetPrefs();
  if (!pref_service->GetBoolean(
          autofill_assistant::prefs::kAutofillAssistantEnabled)) {
    DVLOG(0) << "Autofill Assistant pref is false, stopping APC.";
    std::move(callback).Run(false);
    return;
  }

  if (GetPasswordManagerClient() == nullptr) {
    DVLOG(0) << "Cannot obtain password manager client, stopping APC.";
    std::move(callback).Run(false);
    return;
  }

  // Ensure that only one run is ongoing.
  if (is_running_) {
    DVLOG(0) << "APC already ongoing, not starting a new run.";
    std::move(callback).Run(false);
    return;
  }
  is_running_ = true;
  result_callback_ = std::move(callback);

  GetRuntimeManager()->SetUIState(autofill_assistant::UIState::kShown);

  url_ = url;
  username_ = username;
  skip_login_ = skip_login;
  debug_run_information_ = debug_run_information;

  // The coordinator takes care of checking whether a user has previously given
  // consent and, if not, prompts the user to give consent now.
  onboarding_coordinator_ = CreateOnboardingCoordinator();
  onboarding_coordinator_->PerformOnboarding(base::BindOnce(
      &ApcClientImpl::OnOnboardingComplete, base::Unretained(this)));
}

void ApcClientImpl::Stop(bool success) {
  GetRuntimeManager()->SetUIState(autofill_assistant::UIState::kNotShown);
  onboarding_coordinator_.reset();
  external_script_controller_.reset();
  scrim_manager_.reset();
  is_running_ = false;
  if (result_callback_)
    std::move(result_callback_).Run(success);
}

bool ApcClientImpl::IsRunning() const {
  return is_running_;
}

void ApcClientImpl::PromptForConsent(OnboardingResultCallback callback) {
  if (is_running_) {
    // If a run is ongoing and beyond the onboarding stage, consent must have
    // been given.
    std::move(callback).Run(onboarding_coordinator_ == nullptr);
    return;
  }
  is_running_ = true;

  onboarding_coordinator_ = CreateOnboardingCoordinator();
  onboarding_coordinator_->PerformOnboarding(std::move(callback).Then(
      base::BindOnce(&ApcClientImpl::Stop, base::Unretained(this), false)));
}

void ApcClientImpl::RevokeConsent(const std::vector<int>& description_grd_ids) {
  if (is_running_)
    Stop(false);

  onboarding_coordinator_ = CreateOnboardingCoordinator();
  onboarding_coordinator_->RevokeConsent(description_grd_ids);
  onboarding_coordinator_.reset();
}

base::flat_map<std::string, std::string> ApcClientImpl::GetScriptParameters()
    const {
  base::flat_map<std::string, std::string> params_map = {
      {autofill_assistant::public_script_parameters::
           kPasswordChangeUsernameParameterName,
       username_},
      {autofill_assistant::public_script_parameters::kIntentParameterName,
       kIntent},
      {autofill_assistant::public_script_parameters::
           kStartImmediatelyParameterName,
       kTrue},
      {autofill_assistant::public_script_parameters::
           kOriginalDeeplinkParameterName,
       url_.spec()},
      {autofill_assistant::public_script_parameters::
           kPasswordChangeSkipLoginParameterName,
       skip_login_ ? kTrue : kFalse},
      {autofill_assistant::public_script_parameters::kEnabledParameterName,
       kTrue},
      {autofill_assistant::public_script_parameters::kCallerParameterName,
       base::NumberToString(kInChromeCaller)},
      {autofill_assistant::public_script_parameters::kSourceParameterName,
       skip_login_ ? base::NumberToString(kSourcePasswordChangeLeakWarning)
                   : base::NumberToString(kSourcePasswordChangeSettings)}};

  if (debug_run_information_.has_value()) {
    params_map[autofill_assistant::public_script_parameters::
                   kDebugBundleIdParameterName] =
        debug_run_information_.value().bundle_id;
    params_map[autofill_assistant::public_script_parameters::
                   kDebugSocketIdParameterName] =
        debug_run_information_.value().socket_id;
  }

  // TODO(b/251365675): Remove once all endpoints support RPC signing.
  if (!base::CommandLine::ForCurrentProcess()
           ->GetSwitchValueASCII(kAutofillAssistantUrl)
           .empty()) {
    DVLOG(0) << __func__
             << " custom server URL provided - CUP will not be used.";
    params_map[autofill_assistant::public_script_parameters::
                   kDisableRpcSigningParameterName] = kTrue;
  }

  return params_map;
}

// `success` indicates whether onboarding was successful, i.e. whether consent
// has been given.
void ApcClientImpl::OnOnboardingComplete(bool success) {
  onboarding_coordinator_.reset();
  if (!success) {
    Stop(/*success=*/false);
    return;
  }

  // Only create a new sidepanel coordinator if there is not one already shown.
  if (!side_panel_coordinator_) {
    side_panel_coordinator_ = CreateSidePanel();
    if (!side_panel_coordinator_) {
      Stop(/*success=*/false);
      return;
    }
    side_panel_coordinator_->AddObserver(this);
  }

  assistant_stopped_bubble_coordinator_ =
      CreateAssistantStoppedBubbleCoordinator();

  scrim_manager_ = CreateApcScrimManager();
  website_login_manager_ = CreateWebsiteLoginManager();

  apc_external_action_delegate_ = CreateApcExternalActionDelegate();
  apc_external_action_delegate_->SetupDisplay();
  apc_external_action_delegate_->ShowStartingScreen(url_);

  external_script_controller_ = CreateHeadlessScriptController();
  scrim_manager_->Show();
  external_script_controller_->StartScript(
      GetScriptParameters(),
      base::BindOnce(&ApcClientImpl::OnRunComplete, base::Unretained(this)));
}

void ApcClientImpl::OnRunComplete(
    autofill_assistant::HeadlessScriptController::ScriptResult result) {
  Stop(result.success);

  if (!result.success) {
    apc_external_action_delegate_->ShowErrorScreen();
    return;
  }

  if (apc_external_action_delegate_->PasswordWasSuccessfullyChanged()) {
    apc_external_action_delegate_->ShowCompletionScreen(base::BindRepeating(
        &ApcClientImpl::CloseSidePanel, base::Unretained(this)));
  } else {
    CloseSidePanel();
  }
}

void ApcClientImpl::OnHidden() {
  if (is_running_) {
    assistant_stopped_bubble_coordinator_->Show();
  }
  Stop(/*success=*/false);

  // The two resets below are not included in `Stop()`, since we may wish to
  // render content in the side panel even for a stopped flow.
  apc_external_action_delegate_.reset();
  side_panel_coordinator_.reset();
}

void ApcClientImpl::CloseSidePanel() {
  side_panel_coordinator_.reset();
}

std::unique_ptr<AssistantStoppedBubbleCoordinator>
ApcClientImpl::CreateAssistantStoppedBubbleCoordinator() {
  return AssistantStoppedBubbleCoordinator::Create(&GetWebContents(), url_,
                                                   username_);
}
std::unique_ptr<ApcOnboardingCoordinator>
ApcClientImpl::CreateOnboardingCoordinator() {
  return ApcOnboardingCoordinator::Create(&GetWebContents());
}

std::unique_ptr<AssistantSidePanelCoordinator>
ApcClientImpl::CreateSidePanel() {
  return AssistantSidePanelCoordinator::Create(&GetWebContents());
}

std::unique_ptr<autofill_assistant::HeadlessScriptController>
ApcClientImpl::CreateHeadlessScriptController() {
  DCHECK(scrim_manager_);
  DCHECK(apc_external_action_delegate_);
  DCHECK(website_login_manager_);

  std::unique_ptr<autofill_assistant::AutofillAssistant> autofill_assistant =
      autofill_assistant::AutofillAssistantFactory::CreateForBrowserContext(
          GetWebContents().GetBrowserContext(),
          std::make_unique<autofill_assistant::CommonDependenciesChrome>(
              GetWebContents().GetBrowserContext()));
  return autofill_assistant->CreateHeadlessScriptController(
      &GetWebContents(), apc_external_action_delegate_.get(),
      website_login_manager_.get());
}

autofill_assistant::RuntimeManager* ApcClientImpl::GetRuntimeManager() {
  return autofill_assistant::RuntimeManager::GetOrCreateForWebContents(
      &GetWebContents());
}

std::unique_ptr<ApcScrimManager> ApcClientImpl::CreateApcScrimManager() {
  return ApcScrimManager::Create(&GetWebContents());
}

std::unique_ptr<ApcExternalActionDelegate>
ApcClientImpl::CreateApcExternalActionDelegate() {
  DCHECK(scrim_manager_);
  DCHECK(website_login_manager_);

  return std::make_unique<ApcExternalActionDelegate>(
      &GetWebContents(), side_panel_coordinator_.get(), scrim_manager_.get(),
      website_login_manager_.get());
}

std::unique_ptr<autofill_assistant::WebsiteLoginManager>
ApcClientImpl::CreateWebsiteLoginManager() {
  return std::make_unique<autofill_assistant::WebsiteLoginManagerImpl>(
      GetPasswordManagerClient(), &GetWebContents());
}

password_manager::PasswordManagerClient*
ApcClientImpl::GetPasswordManagerClient() {
  return ChromePasswordManagerClient::FromWebContents(&GetWebContents());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ApcClientImpl);
