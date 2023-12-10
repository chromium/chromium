// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/app_shortcuts_search_provider.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/search/app_shortcuts_search_result.h"
#include "chrome/browser/ash/app_list/search/files/file_result.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/string_matching/fuzzy_tokenized_string_match.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"

namespace app_list {
namespace {

constexpr size_t kMinQueryLength = 3u;
constexpr double kRelevanceThreshold = 0.79;

constexpr char kAppShortcutSearchPrefix[] = "appshortcutsearch://";

using FuzzyTokenizedStringMatch =
    ::ash::string_matching::FuzzyTokenizedStringMatch;
using TokenizedString = ::ash::string_matching::TokenizedString;

}  // namespace

AppShortcutsSearchProvider::AppShortcutsSearchProvider(Profile* profile)
    : SearchProvider(ControlCategory::kAppShortcuts), profile_(profile) {
  DCHECK(profile_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "AppShortcutsSearchProvider";
}

AppShortcutsSearchProvider::~AppShortcutsSearchProvider() = default;

ash::AppListSearchResultType AppShortcutsSearchProvider::ResultType() const {
  return ash::AppListSearchResultType::kAppShortcutV2;
}

void AppShortcutsSearchProvider::Start(const std::u16string& query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "Start " << query;
  if (query.size() < kMinQueryLength) {
    return;
  }

  query_start_time_ = base::TimeTicks::Now();
  last_query_ = query;

  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile_);

  OnSearchComplete(proxy->ShortcutRegistryCache()->GetAllShortcuts());
}

void AppShortcutsSearchProvider::StopQuery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  last_query_.clear();
}

void AppShortcutsSearchProvider::OnSearchComplete(
    const std::vector<apps::ShortcutView>& app_shortcuts) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "OnSearchComplete";

  TokenizedString tokenized_query(last_query_, TokenizedString::Mode::kWords);
  SearchProvider::Results results;
  for (const auto& app_shortcut : app_shortcuts) {
    if (!app_shortcut->name.has_value()) {
      continue;
    }

    double relevance = 0;
    auto terms = TokenizedString(base::UTF8ToUTF16(app_shortcut->name.value()),
                                 TokenizedString::Mode::kWords);
    if (tokenized_query.tokens().size() == 1) {
      for (const auto& term : terms.tokens()) {
        relevance = std::max(
            relevance, FuzzyTokenizedStringMatch::TokenSetRatio(
                           tokenized_query,
                           TokenizedString(term, TokenizedString::Mode::kWords),
                           /*partial=*/false));
      }
    } else {
      relevance = FuzzyTokenizedStringMatch::TokenSetRatio(tokenized_query,
                                                           std::move(terms),
                                                           /*partial=*/false);
    }
    if (relevance < kRelevanceThreshold) {
      continue;
    }

    results.push_back(MakeResult(app_shortcut, relevance));
  }

  SwapResults(&results);
  UMA_HISTOGRAM_TIMES("Apps.AppList.AppShortcutsSearchProvider.Latency",
                      base::TimeTicks::Now() - query_start_time_);
}

std::unique_ptr<AppShortcutSearchResult> AppShortcutsSearchProvider::MakeResult(
    const apps::ShortcutView& app_shortcut,
    double relevance) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << "id: " << kAppShortcutSearchPrefix + app_shortcut->name.value()
           << " name: " << app_shortcut->name.value()
           << " query: " << last_query_ << " rl: " << relevance;

  auto result = std::make_unique<AppShortcutSearchResult>(
      /*id*/ kAppShortcutSearchPrefix + app_shortcut->name.value(),
      base::UTF8ToUTF16(app_shortcut->name.value()), profile_, relevance);
  return result;
}

}  // namespace app_list
