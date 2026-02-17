// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_button_controller.h"

#include "chrome/browser/glic/browser_ui/glic_button_controller_delegate.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
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
    GlicButtonControllerDelegate* tab_strip_delegate,
    GlicButtonControllerDelegate* toolbar_delegate,
    GlicKeyedService* service)
    : profile_(profile),
      browser_(browser),
      tab_strip_glic_controller_delegate_(tab_strip_delegate),
      toolbar_glic_controller_delegate_(toolbar_delegate),
      glic_keyed_service_(service) {
  CHECK(tab_strip_glic_controller_delegate_);
  CHECK(toolbar_glic_controller_delegate_);
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
  subscriptions_.push_back(
      glic_keyed_service_->fre_controller().AddWebUiStateChangedCallback(
          base::BindRepeating(&GlicButtonController::OnFreStateChanged,
                              base::Unretained(this))));
}

GlicButtonController::~GlicButtonController() = default;

void GlicButtonController::UpdateButton() {
  const bool is_enabled_for_profile =
      GlicEnabling::IsEnabledForProfile(profile_);
  const bool is_pinned_to_tabstrip =
      profile_->GetPrefs()->GetBoolean(prefs::kGlicPinnedToTabstrip);
  if (!is_enabled_for_profile || !is_pinned_to_tabstrip) {
    // If the button shouldn't be shown, just hide it.
    tab_strip_glic_controller_delegate_->SetGlicShowState(false);
    toolbar_glic_controller_delegate_->SetGlicShowState(false);
    return;
  }

  tab_strip_glic_controller_delegate_->SetGlicShowState(true);
  toolbar_glic_controller_delegate_->SetGlicShowState(true);

  // Try preloading since we know the button is visible.
  glic_keyed_service_->TryPreload();

  bool is_glic_panel_open =
      glic_keyed_service_->IsFreShowing() ||
      glic_keyed_service_->IsPanelShowingForBrowser(*browser_);
  tab_strip_glic_controller_delegate_->SetGlicPanelIsOpen(is_glic_panel_open);
  toolbar_glic_controller_delegate_->SetGlicPanelIsOpen(is_glic_panel_open);
}

void GlicButtonController::OnFreStateChanged(mojom::FreWebUiState) {
  UpdateButton();
}

}  // namespace glic
