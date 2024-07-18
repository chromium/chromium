// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/perks_discovery_screen.h"

#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/perks_discovery_screen_handler.h"
#include "chromeos/ash/components/growth/campaigns_manager.h"
#include "chromeos/ash/components/growth/campaigns_model.h"

namespace ash {
namespace {

// constexpr const char kUserActionNext[] = "next";

}  // namespace

// static
std::string PerksDiscoveryScreen::GetResultString(Result result) {
  switch (result) {
    case Result::kNext:
      return "Next";
    case Result::kError:
      return "Error";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
}

PerksDiscoveryScreen::PerksDiscoveryScreen(
    base::WeakPtr<PerksDiscoveryScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(PerksDiscoveryScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

PerksDiscoveryScreen::~PerksDiscoveryScreen() = default;

bool PerksDiscoveryScreen::MaybeSkip(WizardContext& context) {
  if (context.skip_post_login_screens_for_tests) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  return false;
}

void PerksDiscoveryScreen::GetOobePerksPayload() {
  auto* campaigns_manager = growth::CampaignsManager::Get();
  auto* campaign =
      campaigns_manager->GetCampaignBySlot(growth::Slot::kOobePerkDiscovery);
  if (!campaign) {
    LOG(ERROR) << "Campaign object is null. Failed to retrieve campaign for "
                  "slot kOobePerkDiscovery.";
    exit_callback_.Run(Result::kError);
    return;
  }
  auto* payload = GetPayloadBySlot(campaign, growth::Slot::kOobePerkDiscovery);
  if (!payload) {
    LOG(ERROR) << "Payload object is null. Failed to retrieve payload for "
                  "campaign and slot kOobePerkDiscovery.";
    exit_callback_.Run(Result::kError);
    return;
  }
}

void PerksDiscoveryScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  view_->Show();

  // TODO(b:353863015) Optimize OOBE Load Growth Campaign Latency
  auto* campaigns_manager = growth::CampaignsManager::Get();
  if (!campaigns_manager) {
    LOG(ERROR) << "CampaignsManager object is null. Failed to retrieve "
                  "CampaignsManager instance.";
    exit_callback_.Run(Result::kError);
    return;
  }
  campaigns_manager->LoadCampaigns(base::BindOnce(
      &PerksDiscoveryScreen::GetOobePerksPayload, weak_factory_.GetWeakPtr()));
}

void PerksDiscoveryScreen::HideImpl() {}

void PerksDiscoveryScreen::OnUserAction(const base::Value::List& args) {
  NOTIMPLEMENTED();
}

}  // namespace ash
