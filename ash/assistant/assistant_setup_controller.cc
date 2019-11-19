// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_setup_controller.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/assistant/util/i18n_util.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/services/assistant/public/features.h"

namespace {

constexpr char kGSuiteAdministratorInstructionsUrl[] =
    "https://support.google.com/a/answer/6304876";

}  // namespace

namespace ash {

AssistantSetupController::AssistantSetupController(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller) {
  assistant_controller_->AddObserver(this);
}

AssistantSetupController::~AssistantSetupController() {
  assistant_controller_->RemoveObserver(this);
}

void AssistantSetupController::OnAssistantControllerConstructed() {
  assistant_controller_->view_delegate()->AddObserver(this);
}

void AssistantSetupController::OnAssistantControllerDestroying() {
  assistant_controller_->view_delegate()->RemoveObserver(this);
}

void AssistantSetupController::OnDeepLinkReceived(
    assistant::util::DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  if (type != assistant::util::DeepLinkType::kOnboarding)
    return;

  base::Optional<bool> relaunch = assistant::util::GetDeepLinkParamAsBool(
      params, assistant::util::DeepLinkParam::kRelaunch);

  StartOnboarding(relaunch.value_or(false));
}

void AssistantSetupController::OnOptInButtonPressed() {
  if (AssistantState::Get()->consent_status().value_or(
          chromeos::assistant::prefs::ConsentStatus::kUnknown) ==
      chromeos::assistant::prefs::ConsentStatus::kUnauthorized) {
    assistant_controller_->OpenUrl(assistant::util::CreateLocalizedGURL(
        kGSuiteAdministratorInstructionsUrl));
  } else {
    StartOnboarding(/*relaunch=*/true);
  }
}

void AssistantSetupController::StartOnboarding(bool relaunch, FlowType type) {
  auto* assistant_setup = AssistantSetup::GetInstance();
  if (!assistant_setup)
    return;

  if (relaunch) {
    assistant_setup->StartAssistantOptInFlow(
        type, base::BindOnce(&AssistantSetupController::OnOptInFlowFinished,
                             weak_ptr_factory_.GetWeakPtr()));
  } else {
    assistant_setup->StartAssistantOptInFlow(type, base::DoNothing());
  }

  // Assistant UI should be hidden while the user onboards.
  assistant_controller_->ui_controller()->HideUi(AssistantExitPoint::kSetup);
}

void AssistantSetupController::OnOptInFlowFinished(bool completed) {
  if (completed)
    assistant_controller_->ui_controller()->ShowUi(AssistantEntryPoint::kSetup);
}

}  // namespace ash
