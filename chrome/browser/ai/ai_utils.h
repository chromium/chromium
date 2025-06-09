// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_UTILS_H_
#define CHROME_BROWSER_AI_AI_UTILS_H_

#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom.h"

class AIUtils {
 public:
  using LanguageCodes =
      std::optional<std::vector<blink::mojom::AILanguageCodePtr>>;

  template <typename ClientRemoteInterface>
  static void SendClientRemoteError(
      const mojo::Remote<ClientRemoteInterface>& client_remote,
      blink::mojom::AIManagerCreateClientError error,
      blink::mojom::QuotaErrorInfoPtr quota_error_info = nullptr) {
    if (client_remote) {
      client_remote->OnError(error, std::move(quota_error_info));
    }
  }

  static void SendStreamingStatus(
      const mojo::Remote<blink::mojom::ModelStreamingResponder>& responder,
      blink::mojom::ModelStreamingResponseStatus status,
      blink::mojom::QuotaErrorInfoPtr quota_error_info = nullptr) {
    if (responder) {
      responder->OnError(status, std::move(quota_error_info));
    }
  }

  static void SendStreamingStatus(
      blink::mojom::ModelStreamingResponder* responder,
      blink::mojom::ModelStreamingResponseStatus status,
      blink::mojom::QuotaErrorInfoPtr quota_error_info = nullptr) {
    if (responder) {
      responder->OnError(status, std::move(quota_error_info));
    }
  }

  static blink::mojom::ModelStreamingResponseStatus ConvertModelExecutionError(
      optimization_guide::OptimizationGuideModelExecutionError::
          ModelExecutionError error);

  static constexpr int kNormalizedDownloadProgressMax = 0x10000;

  // Normalizes the model download progress by scaling `bytes_so_far` from
  // having `total_bytes` its max to having a `kNormalizedDownloadProgressMax`
  // as its max.
  static int64_t NormalizeModelDownloadProgress(int64_t bytes_so_far,
                                                int64_t total_bytes);
};

#endif  // CHROME_BROWSER_AI_AI_UTILS_H_
