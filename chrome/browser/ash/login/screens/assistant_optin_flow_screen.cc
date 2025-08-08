// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/assistant_optin_flow_screen.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/assistant/assistant_util.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/assistant_optin_flow_screen_handler.h"
#include "chromeos/ash/components/assistant/buildflags.h"

namespace ash {
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
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::NEXT:
      return "Next";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

AssistantOptInFlowScreen::AssistantOptInFlowScreen(
    base::WeakPtr<AssistantOptInFlowScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(AssistantOptInFlowScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {
  DCHECK(view_);
}

AssistantOptInFlowScreen::~AssistantOptInFlowScreen() = default;

bool AssistantOptInFlowScreen::MaybeSkip(WizardContext& context) {
  if (features::IsOobeSkipAssistantEnabled()) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }

  if (context.skip_post_login_screens_for_tests || !g_libassistant_enabled ||
      chrome_user_manager_util::IsManagedGuestSessionOrEphemeralLogin()) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }

  if (::assistant::IsAssistantAllowedForProfile(
          ProfileManager::GetActiveUserProfile()) ==
      assistant::AssistantAllowedState::ALLOWED) {
    return false;
  }

  exit_callback_.Run(Result::NOT_APPLICABLE);
  return true;
}

void AssistantOptInFlowScreen::ShowImpl() {
  if (view_)
    view_->Show();
}

// static
std::unique_ptr<base::AutoReset<bool>>
AssistantOptInFlowScreen::ForceLibAssistantEnabledForTesting(bool enabled) {
  return std::make_unique<base::AutoReset<bool>>(&g_libassistant_enabled,
                                                 enabled);
}

void AssistantOptInFlowScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kFlowFinished)
    exit_callback_.Run(Result::NEXT);
  else
    BaseScreen::OnUserAction(args);
}

}  // namespace ash
