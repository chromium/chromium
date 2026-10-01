// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_EXPLAIN_SUGGESTION_H_
#define CHROME_BROWSER_GLIC_SELECTION_EXPLAIN_SUGGESTION_H_

#include <string>

#include "base/memory/raw_ref.h"
#include "chrome/browser/glic/selection/explain_fulfillment.mojom.h"
#include "chrome/browser/selection/suggestion.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace tabs {
class TabInterface;
}

namespace glic {

// Shows the "Explain" card in the selection overlay, and serves the card's
// requests.
class ExplainSuggestion : public ::selection::Suggestion,
                          public selection::ExplainFulfillment {
 public:
  // The text the card shows.
  // TODO(liuwilliam): Wire up the response from
  // `OptimizationGuideModelExecutionResult`.
  static constexpr char kPlaceholderText[] =
      "An explanation of your selection will appear here.";

  explicit ExplainSuggestion(tabs::TabInterface& tab);
  ExplainSuggestion(const ExplainSuggestion&) = delete;
  ExplainSuggestion& operator=(const ExplainSuggestion&) = delete;
  ~ExplainSuggestion() override;

  // ::selection::Suggestion:
  const std::u16string& GetLabel() const override;
  void OnSuggestionPresented() override;
  void OnSuggestionExecuted() override;
  ::selection::mojom::ActionPtr GetAction() const override;

  // selection::ExplainFulfillment:
  void GetExplanation(GetExplanationCallback callback) override;
  void OpenTabForSearch(const std::string& query) override;
  void AskGemini() override;

 private:
  void Bind(
      mojo::PendingAssociatedReceiver<selection::ExplainFulfillment> receiver);

  const raw_ref<tabs::TabInterface> tab_;
  const std::u16string label_ = u"Explain";
  mojo::AssociatedReceiver<selection::ExplainFulfillment> receiver_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SELECTION_EXPLAIN_SUGGESTION_H_
