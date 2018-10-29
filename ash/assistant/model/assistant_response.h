// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_RESPONSE_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_RESPONSE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"

namespace ash {

class AssistantResponseObserver;
class AssistantUiElement;

// Models a renderable Assistant response.
class AssistantResponse {
 public:
  enum class ProcessingState {
    kUnprocessed,  // Response has not yet been processed.
    kProcessing,   // Response is currently being processed.
    kProcessed,    // Response has finished processing.
  };

  using AssistantSuggestion = chromeos::assistant::mojom::AssistantSuggestion;
  using AssistantSuggestionPtr =
      chromeos::assistant::mojom::AssistantSuggestionPtr;

  AssistantResponse();
  ~AssistantResponse();

  // Adds/removes the specified |observer|.
  void AddObserver(AssistantResponseObserver* observer);
  void RemoveObserver(AssistantResponseObserver* observer);

  // Adds the specified |ui_element| that should be rendered for the
  // interaction.
  void AddUiElement(std::unique_ptr<AssistantUiElement> ui_element);

  // Returns all UI elements belonging to the response.
  const std::vector<std::unique_ptr<AssistantUiElement>>& GetUiElements() const;

  // Adds the specified |suggestions| that should be rendered for the
  // interaction.
  void AddSuggestions(std::vector<AssistantSuggestionPtr> suggestions);

  // Returns the suggestion uniquely identified by |id|.
  const AssistantSuggestion* GetSuggestionById(int id) const;

  // Returns all suggestions belongs to the response, mapped to a unique id.
  std::map<int, const AssistantSuggestion*> GetSuggestions() const;

  // Gets/sets the processing state for the response.
  ProcessingState processing_state() const { return processing_state_; }
  void set_processing_state(ProcessingState processing_state) {
    processing_state_ = processing_state;
  }

  // Gets/sets if the response has TTS. This can only be reliably checked after
  // the response is finalized for obvious reasons.
  bool has_tts() const { return has_tts_; }
  void set_has_tts(bool has_tts) { has_tts_ = has_tts; }

  // Returns a weak pointer to this instance.
  base::WeakPtr<AssistantResponse> GetWeakPtr();

 private:
  void NotifyDestroying();

  std::vector<std::unique_ptr<AssistantUiElement>> ui_elements_;
  std::vector<AssistantSuggestionPtr> suggestions_;
  ProcessingState processing_state_ = ProcessingState::kUnprocessed;
  bool has_tts_ = false;

  base::ObserverList<AssistantResponseObserver>::Unchecked observers_;

  base::WeakPtrFactory<AssistantResponse> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AssistantResponse);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_RESPONSE_H_
