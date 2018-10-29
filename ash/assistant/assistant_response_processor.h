// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_ASSISTANT_RESPONSE_PROCESSOR_H_
#define ASH_ASSISTANT_ASSISTANT_RESPONSE_PROCESSOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

class AssistantController;
class AssistantCardElement;
class AssistantResponse;
enum class AssistantUiElementType;

// The AssistantResponseProcessor is responsible for performing any processing
// steps necessary on an Assistant response before it is ready for presentation.
class AssistantResponseProcessor {
 public:
  using ProcessCallback = base::OnceCallback<void(bool)>;

  explicit AssistantResponseProcessor(
      AssistantController* assistant_controller);
  ~AssistantResponseProcessor();

  // Performs processing of the specified Assistant |response|. Upon completion
  // of processing, |callback| is run indicating success or failure. Note that
  // only one Assistant response may be processed at a time. Calling this method
  // while another response is being processed will abort the previous task.
  void Process(AssistantResponse& response, ProcessCallback callback);

 private:
  // Encapsulates a processing task for a given Assistant response. Upon task
  // abort/completion, the associated callback should be run.
  struct Task {
   public:
    Task(AssistantResponse& response, ProcessCallback callback);
    ~Task();

    // Weak pointer to the response being processed.
    base::WeakPtr<AssistantResponse> response;

    // Callback to be run on task abort/completion.
    ProcessCallback callback;

    // Count of UI elements that are being asynchronously processed.
    int processing_count = 0;
  };

  // Processes a card element as a part of the task identified by |task_id|.
  void ProcessCardElement(AssistantCardElement* card_element);

  // Invoked when a card element has completed processing. The event is
  // associated with the task identified by |task_id| for the specified
  // |card_element|.
  void OnCardElementProcessed(
      AssistantCardElement* card_element,
      const base::Optional<base::UnguessableToken>& embed_token);

  // Checks if the |task_| exists. If so, processing is aborted, the callback
  // associated with the task is run and the task is cleaned up. Otherwise this
  // is a no-op.
  void TryAbortingTask();

  // Checks if the |task_| is finished processing. If so, the callback
  // associated with the task is run and the task is cleaned up. Otherwise this
  // is a no-op.
  void TryFinishingTask();

  AssistantController* const assistant_controller_;  // Owned by Shell.

  base::Optional<Task> task_;

  base::WeakPtrFactory<AssistantResponseProcessor> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(AssistantResponseProcessor);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_ASSISTANT_RESPONSE_PROCESSOR_H_
