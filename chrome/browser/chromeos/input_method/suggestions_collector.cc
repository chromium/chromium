// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/suggestions_collector.h"

namespace chromeos {

SuggestionsCollector::SuggestionsCollector(
    SuggestionsSource* assistive_suggester)
    : assistive_suggester_(assistive_suggester) {}

void SuggestionsCollector::GatherSuggestions(
    const SuggestionContext& suggestion_context,
    GatherSuggestionsCallback callback) {
  // TODO(crbug/1146266): Fetch suggestions from suggestions service as well.
  std::move(callback).Run(assistive_suggester_->GetSuggestions());
}

}  // namespace chromeos
