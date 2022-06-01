// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_assistant/password_change/apc_client_impl.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/autofill_assistant/common_dependencies_chrome.h"
#include "chrome/browser/autofill_assistant/password_change/apc_external_action_delegate.h"
#include "chrome/browser/autofill_assistant/password_change/apc_onboarding_coordinator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_display_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/channel_info.h"
#include "components/autofill_assistant/browser/public/autofill_assistant_factory.h"
#include "components/autofill_assistant/browser/public/external_script_controller.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

// TODO(b/234418435): Remove these values from here once exposed in
// autofill_assistant/browser/public.
namespace {
constexpr char kPasswordChangeUsername[] = "PASSWORD_CHANGE_USERNAME";
constexpr char kPasswordChangeSkipLoginParameter[] =
    "PASSWORD_CHANGE_SKIP_LOGIN";
constexpr char kIntentParameter[] = "INTENT";
constexpr char kSourceParameter[] = "SOURCE";
constexpr char kIntent[] = "PASSWORD_CHANGE";
constexpr char kParameterStartImediately[] = "START_IMMEDIATELY";
constexpr char kParameterOriginalDeepLink[] = "ORIGINAL_DEEPLINK";
constexpr char kParameterEnabled[] = "ENABLED";
constexpr char kParameterCaller[] = "CALLER";

constexpr int kInChromeCaller = 7;
constexpr int kSourcePasswordChangeLeakWarning = 10;
constexpr int kSourcePasswordChangeSettings = 11;
}  // namespace

ApcClientImpl::ApcClientImpl(content::WebContents* web_contents)
    : content::WebContentsUserData<ApcClientImpl>(*web_contents) {}

ApcClientImpl::~ApcClientImpl() = default;

bool ApcClientImpl::Start(const GURL& url,
                          const std::string& username,
                          bool skip_login) {
  // If the unified side panel is not enabled, trying to register an entry in it
  // later on will crash.
  if (!base::FeatureList::IsEnabled(features::kUnifiedSidePanel))
    return false;

  // Ensure that only one run is ongoing.
  if (is_running_)
    return false;
  is_running_ = true;

  url_ = url;
  username_ = username;
  skip_login_ = skip_login;

  // The coordinator takes care of checking whether a user has previously given
  // consent and, if not, prompts the user to give consent now.
  onboarding_coordinator_ = CreateOnboardingCoordinator();
  onboarding_coordinator_->PerformOnboarding(base::BindOnce(
      &ApcClientImpl::OnOnboardingComplete, base::Unretained(this)));

  return true;
}

void ApcClientImpl::Stop() {
  onboarding_coordinator_.reset();
  external_script_controller_.reset();
  is_running_ = false;
}

bool ApcClientImpl::IsRunning() const {
  return is_running_;
}

// `success` indicates whether onboarding was successful, i.e. whether consent
// has been given.
void ApcClientImpl::OnOnboardingComplete(bool success) {
  if (!success) {
    Stop();
    return;
  }

  base::flat_map<std::string, std::string> params_map;
  params_map[kPasswordChangeUsername] = username_;
  params_map[kIntentParameter] = kIntent;
  params_map[kParameterStartImediately] = "true";
  params_map[kParameterOriginalDeepLink] = url_.spec();
  params_map[kPasswordChangeSkipLoginParameter] =
      skip_login_ ? "true" : "false";
  params_map[kParameterEnabled] = "true";
  params_map[kParameterCaller] = base::NumberToString(kInChromeCaller);
  params_map[kSourceParameter] =
      skip_login_ ? base::NumberToString(kSourcePasswordChangeLeakWarning)
                  : base::NumberToString(kSourcePasswordChangeSettings);

  external_script_controller_ = CreateExternalScriptController();
  external_script_controller_->StartScript(
      params_map,
      base::BindOnce(&ApcClientImpl::OnRunComplete, base::Unretained(this)));
}

void ApcClientImpl::OnRunComplete(
    autofill_assistant::ExternalScriptController::ScriptResult result) {
  // TODO(crbug.com/1324089): Handle failed result.
  Stop();
}

void ApcClientImpl::OnHidden() {
  // The side panel was hidden, so we need to destroy it.
  side_panel_coordinator_.reset();

  // TODO(crbug.com/1324089): Destroy the ApcExternalAction delegate and decide
  // whether to log any data about the shutdown.

  Stop();
}

std::unique_ptr<ApcOnboardingCoordinator>
ApcClientImpl::CreateOnboardingCoordinator() {
  return ApcOnboardingCoordinator::Create(&GetWebContents());
}

std::unique_ptr<autofill_assistant::ExternalScriptController>
ApcClientImpl::CreateExternalScriptController() {
  side_panel_coordinator_ =
      AssistantSidePanelCoordinator::Create(&GetWebContents());
  apc_external_action_delegate_ = std::make_unique<ApcExternalActionDelegate>(
      side_panel_coordinator_.get());
  apc_external_action_delegate_->SetupDisplay();

  std::unique_ptr<autofill_assistant::AutofillAssistant> autofill_assistant =
      autofill_assistant::AutofillAssistantFactory::CreateForBrowserContext(
          GetWebContents().GetBrowserContext(),
          std::make_unique<autofill_assistant::CommonDependenciesChrome>());
  return autofill_assistant->CreateExternalScriptController(
      &GetWebContents(), apc_external_action_delegate_.get());
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ApcClientImpl);
