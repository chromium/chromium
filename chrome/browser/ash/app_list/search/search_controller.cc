// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/search_controller.h"

#include <algorithm>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/shell.h"
#include "ash/system/federated/federated_service_controller_impl.h"
#include "base/logging.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/search/app_search_data_source.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/common/keyword_util.h"
#include "chrome/browser/ash/app_list/search/common/string_util.h"
#include "chrome/browser/ash/app_list/search/ranking/ranker_manager.h"
#include "chrome/browser/ash/app_list/search/ranking/sorting.h"
#include "chrome/browser/ash/app_list/search/search_features.h"
#include "chrome/browser/ash/app_list/search/search_metrics_manager.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chrome/browser/ash/app_list/search/search_session_metrics_manager.h"
#include "chrome/browser/metrics/structured/event_logging_features.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace app_list {
namespace {

void ClearNonZeroStateResults(ResultsMap& results) {
  for (auto it = results.begin(); it != results.end();) {
    if (!ash::IsZeroStateResultType(it->first)) {
      it = results.erase(it);
    } else {
      ++it;
    }
  }
}

bool IsTabletMode() {
  return ash::TabletMode::IsInTabletMode();
}

}  // namespace

SearchController::SearchController(
    AppListModelUpdater* model_updater,
    AppListControllerDelegate* list_controller,
    ash::AppListNotifier* notifier,
    Profile* profile,
    ash::federated::FederatedServiceController* federated_service_controller)
    : profile_(profile),
      model_updater_(model_updater),
      list_controller_(list_controller),
      notifier_(notifier),
      federated_service_controller_(federated_service_controller) {}

SearchController::~SearchController() = default;

void SearchController::Initialize() {
  burn_in_controller_ = std::make_unique<BurnInController>(base::BindRepeating(
      &SearchController::OnBurnInPeriodElapsed, base::Unretained(this)));
  ranker_manager_ = std::make_unique<RankerManager>(profile_, this);
  metrics_manager_ =
      std::make_unique<SearchMetricsManager>(profile_, notifier_);
  session_metrics_manager_ =
      std::make_unique<SearchSessionMetricsManager>(profile_, notifier_);
  federated_metrics_manager_ =
      std::make_unique<federated::FederatedMetricsManager>(
          notifier_, federated_service_controller_);
  app_search_data_source_ = std::make_unique<AppSearchDataSource>(
      profile_, list_controller_, base::DefaultClock::GetInstance());
  app_discovery_metrics_manager_ =
      std::make_unique<AppDiscoveryMetricsManager>(profile_);
}

void SearchController::OnBurnInPeriodElapsed() {
  ranker_manager_->OnBurnInPeriodElapsed();
  if (search_features::isLauncherOmniboxPublishLogicLogEnabled()) {
    LOG(ERROR) << "Launcher search burn-in period elapsed publish";
  }
  Publish();
}

void SearchController::AddProvider(std::unique_ptr<SearchProvider> provider) {
  if (ash::IsZeroStateResultType(provider->ResultType())) {
    ++total_zero_state_blockers_;
  }
  provider->set_controller(this);
  providers_.emplace_back(std::move(provider));
}

void SearchController::StartSearch(const std::u16string& query) {
  DCHECK(!query.empty());

  burn_in_controller_->Start();

  // TODO(b/266468933): This logging is limited to a short maximum query
  // length. Add another metric which measures the bucket count of query length,
  // with no maximum.
  ash::RecordLauncherIssuedSearchQueryLength(query.length());
  // Limit query length, for efficiency reasons in matching query to texts.
  const std::u16string truncated_query =
      query.length() > kMaxAllowedQueryLength
          ? query.substr(0, kMaxAllowedQueryLength)
          : query;

  // Clear all search results but preserve zero-state results.
  ClearNonZeroStateResults(results_);

  // NOTE: Not publishing change to clear results when the search query changes
  // so the old results stay on screen until the new ones are ready.
  if (last_query_.empty()) {
    Publish();
  }

  categories_ = CreateAllCategories();
  ranker_manager_->Start(truncated_query, results_, categories_);

  session_start_ = base::Time::Now();
  last_query_ = truncated_query;

  // Search all providers.
  for (const auto& provider : providers_) {
    provider->Start(truncated_query);
  }
}

