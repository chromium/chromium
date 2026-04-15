// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_button_controller.h"

#include "chrome/browser/glic/browser_ui/glic_button_controller_delegate.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/pdf/common/constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
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

  tab_strip_glic_controller_delegate_->SetButtonController(this);
  toolbar_glic_controller_delegate_->SetButtonController(this);

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
      glic_keyed_service_->instance_coordinator().AddGlobalShowHideCallback(
          update_callback));
  subscriptions_.push_back(
      glic_keyed_service_->fre_controller().AddWebUiStateChangedCallback(
          base::BindRepeating(&GlicButtonController::OnFreStateChanged,
                              base::Unretained(this))));
}

GlicButtonController::~GlicButtonController() {
  if (tab_strip_glic_controller_delegate_) {
    tab_strip_glic_controller_delegate_->SetButtonController(nullptr);
  }
  if (toolbar_glic_controller_delegate_) {
    toolbar_glic_controller_delegate_->SetButtonController(nullptr);
  }
}

void GlicButtonController::UpdateButton() {
  // Attempt to record startup metrics when the button controller is first
  // created, no-op if startup metrics have already been measured.
  // Note that this will not record metrics for profiles that are not eligible
  // for Glic (i.e. GlicEnabling::IsProfileEligible() is false), as they will
  // never have a GlicButtonController created. Recording metrics for those
  // cases is handled by GlicProfileManager instead.
  glic_keyed_service_->enabling().MaybeRecordStartupMetrics();

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

  bool is_glic_panel_open =
      glic_keyed_service_->IsPanelShowingForBrowser(*browser_);
  tab_strip_glic_controller_delegate_->SetGlicPanelIsOpen(is_glic_panel_open);
  toolbar_glic_controller_delegate_->SetGlicPanelIsOpen(is_glic_panel_open);
}

void GlicButtonController::OnFreStateChanged(mojom::FreWebUiState) {
  UpdateButton();
}

bool GlicButtonController::ShouldAutoSummarize() const {
  if (!base::FeatureList::IsEnabled(features::kGlicButtonAutoSummarize) ||
      !browser_->GetActiveTabInterface()) {
    return false;
  }

  content::WebContents* web_contents =
      browser_->GetActiveTabInterface()->GetContents();
  if (!web_contents) {
    return false;
  }

  return web_contents->GetContentsMimeType() == pdf::kPDFMimeType;
}

mojom::InvocationSource GlicButtonController::GetInvocationSource(
    bool is_showing_nudge) const {
  if (is_showing_nudge) {
    return mojom::InvocationSource::kNudge;
  } else if (ShouldAutoSummarize()) {
    return mojom::InvocationSource::kZeroStateAutoSummarize;
  }

  return mojom::InvocationSource::kTopChromeButton;
}

}  // namespace glic
