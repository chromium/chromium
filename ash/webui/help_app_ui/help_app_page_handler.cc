// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/help_app_ui/help_app_page_handler.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/webui/help_app_ui/help_app_ui.h"
#include "ash/webui/help_app_ui/help_app_ui_delegate.h"
#include "base/feature_list.h"
#include "url/gurl.h"

namespace ash {

HelpAppPageHandler::HelpAppPageHandler(
    HelpAppUI* help_app_ui,
    mojo::PendingReceiver<help_app::mojom::PageHandler> receiver)
    : receiver_(this, std::move(receiver)),
      help_app_ui_(help_app_ui),
      is_lss_enabled_(
          base::FeatureList::IsEnabled(features::kEnableLocalSearchService)),
      is_launcher_search_enabled_(
          base::FeatureList::IsEnabled(features::kHelpAppLauncherSearch) &&
          base::FeatureList::IsEnabled(features::kEnableLocalSearchService)) {}

HelpAppPageHandler::~HelpAppPageHandler() = default;

void HelpAppPageHandler::OpenFeedbackDialog(
    OpenFeedbackDialogCallback callback) {
  auto error_message = help_app_ui_->delegate()->OpenFeedbackDialog();
  std::move(callback).Run(std::move(error_message));
}

void HelpAppPageHandler::ShowOnDeviceAppControls() {
  help_app_ui_->delegate()->ShowOnDeviceAppControls();
}

void HelpAppPageHandler::ShowParentalControls() {
  help_app_ui_->delegate()->ShowParentalControls();
}

void HelpAppPageHandler::TriggerWelcomeTipCallToAction(
    help_app::mojom::ActionTypeId action_type_id) {
  help_app_ui_->delegate()->TriggerWelcomeTipCallToAction(action_type_id);
}

void HelpAppPageHandler::IsLssEnabled(IsLssEnabledCallback callback) {
  std::move(callback).Run(is_lss_enabled_);
}

void HelpAppPageHandler::IsLauncherSearchEnabled(
    IsLauncherSearchEnabledCallback callback) {
  std::move(callback).Run(is_launcher_search_enabled_);
}

void HelpAppPageHandler::LaunchMicrosoft365Setup() {
  help_app_ui_->delegate()->LaunchMicrosoft365Setup();
}

void HelpAppPageHandler::MaybeShowReleaseNotesNotification() {
  help_app_ui_->delegate()->MaybeShowReleaseNotesNotification();
}

void HelpAppPageHandler::GetDeviceInfo(GetDeviceInfoCallback callback) {
  help_app_ui_->delegate()->GetDeviceInfo(std::move(callback));
}

void HelpAppPageHandler::OpenUrlInBrowserAndTriggerInstallDialog(
    const GURL& url) {
  auto error_message =
      help_app_ui_->delegate()->OpenUrlInBrowserAndTriggerInstallDialog(url);
  if (error_message.has_value()) {
    receiver_.ReportBadMessage(error_message.value());
  }
}

void HelpAppPageHandler::OpenSettings(
    help_app::mojom::SettingsComponent component) {
  help_app_ui_->delegate()->OpenSettings(component);
}

}  // namespace ash
