// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_RESPONSE_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_RESPONSE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/content/public/cpp/navigable_contents.h"

namespace ash {

class AssistantUiElement;

// Models a renderable Assistant response.
// It is refcounted so that views that display the response can safely
// reference the data inside this response.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantResponse
    : public base::RefCounted<AssistantResponse> {
 public:
  using AssistantSuggestion = chromeos::assistant::mojom::AssistantSuggestion;
  using AssistantSuggestionPtr =
      chromeos::assistant::mojom::AssistantSuggestionPtr;

  using ProcessingCallback = base::OnceCallback<void(bool)>;

  enum class ProcessingState {
    kUnprocessed,  // Response has not yet been processed.
    kProcessing,   // Response is currently being processed.
    kProcessed,    // Response has finished processing.
  };

  AssistantResponse();

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

  // Invoke to begin processing the response. Upon completion, |callback| will
  // be run to indicate success or failure.
  void Process(
      mojo::Remote<content::mojom::NavigableContentsFactory> contents_factory,
      ProcessingCallback callback);

 private:
  friend class base::RefCounted<AssistantResponse>;
  ~AssistantResponse();

  // Handles processing for an AssistantResponse.
  class Processor {
   public:
    Processor(
        AssistantResponse& response,
        mojo::Remote<content::mojom::NavigableContentsFactory> contents_factory,
        ProcessingCallback callback);
    ~Processor();

    // Invoke to begin processing.
    void Process();

   private:
    // Event fired upon completion of a UI element's asynchronous processing.
    // Once all asynchronous processing of UI elements has completed, the
    // response itself has finished processing.
    void OnFinishedProcessing(bool success);

    // Attempts to successfully complete response processing. This will no-op
    // if we have already finished or if elements are still processing.
    void TryFinishing();

    AssistantResponse& response_;
    mojo::Remote<content::mojom::NavigableContentsFactory> contents_factory_;
    ProcessingCallback callback_;

    int processing_count_ = 0;

    DISALLOW_COPY_AND_ASSIGN(Processor);
  };

  std::vector<std::unique_ptr<AssistantUiElement>> ui_elements_;
  std::vector<AssistantSuggestionPtr> suggestions_;
  ProcessingState processing_state_ = ProcessingState::kUnprocessed;
  bool has_tts_ = false;

  std::unique_ptr<Processor> processor_;

  DISALLOW_COPY_AND_ASSIGN(AssistantResponse);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_RESPONSE_H_
