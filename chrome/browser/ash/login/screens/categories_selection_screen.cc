// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/categories_selection_screen.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/oobe_apps_service/oobe_apps_discovery_service.h"
#include "chrome/browser/ash/login/oobe_apps_service/oobe_apps_discovery_service_factory.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/categories_selection_screen_handler.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace ash {
namespace {

constexpr const char kUserActionNext[] = "next";
constexpr const char kUserActionSkip[] = "skip";
constexpr const char kUserActionLoaded[] = "loaded";

// Current max amount of use-cases we should get from the server.
constexpr const int kMaxUseCasesCount = 20;

bool HasBeenSelected(std::string category_id) {
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  const base::Value::List& selected_categories =
      prefs->GetList(prefs::kOobeCategoriesSelected);
  for (const auto& category : selected_categories) {
    if (category.GetString() == category_id) {
      return true;
    }
  }
  return false;
}

void RecordUmaLoadingTime(base::TimeDelta delta) {
  // The maximum is set to 60 seconds due to the fact that the screen is skipped
  // if the loading time exceeds 60 seconds.
  base::UmaHistogramCustomTimes("OOBE.CategoriesSelectionScreen.LoadingTime",
                                delta, base::Milliseconds(1), base::Minutes(1),
                                50);
}

void RecordUmaSelectedUseCasesCount(int selected_use_cases_count) {
  base::UmaHistogramCustomCounts(
      "OOBE.CategoriesSelectionScreen.SelectedUseCasesCount",
      selected_use_cases_count,
      /*min=*/0,
      /*exclusive_max=*/kMaxUseCasesCount + 1,
      /*buckets=*/kMaxUseCasesCount + 1);
}

void RecordUmaSelectedUseCasesPercentage(int selected_use_cases_percentage) {
  base::UmaHistogramPercentage(
      "OOBE.CategoriesSelectionScreen.SelectedUseCasesPercentage",
      selected_use_cases_percentage);
}

void RecordUmaUseCaseID(int use_case_order_id) {
  // Order is a zero-based index, so we can use ExactLinear histogram for now.
  base::UmaHistogramExactLinear(
      "OOBE.CategoriesSelectionScreen.SelectedUseCaseIDs", use_case_order_id,
      kMaxUseCasesCount + 1);
}

}  // namespace

// static
std::string CategoriesSelectionScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::kNext:
      return "Next";
    case Result::kSkip:
      return "Skip";
    case Result::kError:
      return "Error";
    case Result::kDataMalformed:
      return "DataMalformed";
    case Result::kTimeout:
      return "Timeout";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

