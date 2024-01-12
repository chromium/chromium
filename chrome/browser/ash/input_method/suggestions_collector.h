// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_SUGGESTIONS_COLLECTOR_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_SUGGESTIONS_COLLECTOR_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/input_method/suggestions_source.h"
#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"
#include "chromeos/ash/services/ime/public/mojom/input_method_host.mojom.h"

namespace ash {
namespace input_method {

// Used to collect any text suggestions from the system
class SuggestionsCollector {
 public:
  // `assistive_suggester` must exist for the lifetime of this instance.
  SuggestionsCollector(
      SuggestionsSource* assistive_suggester,
      std::unique_ptr<AsyncSuggestionsSource> suggestions_service_client);

  ~SuggestionsCollector();

  using GatherSuggestionsCallback =
      base::OnceCallback<void(ime::mojom::SuggestionsResponsePtr)>;

  // Collects all suggestions from the system.
  void GatherSuggestions(ime::mojom::SuggestionsRequestPtr request,
                         GatherSuggestionsCallback callback);

 private:
  // Called when suggestions have been returned from the injected
  // SuggestionsRequestor.
  void OnSuggestionsGathered(
      GatherSuggestionsCallback callback,
      const std::vector<ime::AssistiveSuggestion>& assistive_suggestions,
      const std::vector<ime::AssistiveSuggestion>& system_suggestions);

  // Not owned by this class
  raw_ptr<SuggestionsSource> assistive_suggester_;

  // Client used to request suggestions from the system
  std::unique_ptr<AsyncSuggestionsSource> suggestions_service_client_;
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_SUGGESTIONS_COLLECTOR_H_
