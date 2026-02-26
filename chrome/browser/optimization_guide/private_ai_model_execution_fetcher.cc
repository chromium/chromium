// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/private_ai_model_execution_fetcher.h"

#include <utility>

#include "base/strings/string_split.h"
#include "base/types/expected.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_fetcher.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/features/zero_state_suggestions.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/private_ai/client.h"
#include "components/private_ai/error_code.h"
#include "components/private_ai/proto/private_ai.pb.h"

namespace optimization_guide {
namespace {

using ModelExecutionError =
    OptimizationGuideModelExecutionError::ModelExecutionError;

private_ai::proto::FeatureName ToPrivateAiFeatureName(
    ModelBasedCapabilityKey feature) {
  switch (feature) {
    case ModelBasedCapabilityKey::kZeroStateSuggestions:
      return private_ai::proto::FeatureName::
          FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION;
    default:
      NOTREACHED() << feature;
  }
}

OptimizationGuideModelExecutionError ToModelExecutionError(
    private_ai::ErrorCode error) {
  switch (error) {
    case private_ai::ErrorCode::kAuthenticationFailed:
    case private_ai::ErrorCode::kClientAttestationFailed:
      return OptimizationGuideModelExecutionError::FromModelExecutionError(
          ModelExecutionError::kPermissionDenied);
    case private_ai::ErrorCode::kTimeout:
      return OptimizationGuideModelExecutionError::FromModelExecutionError(
          ModelExecutionError::kRetryableError);
    case private_ai::ErrorCode::kDestroyed:
      return OptimizationGuideModelExecutionError::FromModelExecutionError(
          ModelExecutionError::kCancelled);
    default:
      return OptimizationGuideModelExecutionError::FromModelExecutionError(
          ModelExecutionError::kUnknown);
  }
}

}  // namespace

PrivateAiModelExecutionFetcher::PrivateAiModelExecutionFetcher(
    private_ai::Client* private_ai_client)
    : private_ai_client_(private_ai_client) {
  CHECK(private_ai_client);
}

PrivateAiModelExecutionFetcher::~PrivateAiModelExecutionFetcher() = default;

void PrivateAiModelExecutionFetcher::ExecuteModel(
    ModelBasedCapabilityKey feature,
    signin::IdentityManager* identity_manager,
    const google::protobuf::MessageLite& request_metadata,
    std::optional<base::TimeDelta> timeout,
    ModelExecuteResponseCallback callback) {
  auto private_ai_feature_name = ToPrivateAiFeatureName(feature);

  private_ai::proto::PaicMessage paic_message;
  paic_message.set_feature_name(private_ai_feature_name);
  *paic_message.mutable_execute_request_ext() =
      ToExecuteRequest(feature, request_metadata);

  private_ai::Client::RequestOptions options;
  if (timeout) {
    options.timeout = *timeout;
  }

  private_ai_client_->SendPaicRequest(
      private_ai_feature_name, paic_message,
      base::BindOnce(
          [](ModelBasedCapabilityKey feature,
             ModelExecuteResponseCallback callback,
             base::expected<private_ai::proto::PaicMessage,
                            private_ai::ErrorCode> result) {
            if (!result.has_value()) {
              RecordRequestStatusHistogram(
                  feature, FetcherRequestStatus::kResponseError);
              std::move(callback).Run(
                  base::unexpected(ToModelExecutionError(result.error())));
              return;
            }

            if (!result->has_execute_response_ext()) {
              RecordRequestStatusHistogram(
                  feature, FetcherRequestStatus::kResponseError);
              std::move(callback).Run(base::unexpected(
                  OptimizationGuideModelExecutionError::FromModelExecutionError(
                      ModelExecutionError::kUnknown)));
              return;
            }

            RecordRequestStatusHistogram(feature,
                                         FetcherRequestStatus::kSuccess);
            std::move(callback).Run(base::ok(result->execute_response_ext()));
          },
          feature, std::move(callback)),
      options);
}

}  // namespace optimization_guide
