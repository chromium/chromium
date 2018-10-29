// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_response.h"

#include "ash/assistant/model/assistant_response_observer.h"
#include "ash/assistant/model/assistant_ui_element.h"

namespace ash {

AssistantResponse::AssistantResponse() : weak_factory_(this) {}

AssistantResponse::~AssistantResponse() {
  NotifyDestroying();
}

void AssistantResponse::AddObserver(AssistantResponseObserver* observer) {
  observers_.AddObserver(observer);
}

void AssistantResponse::RemoveObserver(AssistantResponseObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AssistantResponse::AddUiElement(
    std::unique_ptr<AssistantUiElement> ui_element) {
  ui_elements_.push_back(std::move(ui_element));
}

const std::vector<std::unique_ptr<AssistantUiElement>>&
AssistantResponse::GetUiElements() const {
  return ui_elements_;
}

void AssistantResponse::AddSuggestions(
    std::vector<AssistantSuggestionPtr> suggestions) {
  for (AssistantSuggestionPtr& suggestion : suggestions)
    suggestions_.push_back(std::move(suggestion));
}

const chromeos::assistant::mojom::AssistantSuggestion*
AssistantResponse::GetSuggestionById(int id) const {
  // We consider the index of a suggestion within our backing vector to be its
  // unique identifier.
  DCHECK_GE(id, 0);
  DCHECK_LT(id, static_cast<int>(suggestions_.size()));
  return suggestions_.at(id).get();
}

std::map<int, const chromeos::assistant::mojom::AssistantSuggestion*>
AssistantResponse::GetSuggestions() const {
  std::map<int, const AssistantSuggestion*> suggestions;

  // We use index within our backing vector to represent the unique identifier
  // for a suggestion.
  int id = 0;
  for (const AssistantSuggestionPtr& suggestion : suggestions_)
    suggestions[id++] = suggestion.get();

  return suggestions;
}

base::WeakPtr<AssistantResponse> AssistantResponse::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AssistantResponse::NotifyDestroying() {
  for (auto& observer : observers_)
    observer.OnResponseDestroying(*this);
}

}  // namespace ash