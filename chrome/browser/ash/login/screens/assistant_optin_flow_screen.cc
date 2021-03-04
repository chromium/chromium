// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/assistant_optin_flow_screen.h"

#include <memory>

#include "chrome/browser/ash/assistant/assistant_util.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/assistant_optin_flow_screen_handler.h"
#include "chromeos/assistant/buildflags.h"

namespace chromeos {
namespace {

constexpr const char kFlowFinished[] = "flow-finished";

#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
bool g_libassistant_enabled = true;
#else
bool g_libassistant_enabled = false;
#endif

}  // namespace

// static
std::string AssistantOptInFlowScreen::GetResultString(Result result) {
  switch (result) {
    case Result::NEXT:
      return "Next";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
}

AssistantOptInFlowScreen::AssistantOptInFlowScreen(
    AssistantOptInFlowScreenView* view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(AssistantOptInFlowScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  if (view_)
    view_->Bind(this);
}

AssistantOptInFlowScreen::~AssistantOptInFlowScreen() {
  if (view_)
    view_->Unbind();
}

bool AssistantOptInFlowScreen::MaybeSkip(WizardContext* context) {
  if (!g_libassistant_enabled ||
      chrome_user_manager_util::IsPublicSessionOrEphemeralLogin()) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }

  if (::assistant::IsAssistantAllowedForProfile(
          ProfileManager::GetActiveUserProfile()) ==
      chromeos::assistant::AssistantAllowedState::ALLOWED) {
    return false;
  }

  exit_callback_.Run(Result::NOT_APPLICABLE);
  return true;
}

void AssistantOptInFlowScreen::ShowImpl() {
  if (view_)
    view_->Show();
}

void AssistantOptInFlowScreen::HideImpl() {
  if (view_)
    view_->Hide();
}

void AssistantOptInFlowScreen::OnViewDestroyed(
    AssistantOptInFlowScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

// static
std::unique_ptr<base::AutoReset<bool>>
AssistantOptInFlowScreen::ForceLibAssistantEnabledForTesting(bool enabled) {
  return std::make_unique<base::AutoReset<bool>>(&g_libassistant_enabled,
                                                 enabled);
}

void AssistantOptInFlowScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kFlowFinished)
    exit_callback_.Run(Result::NEXT);
  else
    BaseScreen::OnUserAction(action_id);
}

}  // namespace chromeos
