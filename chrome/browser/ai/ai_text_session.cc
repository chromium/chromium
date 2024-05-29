// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_text_session.h"

#include <optional>

#include "base/functional/bind.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_text_session.mojom-shared.h"

using ModelExecutionError = optimization_guide::
    OptimizationGuideModelExecutionError::ModelExecutionError;

AITextSession::AITextSession(
    std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
        session)
    : session_(std::move(session)) {}

AITextSession::~AITextSession() = default;

blink::mojom::ModelStreamingResponseStatus ConvertModelExecutionError(
    ModelExecutionError error) {
  switch (error) {
    case ModelExecutionError::kUnknown:
      return blink::mojom::ModelStreamingResponseStatus::kErrorUnknown;
    case ModelExecutionError::kInvalidRequest:
      return blink::mojom::ModelStreamingResponseStatus::kErrorInvalidRequest;
    case ModelExecutionError::kRequestThrottled:
      return blink::mojom::ModelStreamingResponseStatus::kErrorRequestThrottled;
    case ModelExecutionError::kPermissionDenied:
      return blink::mojom::ModelStreamingResponseStatus::kErrorPermissionDenied;
    case ModelExecutionError::kGenericFailure:
      return blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure;
    case ModelExecutionError::kRetryableError:
      return blink::mojom::ModelStreamingResponseStatus::kErrorRetryableError;
    case ModelExecutionError::kNonRetryableError:
      return blink::mojom::ModelStreamingResponseStatus::
          kErrorNonRetryableError;
    case ModelExecutionError::kUnsupportedLanguage:
      return blink::mojom::ModelStreamingResponseStatus::
          kErrorUnsupportedLanguage;
    case ModelExecutionError::kFiltered:
      return blink::mojom::ModelStreamingResponseStatus::kErrorFiltered;
    case ModelExecutionError::kDisabled:
      return blink::mojom::ModelStreamingResponseStatus::kErrorDisabled;
    case ModelExecutionError::kCancelled:
      return blink::mojom::ModelStreamingResponseStatus::kErrorCancelled;
  }
}

void AITextSession::ModelExecutionCallback(
    mojo::RemoteSetElementId responder_id,
    optimization_guide::OptimizationGuideModelStreamingExecutionResult result) {
  blink::mojom::ModelStreamingResponder* responder =
      responder_set_.Get(responder_id);
  if (!responder) {
    return;
  }

  if (!result.response.has_value()) {
    responder->OnResponse(
        ConvertModelExecutionError(result.response.error().error()),
        std::nullopt);
    return;
  }

  auto response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::StringValue>(result.response->response);
  if (response->has_value()) {
    responder->OnResponse(blink::mojom::ModelStreamingResponseStatus::kOngoing,
                          response->value());
  }
  if (result.response->is_complete) {
    responder->OnResponse(blink::mojom::ModelStreamingResponseStatus::kComplete,
                          std::nullopt);
  }
}

void AITextSession::Prompt(
    const std::string& input,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
        pending_responder) {
  if (!session_) {
    mojo::Remote<blink::mojom::ModelStreamingResponder> responder(
        std::move(pending_responder));
    responder->OnResponse(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed,
        std::nullopt);
    return;
  }

  mojo::RemoteSetElementId responder_id =
      responder_set_.Add(std::move(pending_responder));
  optimization_guide::proto::StringValue request;
  request.set_value(input);
  session_->ExecuteModel(
      request,
      base::BindRepeating(&AITextSession::ModelExecutionCallback,
                          weak_ptr_factory_.GetWeakPtr(), responder_id));
}

void AITextSession::Destroy() {
  if (session_) {
    session_.reset();
  }

  for (auto& responder : responder_set_) {
    responder->OnResponse(
        blink::mojom::ModelStreamingResponseStatus::kErrorSessionDestroyed,
        std::nullopt);
  }

  responder_set_.Clear();
}
