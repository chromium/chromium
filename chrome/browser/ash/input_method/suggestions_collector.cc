// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/suggestions_collector.h"

#include "base/functional/callback.h"
#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"
#include "chromeos/ash/services/ime/public/mojom/input_engine.mojom.h"

namespace ash {
namespace input_method {
namespace {

using ime::AssistiveSuggestion;
using ime::AssistiveSuggestionMode;

std::vector<AssistiveSuggestion> CombineResults(
    const std::vector<AssistiveSuggestion>& first,
    const std::vector<AssistiveSuggestion>& second) {
  std::vector<AssistiveSuggestion> combined;
  combined.reserve(first.size() + second.size());
  combined.insert(combined.end(), first.begin(), first.end());
  combined.insert(combined.end(), second.begin(), second.end());
  return combined;
}

}  // namespace

SuggestionsCollector::SuggestionsCollector(
    SuggestionsSource* assistive_suggester,
    std::unique_ptr<AsyncSuggestionsSource> suggestions_service_client)
    : assistive_suggester_(assistive_suggester),
      suggestions_service_client_(std::move(suggestions_service_client)) {}

SuggestionsCollector::~SuggestionsCollector() = default;

void SuggestionsCollector::GatherSuggestions(
    ime::mojom::SuggestionsRequestPtr request,
    GatherSuggestionsCallback callback) {
  std::vector<AssistiveSuggestion> assistive_suggestions =
      assistive_suggester_->GetSuggestions();

  if (!suggestions_service_client_->IsAvailable()) {
    auto response = ime::mojom::SuggestionsResponse::New(
        /*candidates=*/assistive_suggestions);
    std::move(callback).Run(std::move(response));
    return;
  }

  suggestions_service_client_->RequestSuggestions(
      request->text, request->mode, request->completion_candidates,
      base::BindOnce(&SuggestionsCollector::OnSuggestionsGathered,
                     base::Unretained(this), std::move(callback),
                     assistive_suggestions));
}

void SuggestionsCollector::OnSuggestionsGathered(
    GatherSuggestionsCallback callback,
    const std::vector<AssistiveSuggestion>& assistive_suggestions,
    const std::vector<AssistiveSuggestion>& system_suggestions) {
  auto response = ime::mojom::SuggestionsResponse::New(
      /*candidates=*/CombineResults(assistive_suggestions, system_suggestions));
  std::move(callback).Run(std::move(response));
}

}  // namespace input_method
}  // namespace ash
