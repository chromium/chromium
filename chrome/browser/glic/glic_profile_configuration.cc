// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_profile_configuration.h"

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/launcher/glic_background_mode_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/glic_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace glic {

GlicProfileConfiguration::GlicProfileConfiguration(Profile* profile)
    : profile_(*profile) {
  pref_registrar_.Init(profile_->GetPrefs());
  pref_registrar_.Add(
      prefs::kGlicEnabledByPolicy,
      base::BindRepeating(&GlicProfileConfiguration::OnEnabledByPolicyChanged,
                          base::Unretained(this)));
}

GlicProfileConfiguration::~GlicProfileConfiguration() = default;

// static
void GlicProfileConfiguration::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kGlicEnabledByPolicy, true);
  registry->RegisterBooleanPref(prefs::kGlicMicrophoneEnabled, false);
  registry->RegisterBooleanPref(prefs::kGlicGeolocationEnabled, false);
  registry->RegisterBooleanPref(prefs::kGlicTabContextEnabled, false);
  registry->RegisterBooleanPref(prefs::kGlicCompletedFre, false);
}

bool GlicProfileConfiguration::IsEnabledByPolicy() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kGlicEnabledByPolicy);
}

void GlicProfileConfiguration::OnEnabledByPolicyChanged() {
  // Note: the pref listener can sometimes fire even if the value from
  // GetBoolean doesn't change (e.g. value was set from multiple sources). See
  // GlicPolicyTest.PrefDisabledByPolicy for an example.
  for (Browser* const browser : *BrowserList::GetInstance()) {
    if (browser->profile() == &profile_.get()) {
      TabStripRegionView* tab_strip_region_view =
          browser->window()->AsBrowserView()->tab_strip_region_view();
      CHECK(tab_strip_region_view);
      tab_strip_region_view->GetTabStripActionContainer()->UpdateGlicButton();
    }
  }
  GlicBackgroundModeManager::GetInstance()->OnPolicyChanged();
}

bool GlicProfileConfiguration::HasCompletedFre() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kGlicCompletedFre);
}

}  // namespace glic
