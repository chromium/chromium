// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_INTERACTION_MODEL_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_INTERACTION_MODEL_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/assistant/model/assistant_query_history.h"
#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"

namespace ash {

class AssistantInteractionModelObserver;
class AssistantQuery;
class AssistantResponse;

// Enumeration of interaction input modalities.
enum class InputModality {
  kKeyboard,
  kVoice,
};

// TODO(dmblack): This is an oversimplification. We will eventually want to
// distinctly represent listening/thinking/etc. states explicitly so they can
// be adequately represented in the UI.
// Enumeration of interaction states.
enum class InteractionState {
  kActive,
  kInactive,
};

// Enumeration of interaction mic states.
enum class MicState {
  kClosed,
  kOpen,
};

// Models the Assistant interaction. This includes query state, state of speech
// recognition, as well as a renderable AssistantResponse.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantInteractionModel {
 public:
  AssistantInteractionModel();

  AssistantInteractionModel(const AssistantInteractionModel&) = delete;
  AssistantInteractionModel& operator=(const AssistantInteractionModel&) =
      delete;

  ~AssistantInteractionModel();

  // Adds/removes the specified interaction model |observer|.
  void AddObserver(AssistantInteractionModelObserver* observer) const;
  void RemoveObserver(AssistantInteractionModelObserver* observer) const;

  // Resets the interaction to its initial state.
  void ClearInteraction();

  // Resets the interaction to its initial state. There are instances in which
  // we wish to clear the interaction but retain the committed query. Similarly,
  // there are instances in which we wish to retain the pending response that is
  // currently cached. For such instances, use |retain_committed_query| and
  // |retain_pending_response| respectively.
  void ClearInteraction(bool retain_committed_query,
                        bool retain_pending_response);

  // Sets the interaction state.
  void SetInteractionState(InteractionState interaction_state);

  // Returns the interaction state.
  InteractionState interaction_state() const { return interaction_state_; }

  // Updates the input modality for the interaction.
  void SetInputModality(InputModality input_modality);

  // Returns the input modality for the interaction.
  InputModality input_modality() const { return input_modality_; }

  // Updates the mic state for the interaction.
  void SetMicState(MicState mic_state);

  // Returns the mic state for the interaction.
  MicState mic_state() const { return mic_state_; }

  // Returns the committed query for the interaction.
  const AssistantQuery& committed_query() const { return *committed_query_; }

  // Clears the committed query for the interaction.
  void ClearCommittedQuery();

  // Updates the pending query for the interaction.
  void SetPendingQuery(std::unique_ptr<AssistantQuery> pending_query);

  // Returns the pending query for the interaction.
  const AssistantQuery& pending_query() const { return *pending_query_; }

  // Commits the pending query for the interaction.
  void CommitPendingQuery();

  // Clears the pending query for the interaction.
  void ClearPendingQuery();

  // Sets the pending response for the interaction.
  void SetPendingResponse(scoped_refptr<AssistantResponse> response);

  // Returns the pending response for the interaction.
  AssistantResponse* pending_response() { return pending_response_.get(); }

  // Commits the pending response for the interaction. Note that this will cause
  // the previously committed response, if one exists, to be animated off stage
  // after which the newly committed response will begin rendering.
  void CommitPendingResponse();

  // Clears the pending response for the interaction.
  void ClearPendingResponse();

  // Returns the committed response for the interaction.
  AssistantResponse* response() { return response_.get(); }
  const AssistantResponse* response() const { return response_.get(); }

  // Clears the committed response for the interaction.
  void ClearResponse();

  // Updates the speech level in dB.
  void SetSpeechLevel(float speech_level_db);

  // Returns the reference to query history.
  AssistantQueryHistory& query_history() { return query_history_; }

  // Returns the const reference to query history.
  const AssistantQueryHistory& query_history() const { return query_history_; }

 private:
  void NotifyInteractionStateChanged();
  void NotifyInputModalityChanged();
  void NotifyMicStateChanged();
  void NotifyCommittedQueryChanged();
  void NotifyCommittedQueryCleared();
  void NotifyPendingQueryChanged();
  void NotifyPendingQueryCleared(bool due_to_commit);
  void NotifyResponseChanged();
  void NotifyResponseCleared();
  void NotifySpeechLevelChanged(float speech_level_db);

  InteractionState interaction_state_ = InteractionState::kInactive;
  InputModality input_modality_ = InputModality::kKeyboard;
  MicState mic_state_ = MicState::kClosed;
  AssistantQueryHistory query_history_;
  std::unique_ptr<AssistantQuery> committed_query_;
  std::unique_ptr<AssistantQuery> pending_query_;
  scoped_refptr<AssistantResponse> pending_response_;
  scoped_refptr<AssistantResponse> response_;

  mutable base::ObserverList<AssistantInteractionModelObserver> observers_;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_INTERACTION_MODEL_H_
