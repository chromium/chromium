// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/perks_discovery_screen.h"

#include "ash/constants/ash_features.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/perks_discovery_screen_handler.h"
#include "chromeos/ash/components/growth/campaigns_manager.h"
#include "chromeos/ash/components/growth/campaigns_model.h"

namespace ash {
namespace {

constexpr const char kUserActionFinish[] = "finished";
constexpr const char kUserActionLoaded[] = "loaded";

// Campaign ids from
// google3/chromeos/growth/campaigns/stop_gap_campaigns/stop_gap_campaigns_enums.ts
constexpr const int kPerkGamgeeId100 = 49;
constexpr const int kPerkGamgeeIdExperiment = 33;
constexpr const int kPerkGamgeeIdControl = 38;
constexpr const int kPerkStandardGamgeeId100 = 48;
constexpr const int kPerkStandardGamgeeIdExperiment = 32;
constexpr const int kPerkStandardGamgeeIdControl = 37;

// Current max amount of perks we should get from the server.
constexpr const int kMaxPerksFetched = 10;

void RecordUmaSelectedPerksCount(int selected_perks_count) {
  base::UmaHistogramCustomCounts("OOBE.PerksDiscoveryScreen.SelectedPerksCount",
                                 selected_perks_count,
                                 /*min=*/0,
                                 /*exclusive_max=*/kMaxPerksFetched + 1,
                                 /*buckets=*/kMaxPerksFetched + 1);
}

void RecordUmaSelectedPerksPercentage(int selected_perks_percentage) {
  base::UmaHistogramPercentage(
      "OOBE.PerksDiscoveryScreen.SelectedPerksPercentage",
      selected_perks_percentage);
}

void RecordUmaSelectedPerk(const std::string& perk_id, bool selected) {
  base::UmaHistogramBoolean("OOBE.PerksDiscoveryScreen.Selected." + perk_id,
                            selected);
}

void RecordPerksUmaHistograms(
    const std::vector<SinglePerkDiscoveryPayload>& perks,
    const base::Value::List& selected_perks) {
  size_t selected_perks_count = selected_perks.size();
  size_t total_perks_count = perks.size();

  RecordUmaSelectedPerksCount(selected_perks_count);
  RecordUmaSelectedPerksPercentage(
      (total_perks_count > 0) ? (100 * selected_perks_count / total_perks_count)
                              : 0);

  for (const auto& perk : perks) {
    auto it = std::find_if(selected_perks.cbegin(), selected_perks.cend(),
                           [&](const base::Value& selected_perk) {
                             return selected_perk.GetString() == perk.id;
                           });
    RecordUmaSelectedPerk(perk.id, it != selected_perks.cend());
  }
}

void RecordPerksErrorReasonUmaHistogram(
    PerksDiscoveryScreen::PerksDiscoveryErrorReason error_reason) {
  base::UmaHistogramEnumeration("OOBE.PerksDiscoveryScreen.ErrorReason",
                                error_reason);
}

bool CheckPayloadFormat(const growth::Payload* payload) {
  const base::Value::List* perks = payload->FindList("perks");
  if (!perks) {
    return false;
  }
  for (const auto& perk : *perks) {
    if (!perk.GetDict().FindString("id") ||
        !perk.GetDict().FindString("title") ||
        !perk.GetDict().FindString("text") ||
        !perk.GetDict().FindString("icon") ||
        !perk.GetDict().FindStringByDottedPath("content.illustration.url") ||
        !perk.GetDict().FindStringByDottedPath("content.illustration.height") ||
        !perk.GetDict().FindStringByDottedPath("content.illustration.width") ||
        !perk.GetDict().FindStringByDottedPath("primaryButton.label") ||
        !perk.GetDict().FindStringByDottedPath("secondaryButton.label") ||
        !perk.GetDict().FindDictByDottedPath("primaryButton.action")) {
      return false;
    }
  }
  return true;
}

std::vector<SinglePerkDiscoveryPayload> ParsePayload(
    const growth::Payload* payload) {
  std::vector<SinglePerkDiscoveryPayload> perks_result;
  if (payload->empty()) {
    LOG(WARNING) << "Payload empty.";
    return perks_result;
  }

  if (!CheckPayloadFormat(payload)) {
    // TODO(b/347181006) add a metric to track this error
    LOG(ERROR) << "Payload malformed.";
    return perks_result;
  }

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
  if (perk_data.FindString("additionalText")) {
    additional_text = *perk_data.FindString("additionalText");
  }
}

SinglePerkDiscoveryPayload::~SinglePerkDiscoveryPayload() = default;

SinglePerkDiscoveryPayload::SinglePerkDiscoveryPayload(
    const SinglePerkDiscoveryPayload& perk_data)
    : id(perk_data.id),
      title(perk_data.title),
      subtitle(perk_data.subtitle),
      additional_text(perk_data.additional_text),
      icon_url(perk_data.icon_url),
      content(perk_data.content),
      primary_button(perk_data.primary_button.Clone()),
      secondary_button(perk_data.secondary_button.Clone()) {}

Content::Content() = default;

Content::Content(const Content& content) {
  illustration = content.illustration;
}

Content::~Content() = default;

// static
std::string PerksDiscoveryScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::kNext:
      return "Next";
    case Result::kError:
      return "Error";
    case Result::kTimeout:
      return "Timeout";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
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

  if (!features::IsOobePerksDiscoveryEnabled()) {
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
    RecordPerksErrorReasonUmaHistogram(PerksDiscoveryErrorReason::kNoCampaign);
    exit_callback_.Run(Result::kError);
    return;
  }