void SearchController::ClearSearch() {
  // Cancel a pending search publish if it exists.
  burn_in_controller_->Stop();

  ClearNonZeroStateResults(results_);
  last_query_.clear();

  for (const auto& provider : providers_) {
    provider->StopQuery();
  }

  if (search_features::isLauncherOmniboxPublishLogicLogEnabled()) {
    LOG(ERROR) << "Launcher search clear search publish";
  }
  Publish();
  ranker_manager_->Start(u"", results_, categories_);
}

void SearchController::StartZeroState(base::OnceClosure on_done,
                                      base::TimeDelta timeout) {
  // Clear all results - zero state search request is made when the app list
  // gets first shown, which would indicate that search is not currently active.
  results_.clear();
  burn_in_controller_->Stop();

  // Categories currently are not used by zero-state, but may be required for
  // sorting in SetResults.
  categories_ = CreateAllCategories();

  ranker_manager_->Start(std::u16string(), results_, categories_);

  last_query_.clear();

  on_zero_state_done_.AddUnsafe(std::move(on_done));
  returned_zero_state_blockers_ = 0;

  for (const auto& provider : providers_) {
    provider->StartZeroState();
  }

  zero_state_timeout_.Start(
      FROM_HERE, timeout,
      base::BindOnce(&SearchController::OnZeroStateTimedOut,
                     base::Unretained(this)));
}

void SearchController::OnZeroStateTimedOut() {
  // `on_zero_state_done_` will be empty if all zero-state blocking providers
  // have returned. If it isn't, publish whatever results have been returned.
  // If `last_query_` is non-empty, this indicates that a search query has been
  // issued since zero state results were requested. Do not publish results in
  // this case to avoid interfering with queried search burn-in period.
  // Zero state callbacks will get run when next batch of results gets
  // published.
  if (last_query_.empty() && !on_zero_state_done_.empty()) {
    if (search_features::isLauncherOmniboxPublishLogicLogEnabled()) {
      LOG(ERROR) << "Launcher search zero state timeout publish";
    }
    Publish();
  }
}

void SearchController::AppListViewChanging(bool is_visible) {
  // In tablet mode, the launcher is always visible so do not log launcher open
  // if the device is in tablet mode.
  if (is_visible && !IsTabletMode() &&
      base::FeatureList::IsEnabled(metrics::structured::kAppDiscoveryLogging)) {
    app_discovery_metrics_manager_->OnLauncherOpen();
  }

  // On close.
  if (!is_visible) {
    for (const auto& provider : providers_) {
      provider->StopZeroState();
    }
  }
}

void SearchController::OpenResult(ChromeSearchResult* result, int event_flags) {
  // This can happen in certain circumstances due to races. See
  // https://crbug.com/534772
  if (!result) {
    return;
  }

  metrics_manager_->OnOpen(result->result_type(), last_query_);
  if (base::FeatureList::IsEnabled(metrics::structured::kAppDiscoveryLogging)) {
    app_discovery_metrics_manager_->OnOpenResult(result, last_query_);
  }

  const bool dismiss_view_on_open = result->dismiss_view_on_open();

  // Open() may cause |result| to be deleted.
  result->Open(event_flags);

  // Launching apps can take some time. It looks nicer to eagerly dismiss the
  // app list if |result| permits it. Do not close app list for home launcher.
  if (dismiss_view_on_open &&
      (!ash::TabletMode::Get() || !ash::TabletMode::Get()->InTabletMode())) {
    list_controller_->DismissView();
  }
}

void SearchController::InvokeResultAction(ChromeSearchResult* result,
                                          ash::SearchResultActionType action) {
  if (!result) {
    return;
  }

  if (action == ash::SearchResultActionType::kRemove) {
    ranker_manager_->Remove(result);
    // We need to update the currently published results to not include the
    // just-removed result. Manually set the result as filtered and re-publish.
    result->scoring().set_filtered(true);
    if (search_features::isLauncherOmniboxPublishLogicLogEnabled()) {
      LOG(ERROR) << "Launcher search remove publish";
    }
    Publish();
  }
}

