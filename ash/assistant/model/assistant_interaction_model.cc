// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/model/assistant_interaction_model.h"

#include "ash/assistant/model/assistant_interaction_model_observer.h"
#include "ash/assistant/model/assistant_query.h"
#include "ash/assistant/model/assistant_response.h"

namespace ash {

AssistantInteractionModel::AssistantInteractionModel()
    : committed_query_(std::make_unique<AssistantNullQuery>()),
      pending_query_(std::make_unique<AssistantNullQuery>()) {}

AssistantInteractionModel::~AssistantInteractionModel() = default;

void AssistantInteractionModel::AddObserver(
    AssistantInteractionModelObserver* observer) const {
  observers_.AddObserver(observer);
}

void AssistantInteractionModel::RemoveObserver(
    AssistantInteractionModelObserver* observer) const {
  observers_.RemoveObserver(observer);
}

void AssistantInteractionModel::ClearInteraction() {
  ClearInteraction(/*retain_committed_query=*/false,
                   /*retain_pending_response=*/false);
}

void AssistantInteractionModel::ClearInteraction(bool retain_committed_query,
                                                 bool retain_pending_response) {
  if (!retain_committed_query)
    ClearCommittedQuery();

  ClearPendingQuery();

  if (!retain_pending_response)
    ClearPendingResponse();

  ClearResponse();
}

void AssistantInteractionModel::SetInteractionState(
    InteractionState interaction_state) {
  if (interaction_state == interaction_state_)
    return;

  interaction_state_ = interaction_state;
  NotifyInteractionStateChanged();
}

void AssistantInteractionModel::SetInputModality(InputModality input_modality) {
  if (input_modality == input_modality_)
    return;

  input_modality_ = input_modality;
  NotifyInputModalityChanged();
}

void AssistantInteractionModel::SetMicState(MicState mic_state) {
  if (mic_state == mic_state_)
    return;

  mic_state_ = mic_state;
  NotifyMicStateChanged();
}

void AssistantInteractionModel::ClearCommittedQuery() {
  if (committed_query_->type() == AssistantQueryType::kNull)
    return;

  committed_query_ = std::make_unique<AssistantNullQuery>();
  NotifyCommittedQueryCleared();
}

void AssistantInteractionModel::SetPendingQuery(
    std::unique_ptr<AssistantQuery> pending_query) {
  DCHECK(pending_query->type() != AssistantQueryType::kNull);
  pending_query_ = std::move(pending_query);
  NotifyPendingQueryChanged();
}

void AssistantInteractionModel::CommitPendingQuery() {
  DCHECK_NE(pending_query_->type(), AssistantQueryType::kNull);

  committed_query_ = std::move(pending_query_);
  pending_query_ = std::make_unique<AssistantNullQuery>();

  NotifyCommittedQueryChanged();
  NotifyPendingQueryCleared(/*due_to_commit=*/true);
}

void AssistantInteractionModel::ClearPendingQuery() {
  if (pending_query_->type() == AssistantQueryType::kNull)
    return;

  pending_query_ = std::make_unique<AssistantNullQuery>();
  NotifyPendingQueryCleared(/*due_to_commit=*/false);
}

void AssistantInteractionModel::SetPendingResponse(
    scoped_refptr<AssistantResponse> pending_response) {
  pending_response_ = std::move(pending_response);
}

void AssistantInteractionModel::CommitPendingResponse() {
  DCHECK(pending_response_);
  response_ = std::move(pending_response_);
  NotifyResponseChanged();
}

void AssistantInteractionModel::ClearPendingResponse() {
  pending_response_.reset();
}

void AssistantInteractionModel::ClearResponse() {
  response_.reset();
  NotifyResponseCleared();
}

void AssistantInteractionModel::SetSpeechLevel(float speech_level_db) {
  NotifySpeechLevelChanged(speech_level_db);
}

void AssistantInteractionModel::NotifyInteractionStateChanged() {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnInteractionStateChanged(interaction_state_);
}

void AssistantInteractionModel::NotifyInputModalityChanged() {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnInputModalityChanged(input_modality_);
}

void AssistantInteractionModel::NotifyMicStateChanged() {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnMicStateChanged(mic_state_);
}

void AssistantInteractionModel::NotifyCommittedQueryChanged() {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnCommittedQueryChanged(*committed_query_);
}

void AssistantInteractionModel::NotifyCommittedQueryCleared() {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnCommittedQueryCleared();
}

void AssistantInteractionModel::NotifyPendingQueryChanged() {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnPendingQueryChanged(*pending_query_);
}

void AssistantInteractionModel::NotifyPendingQueryCleared(bool due_to_commit) {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnPendingQueryCleared(due_to_commit);
}

void AssistantInteractionModel::NotifyResponseChanged() {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnResponseChanged(response_);
}

void AssistantInteractionModel::NotifyResponseCleared() {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnResponseCleared();
}

void AssistantInteractionModel::NotifySpeechLevelChanged(
    float speech_level_db) {
  for (AssistantInteractionModelObserver& observer : observers_)
    observer.OnSpeechLevelChanged(speech_level_db);
}

}  // namespace ash
