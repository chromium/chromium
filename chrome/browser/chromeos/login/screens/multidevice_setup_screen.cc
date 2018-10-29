// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/multidevice_setup_screen.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/login/screens/multidevice_setup_screen_view.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/chromeos/multidevice_setup/oobe_completion_tracker_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/services/multidevice_setup/public/cpp/multidevice_setup_client.h"
#include "chromeos/services/multidevice_setup/public/cpp/oobe_completion_tracker.h"

namespace chromeos {

namespace {

constexpr const char kFinishedUserAction[] = "setup-finished";

}  // namespace

MultiDeviceSetupScreen::MultiDeviceSetupScreen(
    BaseScreenDelegate* base_screen_delegate,
    MultiDeviceSetupScreenView* view)
    : BaseScreen(base_screen_delegate, OobeScreen::SCREEN_MULTIDEVICE_SETUP),
      view_(view) {
  DCHECK(view_);
  view_->Bind(this);
}

MultiDeviceSetupScreen::~MultiDeviceSetupScreen() {
  view_->Bind(nullptr);
}

void MultiDeviceSetupScreen::Show() {
  // If multi-device flags are disabled, skip the associated setup flow.
  if (!base::FeatureList::IsEnabled(features::kMultiDeviceApi) ||
      !base::FeatureList::IsEnabled(features::kEnableUnifiedMultiDeviceSetup)) {
    ExitScreen();
    return;
  }

  // Only attempt the setup flow for non-guest users.
  if (IsPublicSessionOrEphemeralLogin()) {
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
  if (action_id == kFinishedUserAction) {
    ExitScreen();
    return;
  }

  BaseScreen::OnUserAction(action_id);
}

void MultiDeviceSetupScreen::ExitScreen() {
  Finish(ScreenExitCode::MULTIDEVICE_SETUP_FINISHED);
}

}  // namespace chromeos
