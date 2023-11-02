// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_SUGGESTIONS_MODEL_OBSERVER_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_SUGGESTIONS_MODEL_OBSERVER_H_

#include <vector>

#include "base/component_export.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_suggestion.h"

namespace ash {

// A checked observer which receives notification of changes to the Assistant
// suggestions model.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantSuggestionsModelObserver
    : public base::CheckedObserver {
 public:
  using AssistantSuggestion = assistant::AssistantSuggestion;

  // Invoked when the cache of conversation starters has changed.
  virtual void OnConversationStartersChanged(
      const std::vector<AssistantSuggestion>& conversation_starters) {}

  // Invoked when the cache of onboarding suggestions has changed.
  virtual void OnOnboardingSuggestionsChanged(
      const std::vector<AssistantSuggestion>& onboarding_suggestions) {}

 protected:
  ~AssistantSuggestionsModelObserver() override = default;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_SUGGESTIONS_MODEL_OBSERVER_H_
