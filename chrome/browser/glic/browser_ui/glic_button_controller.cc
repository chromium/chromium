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
    GlicButtonControllerDelegate* delegate,
    GlicKeyedService* service)
    : profile_(profile),
      glic_controller_delegate_(delegate),
      glic_keyed_service_(service) {
  CHECK(glic_controller_delegate_);
  CHECK(glic_keyed_service_);

  // Initialize default values
  PanelStateChanged(
      glic_keyed_service_->window_controller().GetGlobalPanelState(), {});

  // Observe for changes in preferences and panel state events
  pref_registrar_.Init(profile_->GetPrefs());
  pref_registrar_.Add(glic::prefs::kGlicPinnedToTabstrip,
                      base::BindRepeating(&GlicButtonController::OnPrefsChanged,
                                          base::Unretained(this)));
  subscriptions_.push_back(
      glic_keyed_service_->enabling().RegisterAllowedChanged(
          base::BindRepeating(&GlicButtonController::OnPrefsChanged,
                              base::Unretained(this))));

  glic_keyed_service_->window_controller().AddGlobalStateObserver(this);
}

GlicButtonController::~GlicButtonController() {
  glic_keyed_service_->window_controller().RemoveGlobalStateObserver(this);
}

void GlicButtonController::PanelStateChanged(
    const mojom::PanelState& panel_state,
    const GlicWindowController::PanelStateContext& context) {
  if (GlicWindowController::AlwaysDetached()) {
    UpdateShowState(true);
  } else {
    const bool detached = panel_state.kind == mojom::PanelStateKind::kDetached;
    glic_controller_delegate_->SetGlicDetached(detached);
    UpdateShowState(detached);
  }
}

void GlicButtonController::OnPrefsChanged() {
  UpdateShowState(
      glic_keyed_service_->window_controller().GetGlobalPanelState().kind ==
      mojom::PanelStateKind::kDetached);
}

void GlicButtonController::UpdateShowState(bool detached) {
  // If the glic window is detached, we want to show the re-attach icon
  // regardless of glic enabling/pinned state.
  if (detached && !GlicWindowController::AlwaysDetached()) {
    glic_controller_delegate_->SetGlicShowState(true);
    return;
  }

  const bool is_enabled_for_profile =
      GlicEnabling::IsEnabledForProfile(profile_);
  const bool is_pinned_to_tabstrip =
      profile_->GetPrefs()->GetBoolean(prefs::kGlicPinnedToTabstrip);

  if (is_enabled_for_profile && is_pinned_to_tabstrip) {
    glic_keyed_service_->TryPreload();
    glic_controller_delegate_->SetGlicShowState(true);
  } else {
    glic_controller_delegate_->SetGlicShowState(false);
  }
}

}  // namespace glic
