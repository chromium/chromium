// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/perks_discovery_screen.h"

#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/perks_discovery_screen_handler.h"
#include "chromeos/ash/components/growth/campaigns_manager.h"
#include "chromeos/ash/components/growth/campaigns_model.h"

namespace ash {
namespace {

std::vector<SinglePerkDiscoveryPayload> ParsePayload(
    const growth::Payload* payload) {
  std::vector<SinglePerkDiscoveryPayload> perks_result;
  if (payload->empty()) {
    LOG(WARNING) << "Payload empty.";
    return perks_result;
  }

  // TODO(b:353480634) Add sanity check for the payload data format.
  const base::Value::List* perks = payload->FindList("perks");
  for (const auto& perk : *perks) {
    perks_result.push_back(SinglePerkDiscoveryPayload(perk.GetDict()));
  }
  return perks_result;
}

}  // namespace

SinglePerkDiscoveryPayload::SinglePerkDiscoveryPayload(
    const base::Value::Dict& perk_data)
    : id(*perk_data.FindString("id")),
      title(*perk_data.FindString("title")),
      subtitle(*perk_data.FindString("text")),
      icon_url(*perk_data.FindString("icon")),
      primary_button(perk_data.FindDict("primaryButton")->Clone()),
      secondary_button(perk_data.FindDict("secondaryButton")->Clone()) {
  auto* oobe_content = perk_data.FindDict("content");
  if (oobe_content->FindDict("illustration")) {
    Illustration perk_illustration;
    perk_illustration.url =
        *oobe_content->FindStringByDottedPath("illustration.url");
    perk_illustration.width =
        *oobe_content->FindStringByDottedPath("illustration.width");
    perk_illustration.height =
        *oobe_content->FindStringByDottedPath("illustration.height");
    content.illustration = perk_illustration;
  }
}

SinglePerkDiscoveryPayload::~SinglePerkDiscoveryPayload() = default;

Content::Content() = default;

Content::~Content() = default;

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

  Profile* profile = ProfileManager::GetActiveUserProfile();
  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  bool is_managed_account = profile->GetProfilePolicyConnector()->IsManaged();
  bool is_child_account = user_manager->IsLoggedInAsChildUser();
  if (is_managed_account || is_child_account) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  return false;
}

void PerksDiscoveryScreen::GetOobePerksPayloadAndShow() {
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

  perks_data_ = ParsePayload(payload);

  if (view_ && !perks_data_.empty()) {
    view_->SetPerksData(perks_data_);
    return;
  }

  LOG(WARNING) << "Payload parsing error. Unable to extract required information.";
  exit_callback_.Run(Result::kError);
}

void PerksDiscoveryScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  view_->Show();

  auto* campaigns_manager = growth::CampaignsManager::Get();
  if (!campaigns_manager) {
    LOG(ERROR) << "CampaignsManager object is null. Failed to retrieve "
                  "CampaignsManager instance.";
    exit_callback_.Run(Result::kError);
    return;
  }
  campaigns_manager->LoadCampaigns(
      base::BindOnce(&PerksDiscoveryScreen::GetOobePerksPayloadAndShow,
                     weak_factory_.GetWeakPtr()) , true);
}

void PerksDiscoveryScreen::HideImpl() {}

void PerksDiscoveryScreen::OnUserAction(const base::Value::List& args) {
  NOTIMPLEMENTED();
}

}  // namespace ash
