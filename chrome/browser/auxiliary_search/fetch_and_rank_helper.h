// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUXILIARY_SEARCH_FETCH_AND_RANK_HELPER_H_
#define CHROME_BROWSER_AUXILIARY_SEARCH_FETCH_AND_RANK_HELPER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "components/visited_url_ranking/public/visited_url_ranking_service.h"

namespace visited_url_ranking {
class VisitedURLRankingService;
struct URLVisitsMetadata;
struct URLVisitAggregate;
struct FetchOptions;
struct Config;
}  // namespace visited_url_ranking

// Class to manage history data fetch and rank flow, containing required
// parameters and states, and to create java objects and run the callback.
class FetchAndRankHelper : public base::RefCounted<FetchAndRankHelper> {
 public:
  using FetchResultCallback = base::OnceCallback<void(
      std::vector<jni_zero::ScopedJavaLocalRef<jobject>>,
      const visited_url_ranking::URLVisitsMetadata& metadata)>;

  // The |entries_callback| is called when history data is fetched and ranked.
  // It passes a Java List<AuxiliarySearchDataEntry> and metadata about the
  // entries.
  FetchAndRankHelper(
      visited_url_ranking::VisitedURLRankingService* ranking_service,
      FetchResultCallback entries_callback,
      const std::optional<GURL>& custom_tab_url = std::nullopt,
      std::optional<base::Time> begin_time = std::nullopt);

  // Starts the service to fetch history data.
  void StartFetching();

 private:
  friend class base::RefCounted<FetchAndRankHelper>;

  ~FetchAndRankHelper();

  // Continuing after StartFetching()'s call to FetchURLVisitAggregates().
  void OnFetched(
      visited_url_ranking::ResultStatus status,
      visited_url_ranking::URLVisitsMetadata url_visits_metadata,
      std::vector<visited_url_ranking::URLVisitAggregate> aggregates);

  // Continuing after OnFetched()'s call to RankVisitAggregates().
  void OnRanked(visited_url_ranking::URLVisitsMetadata url_visits_metadata,
                visited_url_ranking::ResultStatus status,
                std::vector<visited_url_ranking::URLVisitAggregate> aggregates);

 private:
  raw_ptr<visited_url_ranking::VisitedURLRankingService> ranking_service_;
  FetchResultCallback entries_callback_;
  std::optional<GURL> custom_tab_url_;
  const visited_url_ranking::FetchOptions fetch_options_;
  const visited_url_ranking::Config config_;
};

#endif  // CHROME_BROWSER_AUXILIARY_SEARCH_FETCH_AND_RANK_HELPER_H_
