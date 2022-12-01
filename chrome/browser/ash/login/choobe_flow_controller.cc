// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/choobe_flow_controller.h"

#include <vector>

#include "base/containers/flat_set.h"
#include "base/notreached.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/theme_selection_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

const int kMinScreensToShowChoobe = 3;
const int kMaxScreensToShowChoobe = 10;

const ChoobeFlowController::OptionalScreen kOptionalScreens[] = {
    {ThemeSelectionScreenView::kScreenId,
     "oobe-32:stars",
     {"choobeThemeSelectionTileTitle",
      IDS_OOBE_CHOOBE_THEME_SELECTION_TILE_TITLE}}};

}  // namespace

ChoobeFlowController::ChoobeFlowController() {}

ChoobeFlowController::~ChoobeFlowController() {}

void ChoobeFlowController::Start() {
  if (is_choobe_flow_active_)
    return;

  LoginDisplayHost* host = LoginDisplayHost::default_host();
  if (!host || !host->GetWizardController())
    return;

  for (auto screen : kOptionalScreens) {
    BaseScreen* screen_obj =
        host->GetWizardController()->GetScreen(screen.screen_id);
    if (!screen_obj->ShouldBeSkipped(*host->GetWizardContext())) {
      eligible_screens_.push_back(screen);
    }
  }

  if (eligible_screens_.size() >= kMinScreensToShowChoobe &&
      eligible_screens_.size() <= kMaxScreensToShowChoobe) {
    is_choobe_flow_active_ = true;
  }
}

void ChoobeFlowController::Stop(PrefService& prefs) {
  eligible_screens_.clear();
  selected_screens_.clear();
  is_choobe_flow_active_ = false;
  prefs.ClearPref(prefs::kChoobeSelectedScreens);
}

std::vector<ChoobeFlowController::OptionalScreen>
ChoobeFlowController::GetEligibleCHOOBEScreens() {
  return eligible_screens_;
}

bool ChoobeFlowController::ShouldScreenBeSkipped(OobeScreenId screen_id) {
  if (!is_choobe_flow_active_)
    return false;
  return selected_screens_.find(screen_id) == selected_screens_.end();
}

bool ChoobeFlowController::IsOptionalScreen(OobeScreenId screen_id) {
  for (const auto& screen : kOptionalScreens) {
    if (screen.screen_id.AsId() == screen_id) {
      return true;
    }
  }
  return WizardController::default_controller()->HasScreen(screen_id);
}

std::vector<ChoobeFlowController::OptionalScreenResource>
ChoobeFlowController::GetOptionalScreensResources() {
  std::vector<ChoobeFlowController::OptionalScreenResource> titles;
  for (const auto& screen : kOptionalScreens) {
    titles.push_back(screen.title_resource);
  }
  return titles;
}

void ChoobeFlowController::OnScreensSelected(PrefService& prefs,
                                             base::Value::List screens_ids) {
  if (!is_choobe_flow_active_)
    NOTREACHED() << "Screens should only be selected when is_choobe_active_";

  for (const auto& screen_id : screens_ids) {
    const auto id = OobeScreenId(screen_id.GetString());
    CHECK(IsOptionalScreen(id));
    selected_screens_.insert(id);
  }

  prefs.SetList(prefs::kChoobeSelectedScreens, std::move(screens_ids));
}

void ChoobeFlowController::MaybeResumeChoobe(const PrefService& prefs) {
  if (!prefs.HasPrefPath(prefs::kChoobeSelectedScreens))
    return;

  const auto& selected_screens_ids =
      prefs.GetList(prefs::kChoobeSelectedScreens);
  for (const auto& screen_id : selected_screens_ids) {
    const auto id = OobeScreenId(screen_id.GetString());
    if (IsOptionalScreen(id)) {
      selected_screens_.insert(id);
    } else {
      LOG(WARNING) << "The selected screen " << screen_id.GetString()
                   << "was not found during the resuming of CHOOBE.";
    }
  }
}

}  // namespace ash