void SearchController::SetResults(const SearchProvider* provider,
                                  Results results) {
  // Re-post onto the UI sequence if not called from there.
  auto ui_thread = content::GetUIThreadTaskRunner({});
  if (!ui_thread->RunsTasksInCurrentSequence()) {
    ui_thread->PostTask(
        FROM_HERE,
        base::BindOnce(&SearchController::SetResults, base::Unretained(this),
                       provider, std::move(results)));
    return;
  }

  results_[provider->ResultType()] = std::move(results);
  if (ash::IsZeroStateResultType(provider->ResultType())) {
    SetZeroStateResults(provider);
  } else {
    SetSearchResults(provider);
  }
  if (results_changed_callback_for_test_) {
    results_changed_callback_for_test_.Run(provider->ResultType());
  }
}

void SearchController::SetSearchResults(const SearchProvider* provider) {
  Rank(provider->ResultType());

  for (const auto& result : results_[provider->ResultType()]) {
    metrics_manager_->OnSearchResultsUpdated(result->scoring());
  }

  bool is_post_burn_in = burn_in_controller_->UpdateResults(
      results_, categories_, provider->ResultType());
  // If the burn-in period has not yet elapsed, don't call Publish here (this
  // case is covered by a call scheduled within the burn-in controller).
  if (!last_query_.empty() && is_post_burn_in) {
    if (search_features::isLauncherOmniboxPublishLogicLogEnabled()) {
      LOG(ERROR) << "Launcher search post burn-in publish from "
                 << static_cast<int>(provider->ResultType());
    }
    Publish();
  } else if (search_features::isLauncherOmniboxPublishLogicLogEnabled() &&
             !last_query_.empty()) {
    LOG(ERROR) << "Launcher search pre burn-in from "
               << static_cast<int>(provider->ResultType());
  } else if (search_features::isLauncherOmniboxPublishLogicLogEnabled()) {
    // We should expect no log from here.
    LOG(ERROR) << "Launcher search empty query results from "
               << static_cast<int>(provider->ResultType());
  }
}

void SearchController::SetZeroStateResults(const SearchProvider* provider) {
  Rank(provider->ResultType());

  if (ash::IsZeroStateResultType(provider->ResultType())) {
    ++returned_zero_state_blockers_;
  }

  // Don't publish zero-state results if a queried search is currently in
  // progress.
  if (!last_query_.empty()) {
    return;
  }

  // Wait until all zero state providers have returned before publishing
  // results.
  if (!on_zero_state_done_.empty() &&
      returned_zero_state_blockers_ < total_zero_state_blockers_) {
    return;
  }
  if (search_features::isLauncherOmniboxPublishLogicLogEnabled()) {
    LOG(ERROR) << "Launcher search zero state publish";
  }
  Publish();
}

void SearchController::Rank(ProviderType provider_type) {
  DCHECK(ranker_manager_);
  if (results_.empty()) {
    // Happens if the burn-in period has elapsed without any results having been
    // received from providers. Return early.
    return;
  }

  if (disable_ranking_for_test_) {
    return;
  }

  // Update ranking of all results and categories for this provider. This
  // ordering is important, as result scores may affect category scores.
  ranker_manager_->UpdateResultRanks(results_, provider_type);
  ranker_manager_->UpdateCategoryRanks(results_, categories_, provider_type);
}

