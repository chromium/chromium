// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/model_execution/model_execution_session.h"

#include <optional>

#include "base/functional/bind.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "mojo/public/cpp/bindings/remote_set.h"

ModelExecutionSession::ModelExecutionSession(
    std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
        session)
    : session_(std::move(session)) {}

ModelExecutionSession::~ModelExecutionSession() = default;

void ModelExecutionSession::BindReceiver(
    mojo::PendingReceiver<blink::mojom::ModelGenericSession> receiver) {
  receiver_.Bind(std::move(receiver));
}

void ModelExecutionSession::ModelExecutionCallback(
    mojo::RemoteSetElementId responder_id,
    optimization_guide::OptimizationGuideModelStreamingExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  blink::mojom::ModelStreamingResponder* responder =
      responder_set_.Get(responder_id);
  if (!responder) {
    return;
  }

  if (!result.has_value()) {
    responder->OnResponse(blink::mojom::ModelStreamingResponseStatus::kError,
                          std::nullopt);
    return;
  }

  auto response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::StringValue>(result->response);
  if (response->has_value()) {
    responder->OnResponse(blink::mojom::ModelStreamingResponseStatus::kOngoing,
                          response->value());
  }
  if (result->is_complete) {
    responder->OnResponse(blink::mojom::ModelStreamingResponseStatus::kComplete,
                          std::nullopt);
  }
}

void ModelExecutionSession::Execute(
    const std::string& input,
    mojo::PendingRemote<blink::mojom::ModelStreamingResponder> responder) {
  mojo::RemoteSetElementId responder_id =
      responder_set_.Add(std::move(responder));
  optimization_guide::proto::StringValue request;
  request.set_value(input);
  session_->ExecuteModel(
      request,
      base::BindRepeating(&ModelExecutionSession::ModelExecutionCallback,
                          weak_ptr_factory_.GetWeakPtr(), responder_id));
}
