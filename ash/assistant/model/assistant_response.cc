// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_response.h"

#include <utility>

#include "ash/assistant/model/assistant_response_observer.h"
#include "ash/assistant/model/ui/assistant_error_element.h"
#include "ash/assistant/model/ui/assistant_ui_element.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/ranges/algorithm.h"
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

  void Process() {
    // Responses should only be processed once.
    DCHECK_EQ(ProcessingState::kUnprocessed, response_->processing_state());
    response_->set_processing_state(ProcessingState::kProcessing);

    // Completion of |response_| processing is indicated by |processing_count_|
    // reaching zero. This value is decremented as each UI element is processed.
    processing_count_ = response_->GetUiElements().size();

    // Try finishing directly if there are no UI elements to be processed.
    if (processing_count_ == 0) {
      TryFinishing();
      return;
    }

    for (const auto& ui_element : response_->GetUiElements()) {
      // Start asynchronous processing of the UI element. Note that if the UI
      // element does not require any pre-rendering processing the callback may
      // be run synchronously. Also we must use WeakPtr here because |this| will
      // destroy before |ui_element| by design.
      ui_element->Process(
          base::BindOnce(&AssistantResponse::Processor::OnFinishedProcessing,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }

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

void AssistantResponse::AddUiElement(
    std::unique_ptr<AssistantUiElement> ui_element) {
  // In processing v2, UI elements are first cached in a pending state...
  auto pending_ui_element = std::make_unique<PendingUiElement>();
  pending_ui_element->ui_element = std::move(ui_element);
  pending_ui_element->is_processing = true;
  pending_ui_elements_.push_back(std::move(pending_ui_element));

  // ...while we perform any pre-processing necessary prior to rendering.
  pending_ui_elements_.back()->ui_element->Process(base::BindOnce(
      [](const base::WeakPtr<AssistantResponse>& self,
         PendingUiElement* pending_ui_element) {
        if (!self)
          return;

        // Indicate that |pending_ui_element| has finished processing.
        pending_ui_element->is_processing = false;

        // Add any UI elements that are ready for rendering to the response.
        // Note that this may or may not include the |pending_ui_element| which
        // just finished processing as we are required to add renderable UI
        // elements to the response in the same order that they were initially
        // pended to avoid inadvertently shuffling the response.
        while (!self->pending_ui_elements_.empty() &&
               !self->pending_ui_elements_.front()->is_processing) {
          self->ui_elements_.push_back(
              std::move(self->pending_ui_elements_.front()->ui_element));
          self->pending_ui_elements_.pop_front();
          self->NotifyUiElementAdded(self->ui_elements_.back().get());
        }
      },
      weak_factory_.GetWeakPtr(),
      base::Unretained(pending_ui_elements_.back().get())));
}

const std::vector<std::unique_ptr<AssistantUiElement>>&
AssistantResponse::GetUiElements() const {
  return ui_elements_;
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
  DCHECK(element);

  bool contains_element = base::Contains(
      ui_elements_, *element, &std::unique_ptr<AssistantUiElement>::operator*);

  return contains_element || ContainsPendingUiElement(element);
}

bool AssistantResponse::ContainsPendingUiElement(
    const AssistantUiElement* element) const {
  DCHECK(element);

  return base::ranges::any_of(
      pending_ui_elements_,
      [element](const std::unique_ptr<PendingUiElement>& other) {
        return *other->ui_element == *element;
      });
}
}  // namespace ash
