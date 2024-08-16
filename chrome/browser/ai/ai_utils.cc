// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_utils.h"

#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

using ModelExecutionError = optimization_guide::
    OptimizationGuideModelExecutionError::ModelExecutionError;

blink::mojom::ModelStreamingResponseStatus AIUtils::ConvertModelExecutionError(
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
