// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/legion_model_execution_fetcher.h"

#include <utility>

#include "base/strings/string_split.h"
#include "components/legion/client.h"
#include "components/legion/error_code.h"
#include "components/legion/proto/legion.pb.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_fetcher.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/features/zero_state_suggestions.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"

namespace optimization_guide {
namespace {
legion::proto::FeatureName ToLegionFeatureName(
    ModelBasedCapabilityKey feature) {
  switch (feature) {
    case ModelBasedCapabilityKey::kZeroStateSuggestions:
      return legion::proto::FeatureName::
          FEATURE_NAME_CHROME_ZERO_STATE_SUGGESTION;
    default:
      NOTREACHED() << feature;
  }
}

OptimizationGuideModelExecutionError ToModelExecutionError(
    legion::ErrorCode error) {
  // TODO(crbug.com/460052805): Figure out how to store legion errors.
  return OptimizationGuideModelExecutionError::FromModelExecutionError(
      OptimizationGuideModelExecutionError::ModelExecutionError::kUnknown);
}

constexpr char prompt[] =
    "Please provide 3 short suggestions for what you could ask Gemini\n"
    "about the content of the following list of websites.\n"
    "Please provide each suggestion on a separate line and no other\n"
    "content in your response. No more than 30 characters per\n"
    "suggestion.\n"
    "Websites:\n";

std::string ToLegionRequest(
    ModelBasedCapabilityKey feature,
    const google::protobuf::MessageLite& request_metadata) {
  CHECK_EQ(feature, ModelBasedCapabilityKey::kZeroStateSuggestions);
  // TODO(crbug.com/460052805): Send proto directly.
  std::stringstream request;
  request << prompt;
  auto* zss_request = static_cast<
      const optimization_guide::proto::ZeroStateSuggestionsRequest*>(
      &request_metadata);
  if (zss_request->has_page_context()) {
    request << zss_request->page_context().url() << " - "
            << zss_request->page_context().title() << '\n';
  }
  if (zss_request->has_page_context_list()) {
    auto& contexts = zss_request->page_context_list().page_contexts();
    for (auto& context : contexts) {
      request << context.page_context().url() << " - "
              << context.page_context().title() << '\n';
    }
  }
  return request.str();
}

proto::ZeroStateSuggestionsResponse ToZSSResponse(const std::string& result) {
  proto::ZeroStateSuggestionsResponse zss;
  for (auto line : base::SplitStringPiece(
           result, "\n", base::WhitespaceHandling::TRIM_WHITESPACE,
           base::SplitResult::SPLIT_WANT_NONEMPTY)) {
    zss.add_suggestions()->set_label(line);
  }
  return zss;
}

}  // namespace

LegionModelExecutionFetcher::LegionModelExecutionFetcher(
    legion::Client* legion_client)
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
  auto request = ToLegionRequest(feature, request_metadata);

  legion::Client::RequestOptions options;
  if (timeout) {
    options.timeout = *timeout;
  }

  legion_client_->SendTextRequest(
      legion_feature_name, request,
      base::BindOnce(
          [](ModelBasedCapabilityKey feature,
             ModelExecuteResponseCallback callback,
             base::expected<std::string, legion::ErrorCode> result) {
            if (!result.has_value()) {
              std::move(callback).Run(
                  base::unexpected(ToModelExecutionError(result.error())));
              return;
            }
            proto::ExecuteResponse response;
            auto* metadata = response.mutable_response_metadata();
            switch (feature) {
              case ModelBasedCapabilityKey::kZeroStateSuggestions: {
                *metadata = AnyWrapProto(ToZSSResponse(result.value()));
                break;
              }
              default:
                NOTREACHED() << feature;
            }
            std::move(callback).Run(base::ok(response));
          },
          feature, std::move(callback)),
      options);
}

}  // namespace optimization_guide
