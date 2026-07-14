// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_button_controller.h"

#include "chrome/browser/glic/browser_ui/glic_split_button_delegate.h"
#include "chrome/browser/glic/browser_ui/glic_vector_icon_manager.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/prefs/pref_service.h"
#include "ui/views/widget/widget.h"

namespace glic {

DEFINE_USER_DATA(GlicButtonController);

// static
GlicButtonController* GlicButtonController::From(
    BrowserWindowInterface* browser) {
  return browser ? GlicButtonController::Get(browser->GetUnownedUserDataHost())
                 : nullptr;
}

GlicButtonController::GlicButtonController(
    Profile* profile,
    BrowserWindowInterface& browser,
    GlicSplitButtonDelegate* tab_strip_delegate,
    GlicSplitButtonDelegate* toolbar_delegate,
    GlicKeyedService* service)
    : profile_(profile),
      browser_(browser),
      tab_strip_glic_controller_delegate_(tab_strip_delegate),
      toolbar_glic_controller_delegate_(toolbar_delegate),
      glic_keyed_service_(service),
      scoped_unowned_user_data_(browser.GetUnownedUserDataHost(), *this) {
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
      glic_keyed_service_->instance_coordinator().AddGlobalShowHideCallback(
          update_callback));
}

GlicButtonController::~GlicButtonController() = default;

void GlicButtonController::UpdateButton() {
  // Attempt to record startup metrics when the button controller is first
  // created, no-op if startup metrics have already been measured.
  // Note that this will not record metrics for profiles that are not eligible
  // for Glic (i.e. GlicEnabling::IsProfileEligible() is false), as they will
  // never have a GlicButtonController created. Recording metrics for those
  // cases is handled by GlicProfileManager instead.
  glic_keyed_service_->enabling().MaybeRecordStartupMetrics();

  const bool should_show_button = GlicEnabling::ShouldShowGlicButton(profile_);
  const bool is_pinned_to_tabstrip =
      profile_->GetPrefs()->GetBoolean(prefs::kGlicPinnedToTabstrip);
  if (!should_show_button || !is_pinned_to_tabstrip) {
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


mojom::InvocationSource GlicButtonController::GetInvocationSource(
    bool is_showing_nudge,
    bool is_toolbar) const {
  if (is_showing_nudge) {
    return mojom::InvocationSource::kNudge;
  }

  return is_toolbar ? mojom::InvocationSource::kToolbarButton
                    : mojom::InvocationSource::kTopChromeButton;
}

}  // namespace glic
