// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/ranking/removed_results_ranker.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service.h"
#include "chrome/browser/ash/file_suggest/file_suggest_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace app_list {
namespace {

// Returns true if `result` is a file suggestion.
bool IsFileSuggestion(const ChromeSearchResult& result) {
  ResultType type = result.result_type();
  return type == ResultType::kZeroStateDrive ||
         type == ResultType::kZeroStateFile;
}

}  // namespace

RemovedResultsRanker::RemovedResultsRanker(Profile* profile)
    : profile_(profile),
      proto_(GetFileSuggestKeyedService()->GetProto(
          base::PassKey<RemovedResultsRanker>())) {
  DCHECK(profile_);
  DCHECK(proto_);

  on_init_subscription_ = proto_->RegisterOnInit(
      base::BindOnce(&RemovedResultsRanker::OnRemovedResultsProtoInit,
                     weak_ptr_factory_.GetWeakPtr()));
}

RemovedResultsRanker::~RemovedResultsRanker() = default;

void RemovedResultsRanker::UpdateResultRanks(ResultsMap& results,
                                             ProviderType provider) {
  const auto it = results.find(provider);
  DCHECK(it != results.end());

  // If `proto_` is not initialized, filter all results except for recent apps.
  // Otherwise, filter any results whose IDs have been recorded as for removal.
  const bool proto_initialized = initialized();
  for (const auto& result : it->second) {
    if (!proto_initialized &&
        result->display_type() != DisplayType::kRecentApps) {
      result->scoring().set_filtered(true);
    }
    if (proto_initialized && (*proto_)->removed_ids().contains(result->id())) {
      result->scoring().set_filtered(true);
    }
  }
}

void RemovedResultsRanker::Remove(ChromeSearchResult* result) {
  if (!initialized()) {
    return;
  }

  if (IsFileSuggestion(*result)) {
    // If `result` is a file suggestion, remove it through the suggestion
    // service.
    auto meta_data_copy = result->CloneMetadata();
    GetFileSuggestKeyedService()->RemoveSuggestionBySearchResultAndNotify(
        *meta_data_copy);

  } else {
    // Record the string ID of |result| to the storage proto's map.
    // Note: We are using a map for its set capabilities; the map value is
    // arbitrary.
    ((*proto_)->mutable_removed_ids())->insert({result->id(), false});
    proto_->StartWrite();
  }
}

void RemovedResultsRanker::OnRemovedResultsProtoInit() {
  // Record `proto_` size in KB.
  base::UmaHistogramMemoryKB("Apps.AppList.RemovedResultsProto.SizeInKB",
                             (*proto_)->ByteSizeLong() / 1000);
}

ash::FileSuggestKeyedService*
RemovedResultsRanker::GetFileSuggestKeyedService() {
  return ash::FileSuggestKeyedServiceFactory::GetInstance()->GetService(
      profile_);
}

}  // namespace app_list
