// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_fre_controller.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/version_info/channel.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/glic_fre_dialog_view.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/launcher/glic_launcher_configuration.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "components/prefs/pref_service.h"

namespace glic {

constexpr static int kFreDefaultWidth = 512;
constexpr static int kFreDefaultHeight = 614;

GlicFreController::GlicFreController() = default;

GlicFreController::~GlicFreController() = default;

bool GlicFreController::ShouldShowFreDialog(Profile* profile) {
  // If the given profile has not previously completed the FRE, then it should
  // be shown.
  // Always show the FRE if `--glic-always-open-fre` is present, for
  // testing convenience.
  auto* command_line = base::CommandLine::ForCurrentProcess();
  return !profile->GetPrefs()->GetBoolean(prefs::kGlicCompletedFre) ||
         command_line->HasSwitch(::switches::kGlicAlwaysOpenFre);
}

bool GlicFreController::CanShowFreDialog(Browser* browser) {
  // The FRE can only be shown given a valid browser. If there is no browser,
  // then an OS-level entrypoint is being used, which should not be possible
  // before the FRE has been accepted.
  if (!browser) {
    return false;
  }
  // If there is a browser, the FRE can also only be shown if no
  // other modal is currently being shown on the same tab.
  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(
      browser->tab_strip_model()->GetActiveWebContents());
  return tab && tab->CanShowModalUI();
}

void GlicFreController::ShowFreDialog(Profile* profile, Browser* browser) {
  // Close any existing FRE dialog before showing.
  DismissFre();
  fre_view_ = new GlicFreDialogView(
      profile, gfx::Size(kFreDefaultWidth, kFreDefaultHeight));

  tabs::TabInterface* tab_interface = tabs::TabInterface::GetFromContents(
      browser->tab_strip_model()->GetActiveWebContents());
  // TODO(crbug.com/393400004): This returned widget should be configured to use
  // a synchronous close.
  fre_widget_ = tab_interface->GetTabFeatures()
                    ->tab_dialog_manager()
                    ->CreateShowDialogAndBlockTabInteraction(fre_view_);
}

void GlicFreController::AcceptFre(Profile* profile) {
  // Update FRE related preferences.
  profile->GetPrefs()->SetBoolean(prefs::kGlicCompletedFre, true);

  // Enable the launcher if it is still disabled by default and the browser
  // is default or is on the stable channel.
  bool is_enabled_default = false;
  const bool is_launcher_enabled =
      GlicLauncherConfiguration::IsEnabled(&is_enabled_default);
  if (is_enabled_default && !is_launcher_enabled) {
    base::MakeRefCounted<shell_integration::DefaultBrowserWorker>()
        ->StartCheckIsDefault(
            base::BindOnce(&GlicFreController::OnCheckIsDefaultBrowserFinished,
                           chrome::GetChannel()));
  }

  DismissFre();

  // Show a glic window attached to the last active browser of the glic
  // profile, which should correspond to the browser used by the FRE.
  if (Browser* new_attached_browser =
          chrome::FindLastActiveWithProfile(profile)) {
    glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile)->ToggleUI(
        new_attached_browser, /*prevent_close=*/true, InvocationSource::kFre);
  }
}

void GlicFreController::DismissFre() {
  if (fre_widget_) {
    fre_view_ = nullptr;
    fre_widget_.reset();
  }
}

content::WebContents* GlicFreController::GetWebContents() {
  if (!fre_view_) {
    return nullptr;
  }
  return fre_view_->web_contents();
}

// static
void GlicFreController::OnCheckIsDefaultBrowserFinished(
    version_info::Channel channel,
    shell_integration::DefaultWebClientState state) {
  // Don't do anything because a different channel is the default browser
  if (state ==
      shell_integration::DefaultWebClientState::OTHER_MODE_IS_DEFAULT) {
    return;
  }

  // Enables the launcher if the current browser is the default or
  // is on the stable channel.
  if (g_browser_process &&
      (state == shell_integration::DefaultWebClientState::IS_DEFAULT ||
       channel == version_info::Channel::STABLE)) {
    g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                                 true);
  }
}
}  // namespace glic
