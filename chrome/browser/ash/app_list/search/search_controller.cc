// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/search_controller.h"

#include <algorithm>
#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/app_list/app_list_controller.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/window_tree_host_lookup.h"
#include "ash/system/federated/federated_service_controller_impl.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/search/app_search_data_source.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/common/file_util.h"
#include "chrome/browser/ash/app_list/search/common/keyword_util.h"
#include "chrome/browser/ash/app_list/search/common/string_util.h"
#include "chrome/browser/ash/app_list/search/common/types_util.h"
#include "chrome/browser/ash/app_list/search/ranking/ranker_manager.h"
#include "chrome/browser/ash/app_list/search/ranking/sorting.h"
#include "chrome/browser/ash/app_list/search/search_engine.h"
#include "chrome/browser/ash/app_list/search/search_features.h"
#include "chrome/browser/ash/app_list/search/search_file_scanner.h"
#include "chrome/browser/ash/app_list/search/search_metrics_manager.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chrome/browser/ash/app_list/search/search_session_metrics_manager.h"
#include "chrome/browser/ash/app_list/search/sparky_event_rewriter.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/metrics/structured/event_logging_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/screen.h"

namespace app_list {
namespace {

// Constants for sparky panel position.
inline constexpr int kPanelBoundsPadding = 8;
inline constexpr int kPanelDefaultWidth = 360;
inline constexpr int kPanelDefaultHeight = 492;

void OpenSparkyPanel() {
  chromeos::MahiManager* sparky_manager = chromeos::MahiManager::Get();
  if (sparky_manager && sparky_manager->IsEnabled()) {
    auto display = display::Screen::GetScreen()->GetPrimaryDisplay();
    // Opens the panel in the bottom right of the screen. It's the same
    // position before the panel position becomes dynamic.
    sparky_manager->OpenMahiPanel(
        display.id(), gfx::Rect(display.work_area().bottom_right().x() -
                                    kPanelDefaultWidth - kPanelBoundsPadding,
                                display.work_area().bottom_right().y() -
                                    kPanelDefaultHeight - kPanelBoundsPadding,
                                kPanelDefaultWidth, kPanelDefaultHeight));
  }
}

void ClearNonZeroStateResults(ResultsMap& results) {
  for (auto it = results.begin(); it != results.end();) {
    if (!ash::IsZeroStateResultType(it->first)) {
      it = results.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace

SearchController::SearchController(
    AppListModelUpdater* model_updater,
    AppListControllerDelegate* list_controller,
    ash::AppListNotifier* notifier,
    Profile* profile,
    ash::federated::FederatedServiceController* federated_service_controller)
    : profile_(profile),
      sparky_event_rewriter_(std::make_unique<SparkyEventRewriter>()),
      model_updater_(model_updater),
      list_controller_(list_controller),
      notifier_(notifier),
      federated_service_controller_(federated_service_controller) {
  if (chromeos::features::IsSparkyEnabled()) {
    // Get the window tree host for the primary display.
    const auto& display = display::Screen::GetScreen()->GetPrimaryDisplay();
    auto* host = ash::GetWindowTreeHostForDisplay(display.id());
    CHECK(host);
    host->GetEventSource()->AddEventRewriter(sparky_event_rewriter_.get());
  }
}

SearchController::~SearchController() = default;

void SearchController::Initialize() {
  burn_in_controller_ = std::make_unique<BurnInController>(base::BindRepeating(
      &SearchController::OnBurnInPeriodElapsed, base::Unretained(this)));
  ranker_manager_ = std::make_unique<RankerManager>(profile_);
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
  search_engine_ = std::make_unique<SearchEngine>(profile_);

  if (search_features::IsLauncherSearchFileScanEnabled()) {
    search_file_scanner_ = std::make_unique<SearchFileScanner>(
        profile_, file_manager::util::GetMyFilesFolderForProfile(profile_),
        GetTrashPaths(profile_));
  }
}

std::vector<ash::AppListSearchControlCategory>
SearchController::GetToggleableCategories() const {
  return toggleable_categories_;
}

void SearchController::OnBurnInPeriodElapsed() {
  ranker_manager_->OnBurnInPeriodElapsed();
  Publish();
}

void SearchController::AddProvider(std::unique_ptr<SearchProvider> provider) {
  if (ash::IsZeroStateResultType(provider->ResultType())) {
    ++total_zero_state_blockers_;
  }

  // TODO(b/315709613): Temporary. Update the factory.
  const auto control_category =
      MapSearchCategoryToControlCategory(provider->search_category());
  if (control_category != ControlCategory::kCannotToggle &&
      std::find(toggleable_categories_.begin(), toggleable_categories_.end(),
                control_category) == toggleable_categories_.end()) {
    toggleable_categories_.push_back(control_category);
    std::sort(toggleable_categories_.begin(), toggleable_categories_.end());
  }

  search_engine_->AddProvider(std::move(provider));
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
  SearchOptions search_options;

  if (ash::features::IsLauncherSearchControlEnabled()) {
    search_options.search_categories = std::vector<SearchCategory>();
    base::flat_set<ControlCategory> disabled_categories;
    for (const auto category : toggleable_categories_) {
      if (!IsControlCategoryEnabled(profile_, category)) {
        disabled_categories.insert(category);
      }
    }

    for (const auto category : search_engine_->GetAllSearchCategories()) {
      if (!disabled_categories.contains(
              MapSearchCategoryToControlCategory(category))) {
        search_options.search_categories->push_back(category);
      }
    }
  }

  ranker_manager_->Start(truncated_query, categories_);

  session_start_ = base::Time::Now();
  last_query_ = truncated_query;

  search_engine_->StartSearch(truncated_query, std::move(search_options),
                              base::BindRepeating(&SearchController::SetResults,
                                                  base::Unretained(this)));
}

void SearchController::ClearSearch() {
  // Cancel a pending search publish if it exists.
  burn_in_controller_->Stop();

  ClearNonZeroStateResults(results_);
  last_query_.clear();

  search_engine_->StopQuery();

  Publish();
  ranker_manager_->Start(u"", categories_);
}

void SearchController::StartZeroState(base::OnceClosure on_done,
                                      base::TimeDelta timeout) {
  // Opens launcher will open sparky UI instead if flag is enabled and shift
  // key is pressed, and it prevents the launcher panel from opening. This code
  // is used for experiment only and should never go to production.
  if (chromeos::features::IsSparkyEnabled() &&
      sparky_event_rewriter_->is_shift_pressed()) {
    OpenSparkyPanel();
    return;
  }

  // Clear all results - zero state search request is made when the app list
  // gets first shown, which would indicate that search is not currently active.
  results_.clear();
  burn_in_controller_->Stop();

  // Categories currently are not used by zero-state, but may be required for
  // sorting in SetResults.
  categories_ = CreateAllCategories();

  ranker_manager_->Start(std::u16string(), categories_);

  last_query_.clear();

  on_zero_state_done_.AddUnsafe(std::move(on_done));
  returned_zero_state_blockers_ = 0;

  search_engine_->StartZeroState(base::BindRepeating(
      &SearchController::SetResults, base::Unretained(this)));

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
    Publish();
  }
}

void SearchController::AppListViewChanging(bool is_visible) {
  // In tablet mode, the launcher is always visible so do not log launcher open
  // if the device is in tablet mode.
  if (is_visible && !display::Screen::GetScreen()->InTabletMode()) {
    app_discovery_metrics_manager_->OnLauncherOpen();
  }

  // On close.
  if (!is_visible) {
    search_engine_->StopZeroState();
  }
}

void SearchController::OpenResult(ChromeSearchResult* result, int event_flags) {
  // This can happen in certain circumstances due to races. See
  // https://crbug.com/534772
  if (!result) {
    return;
  }

  metrics_manager_->OnOpen(result->result_type(), last_query_);
  app_discovery_metrics_manager_->OnOpenResult(result, last_query_);

  const bool dismiss_view_on_open = result->dismiss_view_on_open();

  // Open() may cause |result| to be deleted.
  result->Open(event_flags);

  // Launching apps can take some time. It looks nicer to eagerly dismiss the
  // app list if |result| permits it. Do not close app list for home launcher.
  if (dismiss_view_on_open && !display::Screen::GetScreen()->InTabletMode()) {
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
    Publish();
  }
}

void SearchController::SetResults(ResultType result_type, Results results) {
  // Re-post onto the UI sequence if not called from there.
  auto ui_thread = content::GetUIThreadTaskRunner({});
  if (!ui_thread->RunsTasksInCurrentSequence()) {
    ui_thread->PostTask(
        FROM_HERE,
        base::BindOnce(&SearchController::SetResults, base::Unretained(this),
                       result_type, std::move(results)));
    return;
  }

  results_[result_type] = std::move(results);
  if (ash::IsZeroStateResultType(result_type)) {
    SetZeroStateResults(result_type);
  } else {
    SetSearchResults(result_type);
  }
  if (results_changed_callback_for_test_) {
    results_changed_callback_for_test_.Run(result_type);
  }
}

void SearchController::SetSearchResults(ResultType result_type) {
  Rank(result_type);

  for (const auto& result : results_[result_type]) {
    metrics_manager_->OnSearchResultsUpdated(result->scoring());
  }

  bool is_post_burn_in =
      burn_in_controller_->UpdateResults(results_, categories_, result_type);
  // If the burn-in period has not yet elapsed, don't call Publish here (this
  // case is covered by a call scheduled within the burn-in controller).
  if (!last_query_.empty() && is_post_burn_in) {
    Publish();
  }
}

void SearchController::SetZeroStateResults(ResultType result_type) {
  Rank(result_type);

  if (ash::IsZeroStateResultType(result_type)) {
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
  std::vector<raw_ptr<ChromeSearchResult, VectorExperimental>> all_results;
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
    for (ChromeSearchResult* result : all_results) {
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

void SearchController::OnDefaultSearchIsGoogleSet(bool is_google) {
  federated_metrics_manager_->OnDefaultSearchIsGoogleSet(is_google);
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
  return search_engine_->ReplaceProvidersForResultTypeForTest(
      result_type, std::move(new_provider));
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