void SearchController::Publish() {
  SortCategories(categories_);

  // Create a vector of category enums in display order.
  std::vector<Category> category_enums;
  for (const auto& category : categories_) {
    category_enums.push_back(category.category);
  }

  // Compile a single list of results and sort first by their category with best
  // match first, then by burn-in iteration number, and finally by relevance.
  std::vector<ChromeSearchResult*> all_results;
  for (const auto& type_results : results_) {
    for (const auto& result : type_results.second) {
      double score = result->scoring().FinalScore();

      // Filter out results with negative relevance, which is the rankers'
      // signal that a result should not be displayed at all.
      if (score < 0.0) {
        continue;
      }

      // The display score is the result's final score before display. It is
      // used for sorting below, and may be used directly in ash.
      result->SetDisplayScore(score);
      all_results.push_back(result.get());
    }
  }

  SortResults(all_results, categories_);

  if (!observer_list_.empty()) {
    std::vector<const ChromeSearchResult*> observer_results;
    for (auto* result : all_results) {
      observer_results.push_back(const_cast<const ChromeSearchResult*>(result));
    }

    std::vector<KeywordInfo> extracted_keyword_info =
        ExtractKeywords(last_query_);

    for (Observer& observer : observer_list_) {
      observer.OnResultsAdded(last_query_, extracted_keyword_info,
                              observer_results);
    }
  }

  model_updater_->PublishSearchResults(all_results, category_enums);

  if (!on_zero_state_done_.empty() &&
      (!zero_state_timeout_.IsRunning() ||
       returned_zero_state_blockers_ >= total_zero_state_blockers_)) {
    on_zero_state_done_.Notify();
  }
}

void SearchController::Train(LaunchData&& launch_data) {
  // For non-zero state results (i.e. non continue section results), record the
  // last search query.
  const std::string query = ash::IsZeroStateResultType(launch_data.result_type)
                                ? ""
                                : base::UTF16ToUTF8(last_query_);
  launch_data.query = query;

  if (app_list_features::IsAppListLaunchRecordingEnabled()) {
    metrics_manager_->OnTrain(launch_data, query);
  }

  profile_->GetPrefs()->SetBoolean(ash::prefs::kLauncherResultEverLaunched,
                                   true);

  // Train all search result ranking models.
  ranker_manager_->Train(launch_data);
}

AppSearchDataSource* SearchController::GetAppSearchDataSource() {
  return app_search_data_source_.get();
}

ChromeSearchResult* SearchController::FindSearchResult(
    const std::string& result_id) {
  for (const auto& provider_results : results_) {
    for (const auto& result : provider_results.second) {
      if (result->id() == result_id) {
        return result.get();
      }
    }
  }
  return nullptr;
}

void SearchController::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void SearchController::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

std::u16string SearchController::get_query() {
  return last_query_;
}

base::Time SearchController::session_start() {
  return session_start_;
}

size_t SearchController::ReplaceProvidersForResultTypeForTest(
    ash::AppListSearchResultType result_type,
    std::unique_ptr<SearchProvider> new_provider) {
  DCHECK_EQ(result_type, new_provider->ResultType());

  size_t removed_providers = base::EraseIf(
      providers_, [&](const std::unique_ptr<SearchProvider>& provider) {
        return provider->ResultType() == result_type;
      });
  if (!removed_providers) {
    return 0u;
  }
  DCHECK_EQ(1u, removed_providers);

  if (ash::IsZeroStateResultType(result_type)) {
    total_zero_state_blockers_ -= removed_providers;
  }

  AddProvider(std::move(new_provider));
  return removed_providers;
}

ChromeSearchResult* SearchController::GetResultByTitleForTest(
    const std::string& title) {
  std::u16string target_title = base::ASCIIToUTF16(title);
  for (const auto& provider_results : results_) {
    for (const auto& result : provider_results.second) {
      if (result->title() == target_title &&
          result->result_type() ==
              ash::AppListSearchResultType::kInstalledApp &&
          !result->is_recommendation()) {
        return result.get();
      }
    }
  }
  return nullptr;
}

void SearchController::WaitForZeroStateCompletionForTest(
    base::OnceClosure callback) {
  if (on_zero_state_done_.empty()) {
    std::move(callback).Run();
    return;
  }
  on_zero_state_done_.AddUnsafe(std::move(callback));
}

void SearchController::set_results_changed_callback_for_test(
    ResultsChangedCallback callback) {
  results_changed_callback_for_test_ = std::move(callback);
}

void SearchController::disable_ranking_for_test() {
  disable_ranking_for_test_ = true;
}

}  // namespace app_list
