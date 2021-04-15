// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_SUGGESTIONS_COLLECTOR_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_SUGGESTIONS_COLLECTOR_H_

#include <vector>

#include "base/callback.h"
#include "chrome/browser/chromeos/input_method/suggestions.h"
#include "chrome/browser/chromeos/input_method/suggestions_source.h"

namespace chromeos {

// Used to collect any text suggestions from the system
class SuggestionsCollector {
 public:
  // `assistive_suggester` must exist for the lifetime of this instance.
  explicit SuggestionsCollector(SuggestionsSource* assistive_suggester);

  using GatherSuggestionsCallback =
      base::OnceCallback<void(const std::vector<TextSuggestion>&)>;

  // Collects all suggestions from the system.
  void GatherSuggestions(const SuggestionContext& suggestion_context,
                         GatherSuggestionsCallback callback);

 private:
  // Not owned by this class
  SuggestionsSource* assistive_suggester_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_SUGGESTIONS_COLLECTOR_H_
