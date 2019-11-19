// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_PROACTIVE_SUGGESTIONS_CONTROLLER_H_
#define ASH_ASSISTANT_ASSISTANT_PROACTIVE_SUGGESTIONS_CONTROLLER_H_

#include <memory>
#include <set>

#include "ash/assistant/assistant_controller_observer.h"
#include "ash/assistant/model/assistant_suggestions_model_observer.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/public/cpp/assistant/proactive_suggestions_client.h"
#include "base/macros.h"
#include "base/timer/timer.h"

namespace ash {

class AssistantController;
class ProactiveSuggestions;
class ProactiveSuggestionsView;

namespace assistant {
namespace metrics {
enum class ProactiveSuggestionsShowAttempt;
enum class ProactiveSuggestionsShowResult;
}  // namespace metrics
}  // namespace assistant

// Controller for the Assistant proactive suggestions feature.
class AssistantProactiveSuggestionsController
    : public AssistantControllerObserver,
      public ProactiveSuggestionsClient::Delegate,
      public AssistantSuggestionsModelObserver,
      public AssistantViewDelegateObserver {
 public:
  using ProactiveSuggestionsShowAttempt =
      assistant::metrics::ProactiveSuggestionsShowAttempt;
  using ProactiveSuggestionsShowResult =
      assistant::metrics::ProactiveSuggestionsShowResult;

  explicit AssistantProactiveSuggestionsController(
      AssistantController* assistant_controller);
  ~AssistantProactiveSuggestionsController() override;

  // AssistantControllerObserver:
  void OnAssistantControllerConstructed() override;
  void OnAssistantControllerDestroying() override;
  void OnAssistantReady() override;

  // ProactiveSuggestionsClient::Delegate:
  void OnProactiveSuggestionsClientDestroying() override;
  void OnProactiveSuggestionsChanged(
      scoped_refptr<const ProactiveSuggestions> proactive_suggestions) override;
  void OnSourceVerticalScrollDirectionChanged(
      viz::VerticalScrollDirection scroll_direction) override;

  // AssistantSuggestionsModelObserver:
  void OnProactiveSuggestionsChanged(
      scoped_refptr<const ProactiveSuggestions> proactive_suggestions,
      scoped_refptr<const ProactiveSuggestions> old_proactive_suggestions)
      override;

  // AssistantViewDelegateObserver:
  void OnProactiveSuggestionsCloseButtonPressed() override;
  void OnProactiveSuggestionsViewHoverChanged(bool is_hovering) override;
  void OnProactiveSuggestionsViewPressed() override;

 private:
  void MaybeShowUi();
  void CloseUi(ProactiveSuggestionsShowResult result);
  void HideUi();

  AssistantController* const assistant_controller_;  // Owned by Shell.

  ProactiveSuggestionsView* view_ = nullptr;  // Owned by view hierarchy.

  // When shown, the proactive suggestions view will automatically be closed if
  // the user doesn't interact with it within a fixed time interval.
  base::RetainingOneShotTimer auto_close_timer_;

  // If the hash for a set of proactive suggestions is found in this collection,
  // they should not be shown to the user. A set of proactive suggestions may be
  // added to the blacklist as a result of duplicate suppression or as a result
  // of the user explicitly closing the proactive suggestions view.
  std::set<size_t> proactive_suggestions_blacklist_;

  base::WeakPtrFactory<AssistantProactiveSuggestionsController> weak_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(AssistantProactiveSuggestionsController);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_PROACTIVE_SUGGESTIONS_CONTROLLER_H_
