// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/legion_model_execution_fetcher.h"

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
#include "components/private_ai/proto/legion.pb.h"

namespace optimization_guide {
namespace {
private_ai::proto::FeatureName ToLegionFeatureName(
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
  // TODO(crbug.com/460052805): Figure out how to store legion errors.
  return OptimizationGuideModelExecutionError::FromModelExecutionError(
      OptimizationGuideModelExecutionError::ModelExecutionError::kUnknown);
}

}  // namespace

LegionModelExecutionFetcher::LegionModelExecutionFetcher(
    private_ai::Client* legion_client)
    : legion_client_(legion_client) {
  CHECK(legion_client);
}

LegionModelExecutionFetcher::~LegionModelExecutionFetcher() = default;

void LegionModelExecutionFetcher::ExecuteModel(
    ModelBasedCapabilityKey feature,
    signin::IdentityManager* identity_manager,
    const google::protobuf::MessageLite& request_metadata,
    std::optional<base::TimeDelta> timeout,
    ModelExecuteResponseCallback callback) {
  auto legion_feature_name = ToLegionFeatureName(feature);

  private_ai::proto::PaicMessage paic_message;
  paic_message.set_feature_name(legion_feature_name);
  *paic_message.mutable_execute_request_ext() =
      ToExecuteRequest(feature, request_metadata);

  private_ai::Client::RequestOptions options;
  if (timeout) {
    options.timeout = *timeout;
  }

  legion_client_->SendPaicRequest(
      legion_feature_name, paic_message,
      base::BindOnce(
          [](ModelBasedCapabilityKey feature,
             ModelExecuteResponseCallback callback,
             base::expected<private_ai::proto::PaicMessage,
                            private_ai::ErrorCode> result) {
            if (!result.has_value()) {
              std::move(callback).Run(
                  base::unexpected(ToModelExecutionError(result.error())));
              return;
            }

            if (!result->has_execute_response_ext()) {
              std::move(callback).Run(base::unexpected(
                  OptimizationGuideModelExecutionError::FromModelExecutionError(
                      OptimizationGuideModelExecutionError::
                          ModelExecutionError::kUnknown)));
              return;
            }

            std::move(callback).Run(base::ok(result->execute_response_ext()));
          },
          feature, std::move(callback)),
      options);
}

}  // namespace optimization_guide