  if (!growth::GetCampaignId(campaign).has_value()) {
    LOG(ERROR) << "Invalid: Missing campaign id.";
    // campaign_id is mandatory to forward the user action to the
    // campaign_manager.
    RecordPerksErrorReasonUmaHistogram(
        PerksDiscoveryErrorReason::kNoCampaignID);
    exit_callback_.Run(Result::kError);
    return;
  }

  campaign_id_ = growth::GetCampaignId(campaign).value();
  group_id_ = growth::GetCampaignGroupId(campaign);
  if (campaign_id_ == kPerkGamgeeId100 ||
      campaign_id_ == kPerkGamgeeIdControl ||
      campaign_id_ == kPerkGamgeeIdExperiment ||
      campaign_id_ == kPerkStandardGamgeeId100 ||
      campaign_id_ == kPerkStandardGamgeeIdExperiment ||
      campaign_id_ == kPerkStandardGamgeeIdControl) {
    // Mark the gamgee perk screen as shown for this user.
    ProfileManager::GetActiveUserProfile()->GetPrefs()->SetBoolean(
        prefs::kOobePerksDiscoveryGamgeeShown, true);
  }

  auto* payload = GetPayloadBySlot(campaign, growth::Slot::kOobePerkDiscovery);
  if (!payload) {
    LOG(ERROR) << "Payload object is null. Failed to retrieve payload for "
                  "campaign and slot kOobePerkDiscovery.";
    RecordPerksErrorReasonUmaHistogram(PerksDiscoveryErrorReason::kNoPayload);
    exit_callback_.Run(Result::kError);
    return;
  }

  perks_data_ = ParsePayload(payload);

  if (view_ && !perks_data_.empty()) {
    view_->SetPerksData(perks_data_);
    return;
  }

  LOG(WARNING) << "Payload parsing error. Unable to extract required information.";
  RecordPerksErrorReasonUmaHistogram(
      PerksDiscoveryErrorReason::kMalformedPayload);
  exit_callback_.Run(Result::kError);
}

void PerksDiscoveryScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  view_->Show();

  timeout_overview_timer_.Start(FROM_HERE, delay_exit_timeout_, this,
                                &PerksDiscoveryScreen::ExitScreenTimeout);

  auto* campaigns_manager = growth::CampaignsManager::Get();
  if (!campaigns_manager) {
    LOG(ERROR) << "CampaignsManager object is null. Failed to retrieve "
                  "CampaignsManager instance.";
    RecordPerksErrorReasonUmaHistogram(
        PerksDiscoveryErrorReason::kNoCampaignManager);
    exit_callback_.Run(Result::kError);
    return;
  }

  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile) {
    LOG(ERROR)
        << "Profile object is null. Failed to retrieve profile instance.";
    RecordPerksErrorReasonUmaHistogram(
        PerksDiscoveryErrorReason::kNoUserProfile);
    exit_callback_.Run(Result::kError);
    return;
  }
  campaigns_manager->SetPrefs(profile->GetPrefs());

  campaigns_manager->LoadCampaigns(
      base::BindOnce(&PerksDiscoveryScreen::GetOobePerksPayloadAndShow,
                     weak_factory_.GetWeakPtr()) , true);
}

void PerksDiscoveryScreen::HideImpl() {}

void PerksDiscoveryScreen::ExitScreenTimeout() {
  if (is_hidden()) {
    return;
  }
  exit_callback_.Run(Result::kTimeout);
}

void PerksDiscoveryScreen::ShowOverviewStep() {
  if (view_) {
    view_->SetOverviewStep();
  }
}

void PerksDiscoveryScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();

  if (action_id == kUserActionLoaded) {
    if (!timeout_overview_timer_.IsRunning()) {
      LOG(WARNING) << "Perks discovery screen finished loading after timeout";
      return;
    }
    timeout_overview_timer_.Stop();
    // Add a slight delay before showing the overview to avoid a jarring
    // transition if the loading step finishes too quickly.
    delay_overview_timer_.Start(FROM_HERE, delay_overview_step_, this,
                                &PerksDiscoveryScreen::ShowOverviewStep);
    return;
  }

  if (action_id == kUserActionFinish) {
    CHECK_EQ(args.size(), 2u);
    OnPerksSelectionFinished(args[1].GetList());
    exit_callback_.Run(Result::kNext);
    return;
  }

  BaseScreen::OnUserAction(args);
}

void PerksDiscoveryScreen::OnPerksSelectionFinished(
    const base::Value::List& selected_perks) {
  for (const auto& perk_selected : selected_perks) {
    auto perk = std::find_if(perks_data_.cbegin(), perks_data_.cend(),
                             [&](const SinglePerkDiscoveryPayload& perk_data) {
                               return perk_data.id == perk_selected.GetString();
                             });

    CHECK(perk != perks_data_.end())
        << "Failed to find perk " << perk_selected.GetString();
    PerformButtonAction(perk->primary_button);
  }

  RecordPerksUmaHistograms(perks_data_, selected_perks);
}

void PerksDiscoveryScreen::PerformButtonAction(
    const base::Value::Dict& button_data) {
  if (!button_data.FindDict("action")) {
    RecordPerksErrorReasonUmaHistogram(
        PerksDiscoveryErrorReason::kNoActionFound);
    return;
  }
  const growth::Action action = growth::Action(button_data.FindDict("action"));
  auto* campaigns_manager = growth::CampaignsManager::Get();
  if (!campaigns_manager) {
    RecordPerksErrorReasonUmaHistogram(
        PerksDiscoveryErrorReason::kNoCampaignManager);
    return;
  }
  campaigns_manager->PerformAction(campaign_id_, group_id_, &action);
}

}  // namespace ash
