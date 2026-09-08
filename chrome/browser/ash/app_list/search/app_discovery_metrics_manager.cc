// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/app_discovery_metrics_manager.h"

#include <utility>

#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "chrome/browser/ash/app_list/search/common/types_util.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/string_matching/fuzzy_tokenized_string_match.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "chromeos/ash/components/sync/sync_service_provider.h"
#include "components/account_id/account_id.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "components/sync/base/data_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_utils.h"

namespace app_list {

namespace {

namespace cros_events = metrics::structured::events::v2::cr_os_events;

bool IsResultAppInstallRelated(ash::SearchResultType result_type) {
  return result_type == ash::SearchResultType::PLAY_STORE_UNINSTALLED_APP ||
         result_type == ash::SearchResultType::GAME_SEARCH ||
         result_type == ash::SearchResultType::PLAY_STORE_INSTANT_APP;
}

}  // namespace

AppDiscoveryMetricsManager::AppDiscoveryMetricsManager(Profile* profile)
    : profile_(profile) {}
AppDiscoveryMetricsManager::~AppDiscoveryMetricsManager() = default;

void AppDiscoveryMetricsManager::OnOpenResult(ChromeSearchResult* result,
                                              const std::u16string& query) {
  // If the result is not app-install related, then ignore result as it does not
  // pertain to app-discovery.
  if (!IsResultAppInstallRelated(result->metrics_type())) {
    return;
  }

  auto app_name = ash::string_matching::TokenizedString(result->title());
  auto tokenized_query = ash::string_matching::TokenizedString(query);

  double string_match_score =
      ash::string_matching::FuzzyTokenizedStringMatch::WeightedRatio(
          app_name, tokenized_query);

  // If app-sync is disabled, then do not send app_id.
  std::string app_id = IsAppSyncEnabled() ? result->id() : "";
  std::u16string app_title = IsAppSyncEnabled() ? result->title() : u"";

  metrics::structured::StructuredMetricsClient::Record(
      std::move(cros_events::AppDiscovery_AppLauncherResultOpened()
                    .SetAppId(app_id)
                    .SetAppName(std::string(app_title.begin(), app_title.end()))
                    .SetFuzzyStringMatch(string_match_score)
                    .SetResultCategory(result->metrics_type())));
}

void AppDiscoveryMetricsManager::OnLauncherOpen() {
  metrics::structured::StructuredMetricsClient::Record(
      std::move(cros_events::AppDiscovery_LauncherOpen()));
}

bool AppDiscoveryMetricsManager::IsAppSyncEnabled() {
  // Guest and other non-user profiles carry no usable AccountId -- the
  // annotation is either absent or an empty, invalid AccountId -- and so have
  // no SyncService. GetUploadToGoogleState() treats that the same as the null
  // the Profile-keyed factory used to return for them.
  const AccountId* account_id = ash::AnnotatedAccountId::Get(profile_);
  const syncer::SyncService* sync_service =
      account_id && account_id->is_valid()
          ? ash::SyncServiceProvider::Get().Find(*account_id)
          : nullptr;
  switch (
      syncer::GetUploadToGoogleState(sync_service, syncer::DataType::APPS)) {
    case syncer::UploadState::NOT_ACTIVE:
      return false;
    case syncer::UploadState::INITIALIZING:
      // Note that INITIALIZING is considered good enough, because syncing apps
      // is known to be enabled, and transient errors don't really matter here.
    case syncer::UploadState::ACTIVE:
      return true;
  }
}

}  // namespace app_list
