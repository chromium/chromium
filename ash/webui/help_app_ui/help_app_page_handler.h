// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_HELP_APP_UI_HELP_APP_PAGE_HANDLER_H_
#define ASH_WEBUI_HELP_APP_UI_HELP_APP_PAGE_HANDLER_H_

#include "ash/webui/help_app_ui/help_app_ui.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class GURL;
class PrefService;

namespace ash {

class HelpAppUI;

// Implements the help_app mojom interface providing chrome://help-app
// with browser process functions to call from the render process.
class HelpAppPageHandler : public help_app::mojom::PageHandler {
 public:
  HelpAppPageHandler(
      HelpAppUI* help_app_ui,
      mojo::PendingReceiver<help_app::mojom::PageHandler> receiver,
      base::raw_ref<PrefService> pref_service);
  ~HelpAppPageHandler() override;

  HelpAppPageHandler(const HelpAppPageHandler&) = delete;
  HelpAppPageHandler& operator=(const HelpAppPageHandler&) = delete;

  // help_app::mojom::PageHandler:
  void OpenFeedbackDialog(OpenFeedbackDialogCallback callback) override;
  void ShowOnDeviceAppControls() override;
  void ShowParentalControls() override;
  void TriggerWelcomeTipCallToAction(
      help_app::mojom::ActionTypeId action_type_id) override;
  void IsLauncherSearchEnabled(
      IsLauncherSearchEnabledCallback callback) override;
  void LaunchMicrosoft365Setup() override;
  void MaybeShowReleaseNotesNotification() override;
  void GetDeviceInfo(GetDeviceInfoCallback callback) override;
  void OpenUrlInBrowserAndTriggerInstallDialog(const GURL& url) override;
  void OpenSettings(help_app::mojom::SettingsComponent component) override;
  void SetHasCompletedNewDeviceChecklist() override;
  void SetHasVisitedHowToPage() override;

 private:
  mojo::Receiver<help_app::mojom::PageHandler> receiver_;
  raw_ptr<HelpAppUI> help_app_ui_;  // Owns |this|.
  bool is_launcher_search_enabled_;
  raw_ref<PrefService> pref_service_;
};

}  // namespace ash

#endif  // ASH_WEBUI_HELP_APP_UI_HELP_APP_PAGE_HANDLER_H_
