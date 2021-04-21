// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/suggestions_collector.h"

#include "chromeos/services/ime/public/cpp/suggestions.h"
#include "chromeos/services/ime/public/mojom/input_engine.mojom.h"

namespace chromeos {

SuggestionsCollector::SuggestionsCollector(
    SuggestionsSource* assistive_suggester)
    : assistive_suggester_(assistive_suggester) {}

void SuggestionsCollector::GatherSuggestions(
    ime::mojom::SuggestionsRequestPtr request,
    GatherSuggestionsCallback callback) {
  // TODO(crbug/1146266): Fetch suggestions from suggestions service as well.
  auto response = ime::mojom::SuggestionsResponse::New();
  response->candidates = assistive_suggester_->GetSuggestions();
  std::move(callback).Run(std::move(response));
}

}  // namespace chromeos
