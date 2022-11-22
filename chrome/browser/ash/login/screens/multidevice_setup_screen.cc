// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/multidevice_setup_screen.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/ash/multidevice_setup/oobe_completion_tracker_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/multidevice_setup_screen_handler.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/oobe_completion_tracker.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash {

namespace {

constexpr const char kAcceptedSetupUserAction[] = "setup-accepted";
constexpr const char kDeclinedSetupUserAction[] = "setup-declined";

}  // namespace

// static
std::string MultiDeviceSetupScreen::GetResultString(Result result) {
  switch (result) {
    case Result::NEXT:
      return "Next";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
}

MultiDeviceSetupScreen::MultiDeviceSetupScreen(
    base::WeakPtr<MultiDeviceSetupScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(MultiDeviceSetupScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {
  DCHECK(view_);
}

MultiDeviceSetupScreen::~MultiDeviceSetupScreen() = default;

void MultiDeviceSetupScreen::TryInitSetupClient() {
  if (!setup_client_) {
    setup_client_ =
        multidevice_setup::MultiDeviceSetupClientFactory::GetForProfile(
            ProfileManager::GetActiveUserProfile());
  }
}

bool MultiDeviceSetupScreen::MaybeSkip(WizardContext& context) {
  // Only attempt the setup flow for non-guest users.
  if (context.skip_post_login_screens_for_tests ||
      chrome_user_manager_util::IsPublicSessionOrEphemeralLogin()) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }

  TryInitSetupClient();
  // If there is no eligible multi-device host phone or if there is a phone and
  // it has already been set, skip the setup flow.
  if (!setup_client_) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }
  if (setup_client_->GetHostStatus().first !=
      multidevice_setup::mojom::HostStatus::kEligibleHostExistsButNoHostSet) {
    VLOG(1) << "Skipping MultiDevice setup screen; host status: "
            << setup_client_->GetHostStatus().first;
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }

  return false;
}

void MultiDeviceSetupScreen::ShowImpl() {
  if (view_) {
    view_->Show();
  }

  // Record that user was presented with setup flow to prevent spam
  // notifications from suggesting setup in the future.
  multidevice_setup::OobeCompletionTracker* oobe_completion_tracker =
      multidevice_setup::OobeCompletionTrackerFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile());
  DCHECK(oobe_completion_tracker);
  oobe_completion_tracker->MarkOobeShown();
}

void MultiDeviceSetupScreen::HideImpl() {}

void MultiDeviceSetupScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();

  if (action_id == kAcceptedSetupUserAction) {
    RecordMultiDeviceSetupOOBEUserChoiceHistogram(
        MultiDeviceSetupOOBEUserChoice::kAccepted);
    exit_callback_.Run(Result::NEXT);
  } else if (action_id == kDeclinedSetupUserAction) {
    RecordMultiDeviceSetupOOBEUserChoiceHistogram(
        MultiDeviceSetupOOBEUserChoice::kDeclined);
    exit_callback_.Run(Result::NEXT);
  } else {
    BaseScreen::OnUserAction(args);
    NOTREACHED();
  }
}

void MultiDeviceSetupScreen::RecordMultiDeviceSetupOOBEUserChoiceHistogram(
    MultiDeviceSetupOOBEUserChoice value) {
  UMA_HISTOGRAM_ENUMERATION("MultiDeviceSetup.OOBE.UserChoice", value);
}

}  // namespace ash
