// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_button_controller.h"

#include "chrome/browser/glic/browser_ui/glic_button_controller_delegate.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/prefs/pref_service.h"
#include "ui/views/widget/widget.h"

namespace glic {

GlicButtonController::GlicButtonController(
    Profile* profile,
    BrowserWindowInterface& browser,
    GlicButtonControllerDelegate* delegate,
    GlicKeyedService* service)
    : profile_(profile),
      browser_(browser),
      glic_controller_delegate_(delegate),
      glic_keyed_service_(service) {
  CHECK(glic_controller_delegate_);
  CHECK(glic_keyed_service_);

  // Set initial button state.
  UpdateButton();

  // Observe for changes in preferences and panel state events.
  pref_registrar_.Init(profile_->GetPrefs());

  auto update_callback = base::BindRepeating(
      &GlicButtonController::UpdateButton, base::Unretained(this));

  pref_registrar_.Add(glic::prefs::kGlicPinnedToTabstrip, update_callback);
  subscriptions_.push_back(
      glic_keyed_service_->enabling().RegisterAllowedChanged(update_callback));
  subscriptions_.push_back(
      glic_keyed_service_->window_controller().AddGlobalShowHideCallback(
          update_callback));
}

GlicButtonController::~GlicButtonController() = default;

void GlicButtonController::UpdateButton() {
  const bool is_enabled_for_profile =
      GlicEnabling::IsEnabledForProfile(profile_);
  const bool is_pinned_to_tabstrip =
      profile_->GetPrefs()->GetBoolean(prefs::kGlicPinnedToTabstrip);
  if (!is_enabled_for_profile || !is_pinned_to_tabstrip) {
    // If the button shouldn't be shown, just hide it.
    return glic_controller_delegate_->SetGlicShowState(false);
  }

  glic_controller_delegate_->SetGlicShowState(true);

  // Try preloading since we know the button is visible.
  glic_keyed_service_->TryPreload();

  if (base::FeatureList::IsEnabled(features::kGlicButtonPressedState)) {
    glic_controller_delegate_->SetGlicPanelIsOpen(
        glic_keyed_service_->IsPanelShowingForBrowser(*browser_));
  }
}

}  // namespace glic
