// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_suggestions_model.h"

#include "ash/assistant/model/assistant_suggestions_model_observer.h"
#include "ash/public/cpp/assistant/proactive_suggestions.h"

namespace ash {

AssistantSuggestionsModel::AssistantSuggestionsModel() = default;

AssistantSuggestionsModel::~AssistantSuggestionsModel() = default;

void AssistantSuggestionsModel::AddObserver(
    AssistantSuggestionsModelObserver* observer) {
  observers_.AddObserver(observer);
}

void AssistantSuggestionsModel::RemoveObserver(
    AssistantSuggestionsModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AssistantSuggestionsModel::SetConversationStarters(
    std::vector<AssistantSuggestionPtr> conversation_starters) {
  conversation_starters_.clear();
  conversation_starters_.swap(conversation_starters);

  NotifyConversationStartersChanged();
}

const chromeos::assistant::mojom::AssistantSuggestion*
AssistantSuggestionsModel::GetConversationStarterById(int id) const {
  // We consider the index of a conversation starter within our backing vector
  // to be its unique id.
  DCHECK_GE(id, 0);
  DCHECK_LT(id, static_cast<int>(conversation_starters_.size()));
  return conversation_starters_.at(id).get();
}

std::map<int, const chromeos::assistant::mojom::AssistantSuggestion*>
AssistantSuggestionsModel::GetConversationStarters() const {
  std::map<int, const AssistantSuggestion*> conversation_starters;

  // We use index within our backing vector to represent unique id.
  int id = 0;
  for (const AssistantSuggestionPtr& starter : conversation_starters_)
    conversation_starters[id++] = starter.get();

  return conversation_starters;
}

void AssistantSuggestionsModel::SetProactiveSuggestions(
    scoped_refptr<const ProactiveSuggestions> proactive_suggestions) {
  if (ProactiveSuggestions::AreEqual(proactive_suggestions.get(),
                                     proactive_suggestions_.get())) {
    return;
  }

  auto old_proactive_suggestions = std::move(proactive_suggestions_);
  proactive_suggestions_ = std::move(proactive_suggestions);
  NotifyProactiveSuggestionsChanged(old_proactive_suggestions);
}

scoped_refptr<const ProactiveSuggestions>
AssistantSuggestionsModel::GetProactiveSuggestions() const {
  return proactive_suggestions_;
}

void AssistantSuggestionsModel::NotifyConversationStartersChanged() {
  const std::map<int, const AssistantSuggestion*> conversation_starters =
      GetConversationStarters();

  for (AssistantSuggestionsModelObserver& observer : observers_)
    observer.OnConversationStartersChanged(conversation_starters);
}

void AssistantSuggestionsModel::NotifyProactiveSuggestionsChanged(
    const scoped_refptr<const ProactiveSuggestions>&
        old_proactive_suggestions) {
  for (AssistantSuggestionsModelObserver& observer : observers_) {
    observer.OnProactiveSuggestionsChanged(proactive_suggestions_,
                                           old_proactive_suggestions);
  }
}

}  // namespace ash
