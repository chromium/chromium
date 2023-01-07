// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_SUGGESTIONS_SERVICE_CLIENT_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_SUGGESTIONS_SERVICE_CLIENT_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/input_method/suggestions_source.h"
#include "chromeos/ash/services/ime/public/cpp/assistive_suggestions.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/text_suggester.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {
namespace input_method {

// A client interface to the TextSuggestions service found in the ML service.
class SuggestionsServiceClient : public AsyncSuggestionsSource {
 public:
  SuggestionsServiceClient();
  ~SuggestionsServiceClient() override;

  // AsyncSuggestionsSource overrides
  void RequestSuggestions(
      const std::string& preceding_text,
      const ime::AssistiveSuggestionMode& suggestion_mode,
      const std::vector<ime::DecoderCompletionCandidate>& completion_candidates,
      RequestSuggestionsCallback callback) override;
  bool IsAvailable() override;

 private:
  // Called once the text suggester model has been loaded and is (or is not)
  // available for use.
  void OnTextSuggesterLoaded(
      chromeos::machine_learning::mojom::LoadModelResult result);

  // Called when results are returned from the suggestions service
  void OnSuggestionsReturned(
      base::TimeTicks time_request_was_made,
      RequestSuggestionsCallback callback,
      ime::AssistiveSuggestionMode suggestion_mode_requested,
      chromeos::machine_learning::mojom::TextSuggesterResultPtr result);

  mojo::Remote<chromeos::machine_learning::mojom::TextSuggester>
      text_suggester_;
  bool text_suggester_loaded_ = false;

  base::WeakPtrFactory<SuggestionsServiceClient> weak_ptr_factory_{this};
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_SUGGESTIONS_SERVICE_CLIENT_H_
