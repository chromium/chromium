// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SELECTION_EXPLAIN_SUGGESTION_H_
#define CHROME_BROWSER_GLIC_SELECTION_EXPLAIN_SUGGESTION_H_

#include <memory>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/selection/explain_fulfillment.mojom.h"
#include "chrome/browser/selection/suggestion.h"
#include "components/optimization_guide/proto/features/quick_answers.pb.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace optimization_guide {
class ModelQualityLogEntry;
struct OptimizationGuideModelExecutionResult;
}  // namespace optimization_guide

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace glic {

// Shows the "Explain" card in the selection overlay, and serves the card's
// requests.
class ExplainSuggestion : public ::selection::Suggestion,
                          public selection::ExplainFulfillment {
 public:
  ExplainSuggestion(tabs::TabInterface& tab,
                    optimization_guide::proto::QuickAnswersRequest request);
  ExplainSuggestion(const ExplainSuggestion&) = delete;
  ExplainSuggestion& operator=(const ExplainSuggestion&) = delete;
  ~ExplainSuggestion() override;

  // ::selection::Suggestion:
  ToolId GetToolId() const override;
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
  void OnModelExecutionResponse(
      optimization_guide::OptimizationGuideModelExecutionResult result,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry);

  const raw_ref<tabs::TabInterface> tab_;
  const optimization_guide::proto::QuickAnswersRequest request_;
  const std::u16string label_ = u"Explain";
  GetExplanationCallback pending_callback_;
  mojo::AssociatedReceiver<selection::ExplainFulfillment> receiver_{this};
  base::WeakPtrFactory<ExplainSuggestion> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SELECTION_EXPLAIN_SUGGESTION_H_
