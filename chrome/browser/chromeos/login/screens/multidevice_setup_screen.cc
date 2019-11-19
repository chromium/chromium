// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/multidevice_setup_screen.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_util.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/chromeos/multidevice_setup/oobe_completion_tracker_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/multidevice_setup_screen_handler.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/services/multidevice_setup/public/cpp/oobe_completion_tracker.h"

namespace chromeos {

namespace {

constexpr const char kAcceptedSetupUserAction[] = "setup-accepted";
constexpr const char kDeclinedSetupUserAction[] = "setup-declined";

}  // namespace

MultiDeviceSetupScreen::MultiDeviceSetupScreen(
    MultiDeviceSetupScreenView* view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(MultiDeviceSetupScreenView::kScreenId),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  view_->Bind(this);
}

MultiDeviceSetupScreen::~MultiDeviceSetupScreen() {
  view_->Bind(nullptr);
}

void MultiDeviceSetupScreen::Show() {
  // Only attempt the setup flow for non-guest users.
  if (chrome_user_manager_util::IsPublicSessionOrEphemeralLogin()) {
    ExitScreen();
    return;
  }

  multidevice_setup::MultiDeviceSetupClient* client =
      multidevice_setup::MultiDeviceSetupClientFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile());

  if (!client) {
    ExitScreen();
    return;
  }

  // If there is no eligible multi-device host phone or if there is a phone and
  // it has already been set, skip the setup flow.
  if (client->GetHostStatus().first !=
      multidevice_setup::mojom::HostStatus::kEligibleHostExistsButNoHostSet) {
    VLOG(1) << "Skipping MultiDevice setup screen; host status: "
            << client->GetHostStatus().first;
    ExitScreen();
    return;
  }

  view_->Show();

  // Record that user was presented with setup flow to prevent spam
  // notifications from suggesting setup in the future.
  multidevice_setup::OobeCompletionTracker* oobe_completion_tracker =
      multidevice_setup::OobeCompletionTrackerFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile());
  DCHECK(oobe_completion_tracker);
  oobe_completion_tracker->MarkOobeShown();
}

void MultiDeviceSetupScreen::Hide() {
  view_->Hide();
}

void MultiDeviceSetupScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kAcceptedSetupUserAction) {
    RecordMultiDeviceSetupOOBEUserChoiceHistogram(
        MultiDeviceSetupOOBEUserChoice::kAccepted);
    ExitScreen();
  } else if (action_id == kDeclinedSetupUserAction) {
    RecordMultiDeviceSetupOOBEUserChoiceHistogram(
        MultiDeviceSetupOOBEUserChoice::kDeclined);
    ExitScreen();
  } else {
    BaseScreen::OnUserAction(action_id);
    NOTREACHED();
  }
}

void MultiDeviceSetupScreen::RecordMultiDeviceSetupOOBEUserChoiceHistogram(
    MultiDeviceSetupOOBEUserChoice value) {
  UMA_HISTOGRAM_ENUMERATION("MultiDeviceSetup.OOBE.UserChoice", value);
}

void MultiDeviceSetupScreen::ExitScreen() {
  exit_callback_.Run();
}

}  // namespace chromeos
