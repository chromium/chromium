// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_SUGGESTIONS_COLLECTOR_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_SUGGESTIONS_COLLECTOR_H_

#include <vector>

#include "base/callback.h"
#include "chrome/browser/chromeos/input_method/suggestions.h"

namespace chromeos {

// Used to collect any text suggestions from the system
class SuggestionsCollector {
 public:
  using GatherSuggestionsCallback =
      base::OnceCallback<void(const std::vector<TextSuggestion>&)>;

  // Collects all suggestions from the system.
  void GatherSuggestions(const SuggestionContext& suggestion_context,
                         GatherSuggestionsCallback callback);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_SUGGESTIONS_COLLECTOR_H_