CategoriesSelectionScreen::CategoriesSelectionScreen(
    base::WeakPtr<CategoriesSelectionScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(CategoriesSelectionScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

CategoriesSelectionScreen::~CategoriesSelectionScreen() = default;

bool CategoriesSelectionScreen::MaybeSkip(WizardContext& context) {
  if (context.skip_post_login_screens_for_tests) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  if (!features::IsOobePersonalizedOnboardingEnabled()) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  Profile* profile = ProfileManager::GetActiveUserProfile();

  bool is_managed_account = profile->GetProfilePolicyConnector()->IsManaged();
  bool is_child_account = user_manager->IsLoggedInAsChildUser();
  if (is_managed_account || is_child_account) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  return false;
}

void CategoriesSelectionScreen::ShowImpl() {
  if (!view_) {
    return;
  }
  view_->Show();

  timeout_overview_timer_ = std::make_unique<base::OneShotTimer>();
  timeout_overview_timer_->Start(FROM_HERE, delay_exit_timeout_, this,
                                 &CategoriesSelectionScreen::ExitScreenTimeout);

  loading_start_time_ = base::TimeTicks::Now();

  raw_ptr<OobeAppsDiscoveryService> oobe_apps_discovery_service_ =
      OobeAppsDiscoveryServiceFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile());
  oobe_apps_discovery_service_->GetAppsAndUseCases(
      base::BindOnce(&CategoriesSelectionScreen::OnResponseReceived,
                     weak_factory_.GetWeakPtr()));
}

void CategoriesSelectionScreen::HideImpl() {}

void CategoriesSelectionScreen::OnResponseReceived(
    const std::vector<OOBEAppDefinition>& appInfos,
    const std::vector<OOBEDeviceUseCase>& categories,
    AppsFetchingResult result) {
  if (result != AppsFetchingResult::kSuccess) {
    LOG(ERROR)
        << "Got an error when fetched cached data from the OOBE Apps Service";
    exit_callback_.Run(Result::kError);
    return;
  }

  if (categories.empty()) {
    LOG(ERROR) << "Empty set of use-cases received from the server";
    exit_callback_.Run(Result::kDataMalformed);
    return;
  }

  use_case_id_to_order_.clear();

  base::Value::List categories_list;
  for (OOBEDeviceUseCase category : categories) {
    // Disregard the category with order 0, as it relates to a general,
    // non-specific use case (oobe_otheres).
    if (category.GetOrder() == 0) {
      continue;
    }

    use_case_id_to_order_[category.GetID()] = category.GetOrder();

    base::Value::Dict category_dict;
    category_dict.Set("categoryId", base::Value(std::move(category.GetID())));
    category_dict.Set("title", base::Value(std::move(category.GetLabel())));
    category_dict.Set("subtitle",
                      base::Value(std::move(category.GetDescription())));
    category_dict.Set("icon", base::Value(std::move(category.GetImageURL())));
    category_dict.Set("selected", HasBeenSelected(category.GetID()));
    categories_list.Append(std::move(category_dict));
  }

  if (categories_list.empty()) {
    LOG(ERROR) << "Empty set of use-cases received after filtering data";
    exit_callback_.Run(Result::kDataMalformed);
    return;
  }

  use_cases_total_count_ = categories_list.size();

  base::Value::Dict data;
  data.Set("categories", std::move(categories_list));
  if (view_) {
    view_->SetCategoriesData(std::move(data));
  }
}

void CategoriesSelectionScreen::ExitScreenTimeout() {
  exit_callback_.Run(Result::kTimeout);
}

void CategoriesSelectionScreen::OnSelect(
    base::Value::List selected_use_cases_ids) {
  int selected_use_cases_count =
      static_cast<int>(selected_use_cases_ids.size());
  RecordUmaSelectedUseCasesCount(selected_use_cases_count);

  RecordUmaSelectedUseCasesPercentage(
      (use_cases_total_count_ > 0)
          ? (100 * selected_use_cases_count / use_cases_total_count_)
          : 0);

  for (const auto& selected_use_case_id : selected_use_cases_ids) {
    auto it = use_case_id_to_order_.find(selected_use_case_id.GetString());
    if (it != use_case_id_to_order_.end()) {
      RecordUmaUseCaseID(it->second);
    }
  }

  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  prefs->SetList(prefs::kOobeCategoriesSelected,
                 std::move(selected_use_cases_ids));
}

void CategoriesSelectionScreen::ShowOverviewStep() {
  RecordUmaLoadingTime(base::TimeTicks::Now() - loading_start_time_);
  if (view_) {
    view_->SetOverviewStep();
  }
}

void CategoriesSelectionScreen::OnUserAction(const base::Value::List& args) {
  if (is_hidden()) {
    return;
  }
  const std::string& action_id = args[0].GetString();

  if (action_id == kUserActionLoaded) {
    timeout_overview_timer_.reset();
    delay_overview_timer_ = std::make_unique<base::OneShotTimer>();
    delay_overview_timer_->Start(FROM_HERE, delay_overview_step_, this,
                                 &CategoriesSelectionScreen::ShowOverviewStep);
    return;
  }

  if (action_id == kUserActionSkip) {
    PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
    prefs->ClearPref(prefs::kOobeCategoriesSelected);
    exit_callback_.Run(Result::kSkip);
    return;
  }

  if (action_id == kUserActionNext) {
    CHECK_EQ(args.size(), 2u);
    OnSelect(args[1].GetList().Clone());
    exit_callback_.Run(Result::kNext);
    return;
  }

  BaseScreen::OnUserAction(args);
}

}  // namespace ash
