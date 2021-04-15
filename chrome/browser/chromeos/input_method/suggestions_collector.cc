// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/suggestions_collector.h"

namespace chromeos {

void SuggestionsCollector::GatherSuggestions(
    const SuggestionContext& suggestion_context,
    GatherSuggestionsCallback callback) {
  // TODO(crbug/1146266): Implement this by collecting any suggestions in the
  // assistive suggesters currently, and requesting text suggestions to be
  // generated from the text suggestion service.
  std::move(callback).Run({});
}

}  // namespace chromeos
