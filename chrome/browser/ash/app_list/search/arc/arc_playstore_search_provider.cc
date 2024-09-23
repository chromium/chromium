// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/arc/arc_playstore_search_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/components/arc/app/arc_playstore_search_request_state.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ash/app_list/search/arc/arc_playstore_search_result.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "chrome/browser/ash/extensions/gfx_utils.h"
#include "chrome/browser/profiles/profile.h"

namespace {
constexpr int kHistogramBuckets = 13;
constexpr char kAppListPlayStoreQueryStateHistogram[] =
    "Apps.AppListPlayStoreQueryState";

// Skips Play Store apps that have equivalent extensions installed.
// Do not skip recent instant apps since they should be treated like
// on-device apps.
bool CanSkipSearchResult(content::BrowserContext* context,
                         const arc::mojom::AppDiscoveryResult& result) {
  if (result.is_instant_app && result.is_recent)
    return false;

  if (!result.package_name.has_value())
    return false;

  if (!extensions::util::GetEquivalentInstalledExtensions(
           context, result.package_name.value())
           .empty()) {
    return true;
  }

  // TODO(crbug.com/41343677): Remove this once we have a fix in Phonesky.
  // Don't show installed Android apps.
  const ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(context);
  return arc_prefs && arc_prefs->GetPackage(result.package_name.value());
}

// Checks if we're receiving a result with invalid data from Android.
bool IsInvalidResult(const arc::mojom::AppDiscoveryResult& result) {
  // The result doesn't have a valid launch intent or install intent.
  if ((!result.launch_intent_uri || result.launch_intent_uri->empty()) &&
      (!result.install_intent_uri || result.install_intent_uri->empty())) {
    return true;
  }

  // The result doesn't have a valid label.
  if (!result.label || result.label->empty())
    return true;

  // The result doesn't have a valid launcher icon.
  if (!result.icon)
    return true;

  // The result doesn't have a valid package name.
  if (!result.package_name || result.package_name->empty())
    return true;

  return false;
}

}  // namespace

namespace app_list {

ArcPlayStoreSearchProvider::ArcPlayStoreSearchProvider(
    int max_results,
    Profile* profile,
    AppListControllerDelegate* list_controller)
    : SearchProvider(SearchCategory::kPlayStore),
      max_results_(max_results),
      profile_(profile),
      list_controller_(list_controller) {
  DCHECK_EQ(kHistogramBuckets, max_results + 1);
}

ArcPlayStoreSearchProvider::~ArcPlayStoreSearchProvider() = default;

ash::AppListSearchResultType ArcPlayStoreSearchProvider::ResultType() const {
  return ash::AppListSearchResultType::kPlayStoreApp;
}

void ArcPlayStoreSearchProvider::Start(const std::u16string& query) {
  last_query_ = query;

  arc::mojom::AppInstance* app_instance =
      arc::ArcServiceManager::Get()
          ? ARC_GET_INSTANCE_FOR_METHOD(
                arc::ArcServiceManager::Get()->arc_bridge_service()->app(),
                GetRecentAndSuggestedAppsFromPlayStore)
          : nullptr;

  DCHECK(!query.empty());
  if (app_instance == nullptr)
    return;

  app_instance->GetRecentAndSuggestedAppsFromPlayStore(
      base::UTF16ToUTF8(query), max_results_,
      base::BindOnce(&ArcPlayStoreSearchProvider::OnResults,
                     weak_ptr_factory_.GetWeakPtr(), query,
                     base::TimeTicks::Now()));
}

void ArcPlayStoreSearchProvider::StopQuery() {
  weak_ptr_factory_.InvalidateWeakPtrs();
  last_query_.clear();
}

void ArcPlayStoreSearchProvider::OnResults(
    const std::u16string& query,
    base::TimeTicks query_start_time,
    arc::ArcPlayStoreSearchRequestState state,
    std::vector<arc::mojom::AppDiscoveryResultPtr> results) {
  if (state != arc::ArcPlayStoreSearchRequestState::SUCCESS) {
    DCHECK(
        state ==
            arc::ArcPlayStoreSearchRequestState::PHONESKY_RESULT_INVALID_DATA ||
        results.empty());
    UMA_HISTOGRAM_ENUMERATION(kAppListPlayStoreQueryStateHistogram, state,
                              arc::ArcPlayStoreSearchRequestState::STATE_COUNT);

    // PHONESKY_RESULT_INVALID_DATA indicates that at least one of the apps
    // returned from playstore was invalid. The returned data may still contain
    // valid results - display them in the UI if that's the case.
    if (state !=
            arc::ArcPlayStoreSearchRequestState::PHONESKY_RESULT_INVALID_DATA ||
        results.empty()) {
      return;
    }
  }

  // Play store could have a long latency that when the results come back,
  // user has entered a different query. Do not return the staled results
  // from the previous query in such case.
  if (query != last_query_) {
    UMA_HISTOGRAM_ENUMERATION(
        kAppListPlayStoreQueryStateHistogram,
        arc::ArcPlayStoreSearchRequestState::FAILED_TO_CALL_CANCEL,
        arc::ArcPlayStoreSearchRequestState::STATE_COUNT);
    return;
  }

  SearchProvider::Results new_results;
  size_t instant_app_count = 0;
  bool has_invalid_result = false;
  for (auto& result : results) {
    if (IsInvalidResult(*result)) {
      has_invalid_result = true;
      continue;
    }

    if (result->is_instant_app)
      ++instant_app_count;

    if (CanSkipSearchResult(profile_, *result))
      continue;

    new_results.emplace_back(std::make_unique<ArcPlayStoreSearchResult>(
        std::move(result), list_controller_, last_query_));
  }
  SwapResults(&new_results);

  // Record user metrics.
  if (state == arc::ArcPlayStoreSearchRequestState::SUCCESS) {
    if (has_invalid_result) {
      UMA_HISTOGRAM_ENUMERATION(
          kAppListPlayStoreQueryStateHistogram,
          arc::ArcPlayStoreSearchRequestState::CHROME_GOT_INVALID_RESULT,
          arc::ArcPlayStoreSearchRequestState::STATE_COUNT);
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          kAppListPlayStoreQueryStateHistogram,
          arc::ArcPlayStoreSearchRequestState::SUCCESS,
          arc::ArcPlayStoreSearchRequestState::STATE_COUNT);
    }
  }

  UMA_HISTOGRAM_TIMES("Arc.PlayStoreSearch.QueryTime",
                      base::TimeTicks::Now() - query_start_time);
  UMA_HISTOGRAM_EXACT_LINEAR("Arc.PlayStoreSearch.ReturnedAppsTotalV2",
                             results.size(), kHistogramBuckets);
  UMA_HISTOGRAM_EXACT_LINEAR("Arc.PlayStoreSearch.ReturnedUninstalledAppsV2",
                             results.size() - instant_app_count,
                             kHistogramBuckets);
  UMA_HISTOGRAM_EXACT_LINEAR("Arc.PlayStoreSearch.ReturnedInstantAppsV2",
                             instant_app_count, kHistogramBuckets);
}

}  // namespace app_list
