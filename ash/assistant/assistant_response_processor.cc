// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_response_processor.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/model/assistant_response.h"
#include "ash/assistant/model/assistant_ui_element.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "base/base64.h"

namespace ash {

namespace {

// WebContents.
constexpr char kDataUriPrefix[] = "data:text/html;base64,";

}  // namespace

// AssistantResponseProcessor::Task --------------------------------------------

AssistantResponseProcessor::Task::Task(AssistantResponse& response,
                                       ProcessCallback callback)
    : response(response.GetWeakPtr()), callback(std::move(callback)) {}

AssistantResponseProcessor::Task::~Task() = default;

// AssistantResponseProcessor --------------------------------------------------

AssistantResponseProcessor::AssistantResponseProcessor(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller), weak_factory_(this) {}

AssistantResponseProcessor::~AssistantResponseProcessor() = default;

void AssistantResponseProcessor::Process(AssistantResponse& response,
                                         ProcessCallback callback) {
  // We should only attempt to process responses that are unprocessed.
  DCHECK_EQ(AssistantResponse::ProcessingState::kUnprocessed,
            response.processing_state());

  // Update processing state.
  response.set_processing_state(
      AssistantResponse::ProcessingState::kProcessing);

  // We only support processing a single task at a time. As such, we should
  // abort any task in progress before creating and starting a new one.
  TryAbortingTask();

  // Create a task.
  task_.emplace(/*response=*/response,
                /*callback=*/std::move(callback));

  // Start processing UI elements.
  for (const auto& ui_element : response.GetUiElements()) {
    switch (ui_element->GetType()) {
      case AssistantUiElementType::kCard:
        ProcessCardElement(
            static_cast<AssistantCardElement*>(ui_element.get()));
        break;
      case AssistantUiElementType::kText:
        // No processing necessary.
        break;
    }
  }

  // Try finishing. This will no-op if there are still UI elements being
  // processed asynchronously.
  TryFinishingTask();
}

void AssistantResponseProcessor::ProcessCardElement(
    AssistantCardElement* card_element) {
  // Encode the card HTML using base64.
  std::string encoded_html;
  base::Base64Encode(card_element->html(), &encoded_html);

  // TODO(dmblack): Find a better way of determining desired card size.
  const int width_dip = kPreferredWidthDip - 2 * kUiElementHorizontalMarginDip;

  // Configure parameters for the card.
  ash::mojom::ManagedWebContentsParamsPtr params(
      ash::mojom::ManagedWebContentsParams::New());
  params->url = GURL(kDataUriPrefix + encoded_html);
  params->min_size_dip = gfx::Size(width_dip, 0);
  params->max_size_dip = gfx::Size(width_dip, INT_MAX);

  // Request an embed token for the card whose WebContents will be owned by
  // WebContentsManager.
  assistant_controller_->ManageWebContents(
      card_element->id_token(), std::move(params),
      base::BindOnce(&AssistantResponseProcessor::OnCardElementProcessed,
                     weak_factory_.GetWeakPtr(), card_element));

  // Increment |processing_count| to reflect the fact that a card element is
  // being processed asynchronously.
  ++task_->processing_count;
}

void AssistantResponseProcessor::OnCardElementProcessed(
    AssistantCardElement* card_element,
    const base::Optional<base::UnguessableToken>& embed_token) {
  // If the response has been invalidated we should abort early.
  if (!task_->response) {
    TryAbortingTask();
    return;
  }

  // Save the |embed_token|.
  card_element->set_embed_token(embed_token);

  // Decrement |processing_count| to reflect the fact that a card element has
  // finished being processed asynchronously.
  --task_->processing_count;

  // Try finishing. This will no-op if there are still UI elements being
  // processed asynchronously.
  TryFinishingTask();
}

void AssistantResponseProcessor::TryAbortingTask() {
  if (!task_)
    return;

  // Invalidate weak pointers to prevent processing callbacks from running.
  // Otherwise we might continue receiving card events for the aborted task.
  weak_factory_.InvalidateWeakPtrs();

  // Notify our callback and clean up any task related resources.
  std::move(task_->callback).Run(/*success=*/false);
  task_.reset();
}

void AssistantResponseProcessor::TryFinishingTask() {
  // This method is a no-op if we are still processing.
  if (task_->processing_count > 0)
    return;

  // Update processing state.
  task_->response->set_processing_state(
      AssistantResponse::ProcessingState::kProcessed);

  // Notify our callback and clean up any task related resources.
  std::move(task_->callback).Run(/*success=*/true);
  task_.reset();
}

}  // namespace ash
