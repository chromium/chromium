// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_response.h"

#include <algorithm>
#include <utility>

#include "ash/assistant/model/assistant_response_observer.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"

namespace ash {

// AssistantResponse::PendingUiElement -----------------------------------------

struct AssistantResponse::PendingUiElement {
 public:
  PendingUiElement() = default;
  ~PendingUiElement() = default;

  PendingUiElement(const PendingUiElement&) = delete;
  PendingUiElement& operator=(const PendingUiElement&) = delete;

  std::unique_ptr<AssistantUiElement> ui_element;
  bool is_processing = false;
};

// AssistantResponse::Processor ------------------------------------------------

class AssistantResponse::Processor {
 public:
  Processor(AssistantResponse* response, ProcessingCallback callback)
      : response_(response), callback_(std::move(callback)) {}

  Processor(const Processor& copy) = delete;
  Processor& operator=(const Processor& assign) = delete;

  ~Processor() {
    if (callback_)
      std::move(callback_).Run(/*is_completed=*/false);
  }

  void Process() {}

 private:
  void OnFinishedProcessing() {
    // We handle success/failure cases the same because failures will be skipped
    // in view handling. We decrement our |processing_count_| and attempt to
    // finish response processing. This will no-op if elements are still
    // processing.
    --processing_count_;
    TryFinishing();
  }

  void TryFinishing() {
    // No-op if we are already finished or if elements are still processing.
    if (!callback_ || processing_count_ > 0)
      return;

    // Notify processing completion.
    response_->set_processing_state(ProcessingState::kProcessed);
    std::move(callback_).Run(/*is_completed=*/true);
  }

  // |response_| should outlive the Processor.
  const raw_ptr<AssistantResponse> response_;
  ProcessingCallback callback_;

  int processing_count_ = 0;
  base::WeakPtrFactory<AssistantResponse::Processor> weak_ptr_factory_{this};
};

// AssistantResponse -----------------------------------------------------------

AssistantResponse::AssistantResponse() = default;

AssistantResponse::~AssistantResponse() {
  // Reset |processor_| explicitly in the destructor to guarantee the correct
  // lifecycle where |this| should outlive the |processor_|. This can also force
  // |processor_| to be destroyed before |ui_elements_| as we want regardless of
  // the declaration order.
  processor_.reset();
}

void AssistantResponse::AddObserver(AssistantResponseObserver* observer) const {
  observers_.AddObserver(observer);
}

void AssistantResponse::RemoveObserver(
    AssistantResponseObserver* observer) const {
  observers_.RemoveObserver(observer);
}

void AssistantResponse::AddSuggestions(
    const std::vector<AssistantSuggestion>& suggestions) {
  for (const auto& suggestion : suggestions)
    suggestions_.push_back(suggestion);
  NotifySuggestionsAdded(suggestions);
}

const assistant::AssistantSuggestion* AssistantResponse::GetSuggestionById(
    const base::UnguessableToken& id) const {
  for (auto& suggestion : suggestions_) {
    if (suggestion.id == id)
      return &suggestion;
  }
  return nullptr;
}

const std::vector<assistant::AssistantSuggestion>&
AssistantResponse::GetSuggestions() const {
  return suggestions_;
}

void AssistantResponse::Process(ProcessingCallback callback) {
  processor_ = std::make_unique<Processor>(this, std::move(callback));
  processor_->Process();
}

void AssistantResponse::NotifyUiElementAdded(
    const AssistantUiElement* ui_element) {
  for (auto& observer : observers_)
    observer.OnUiElementAdded(ui_element);
}

void AssistantResponse::NotifySuggestionsAdded(
    const std::vector<AssistantSuggestion>& suggestions) {
  for (auto& observer : observers_)
    observer.OnSuggestionsAdded(suggestions);
}

bool AssistantResponse::ContainsUiElement(
    const AssistantUiElement* element) const {
  return false;
}

bool AssistantResponse::ContainsPendingUiElement(
    const AssistantUiElement* element) const {
  return false;
}
}  // namespace ash
