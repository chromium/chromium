// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_INTERACTION_MODEL_OBSERVER_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_INTERACTION_MODEL_OBSERVER_H_

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list_types.h"

namespace ash {

class AssistantQuery;
class AssistantResponse;
enum class InputModality;
enum class InteractionState;
enum class MicState;

// A checked observer which receives notification of changes to an Assistant
// interaction.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantInteractionModelObserver
    : public base::CheckedObserver {
 public:
  AssistantInteractionModelObserver(const AssistantInteractionModelObserver&) =
      delete;
  AssistantInteractionModelObserver& operator=(
      const AssistantInteractionModelObserver&) = delete;

  // Invoked when the interaction state is changed.
  virtual void OnInteractionStateChanged(InteractionState interaction_state) {}

  // Invoked when the input modality associated with the interaction is changed.
  virtual void OnInputModalityChanged(InputModality input_modality) {}

  // Invoked when the mic state associated with the interaction is changed.
  virtual void OnMicStateChanged(MicState mic_state) {}

  // Invoked when the committed query associated with the interaction is
  // changed.
  virtual void OnCommittedQueryChanged(const AssistantQuery& committed_query) {}

  // Invoked when the committed query associated with the interaction is
  // cleared.
  virtual void OnCommittedQueryCleared() {}

  // Invoked when the pending query associated with the interaction is changed.
  virtual void OnPendingQueryChanged(const AssistantQuery& pending_query) {}

  // Invoked when the pending query associated with the interaction is cleared.
  // |due_to_commit| is true if called from |CommitPendingQuery()|.
  virtual void OnPendingQueryCleared(bool due_to_commit) {}

  // Invoked when the response associated with the interaction is changed.
  virtual void OnResponseChanged(
      const scoped_refptr<AssistantResponse>& response) {}

  // Invoked when the response associated with the interaction is cleared.
  virtual void OnResponseCleared() {}

  // Invoked when the speech level is changed.
  virtual void OnSpeechLevelChanged(float speech_level_db) {}

 protected:
  AssistantInteractionModelObserver() = default;
  ~AssistantInteractionModelObserver() override = default;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_INTERACTION_MODEL_OBSERVER_H_
