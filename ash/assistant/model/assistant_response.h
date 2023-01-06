// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_RESPONSE_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_RESPONSE_H_

#include <deque>
#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_suggestion.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace ash {

class AssistantResponseObserver;
class AssistantUiElement;

// TODO(dmblack): Remove ProcessingState after launch of response processing v2.
// Models a renderable Assistant response.
// It is refcounted so that views that display the response can safely
// reference the data inside this response.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantResponse
    : public base::RefCounted<AssistantResponse> {
 public:
  using AssistantSuggestion = assistant::AssistantSuggestion;
  using ProcessingCallback = base::OnceCallback<void(bool)>;

  enum class ProcessingState {
    kUnprocessed,  // Response has not yet been processed.
    kProcessing,   // Response is currently being processed.
    kProcessed,    // Response has finished processing.
  };

  AssistantResponse();

  AssistantResponse(const AssistantResponse&) = delete;
  AssistantResponse& operator=(const AssistantResponse&) = delete;

  // Adds/removes the specified |observer|.
  // NOTE: only the AssistantInteractionController is able to obtain non-const
  // access to an AssistantResponse through its owned model, but there are const
  // accessors who wish to observe the response for changes in its underlying
  // data. To accomplish this, we make AddObserver() and RemoveObserver() const,
  // though these methods do modify the underlying ObserverList. This is safe to
  // do as AssistantResponseObserver only exposes const access to the underlying
  // response data and so doesn't expose AssistantResponse for modification.
  void AddObserver(AssistantResponseObserver* observer) const;
  void RemoveObserver(AssistantResponseObserver* observer) const;

  // Adds the specified |ui_element| that should be rendered for the
  // interaction.
  void AddUiElement(std::unique_ptr<AssistantUiElement> ui_element);

  // Returns all UI elements belonging to the response.
  const std::vector<std::unique_ptr<AssistantUiElement>>& GetUiElements() const;

  // Adds the specified |suggestions| that should be rendered for the
  // interaction.
  void AddSuggestions(const std::vector<AssistantSuggestion>& suggestions);

  // Returns the suggestion uniquely identified by |id|.
  const AssistantSuggestion* GetSuggestionById(
      const base::UnguessableToken& id) const;

  // Returns all suggestions belongs to the response.
  const std::vector<AssistantSuggestion>& GetSuggestions() const;

  // Gets/sets the processing state for the response.
  ProcessingState processing_state() const { return processing_state_; }
  void set_processing_state(ProcessingState processing_state) {
    processing_state_ = processing_state;
  }

  // Gets/sets if the response has TTS. This can only be reliably checked after
  // the response is finalized for obvious reasons.
  bool has_tts() const { return has_tts_; }
  void set_has_tts(bool has_tts) { has_tts_ = has_tts; }

  // Invoke to begin processing the response. The specified |callback| will be
  // run to indicate whether or not the processor has completed processing of
  // all UI elements in the response.
  void Process(ProcessingCallback callback);

  // Return true if this response contains an identical ui element.
  bool ContainsUiElement(const AssistantUiElement* element) const;

 private:
  void NotifyUiElementAdded(const AssistantUiElement* ui_element);
  void NotifySuggestionsAdded(const std::vector<AssistantSuggestion>&);

  // Return true if the pending ui elements contain an identical ui element.
  bool ContainsPendingUiElement(const AssistantUiElement* other) const;

  struct PendingUiElement;
  class Processor;

  friend class base::RefCounted<AssistantResponse>;
  ~AssistantResponse();

  std::deque<std::unique_ptr<PendingUiElement>> pending_ui_elements_;
  std::vector<AssistantSuggestion> suggestions_;
  ProcessingState processing_state_ = ProcessingState::kUnprocessed;
  bool has_tts_ = false;

  // We specify the declaration order below as intended because we want
  // |processor_| to be destroyed before |ui_elements_| (we also forced this
  // order in the destructor), so that when the response processing got
  // interrupted, the |ProcessingCallback| can have a chance to return false
  // during the destruction to indicate the failure of completion.
  std::vector<std::unique_ptr<AssistantUiElement>> ui_elements_;
  std::unique_ptr<Processor> processor_;

  mutable base::ObserverList<AssistantResponseObserver> observers_;

  base::WeakPtrFactory<AssistantResponse> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_RESPONSE_H_
