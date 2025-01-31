// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_fre_controller.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_fre_dialog_view.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/public/tab_dialog_manager.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "components/prefs/pref_service.h"

namespace glic {

constexpr static int kFreDefaultWidth = 512;
constexpr static int kFreDefaultHeight = 614;

GlicFreController::GlicFreController() = default;

GlicFreController::~GlicFreController() = default;

bool GlicFreController::ShouldShowFreDialog(Profile* profile) {
  // If the given profile has not previously completed the FRE, then it should
  // be shown.
  // TODO(cuianthony): currently this condition is flipped so as to prevent the
  // FRE from showing in all cases - all existing and new preferences are
  // registered as false. Flip this condition back once the remaining FRE code
  // lands.
  return profile->GetPrefs()->GetBoolean(prefs::kGlicCompletedFre);
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
  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               true);

  DismissFre();

  // Show a glic window attached to the last active browser of the glic
  // profile, which should correspond to the browser used by the FRE.
  if (Browser* new_attached_browser =
          chrome::FindLastActiveWithProfile(profile)) {
    glic::GlicKeyedServiceFactory::GetGlicKeyedService(profile)->ToggleUI(
        new_attached_browser);
  }
}

void GlicFreController::DismissFre() {
  if (fre_widget_) {
    fre_view_ = nullptr;
    fre_widget_.reset();
  }
}

}  // namespace glic
