// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/suggestions_collector.h"

#include "base/callback.h"
#include "chromeos/services/ime/public/cpp/suggestions.h"
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"

namespace chromeos {
namespace {

using ::chromeos::ime::TextSuggestion;
using ::chromeos::ime::TextSuggestionMode;

std::vector<TextSuggestion> CombineResults(
    const std::vector<TextSuggestion>& first,
    const std::vector<TextSuggestion>& second) {
  std::vector<TextSuggestion> combined;
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
  std::vector<ime::TextSuggestion> assistive_suggestions =
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
    const std::vector<ime::TextSuggestion>& assistive_suggestions,
    const std::vector<ime::TextSuggestion>& system_suggestions) {
  auto response = ime::mojom::SuggestionsResponse::New(
      /*candidates=*/CombineResults(assistive_suggestions, system_suggestions));
  std::move(callback).Run(std::move(response));
}

}  // namespace chromeos
