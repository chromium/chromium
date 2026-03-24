// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_invoke_handler.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"

namespace glic {

GlicInvokeHandler::GlicInvokeHandler(GlicInstanceImpl& instance,
                                     GlicInvokeOptions options,
                                     CompletionCallback completion_callback)
    : instance_(instance),
      options_(std::move(options)),
      completion_callback_(std::move(completion_callback)) {}

GlicInvokeHandler::~GlicInvokeHandler() = default;

void GlicInvokeHandler::Invoke() {
  // TODO(crbug.com/483387751): Add readiness delay and timeout handling.
  // For now, fail immediately if not ready.
  SendToClient();
}

void GlicInvokeHandler::SendToClient() {
  if (!instance_->host().IsReady()) {
    OnError(GlicInvokeError::kTimeout);
    return;
  }

  instance_->host().Invoke(CreateMojoOptions(),
                           base::BindOnce(&GlicInvokeHandler::OnSuccess,
                                          weak_ptr_factory_.GetWeakPtr()));
}

void GlicInvokeHandler::OnSuccess() {
  if (options_.on_success) {
    std::move(options_.on_success).Run();
  }
  if (completion_callback_) {
    std::move(completion_callback_).Run(&*instance_, this);
  }
}

void GlicInvokeHandler::OnError(GlicInvokeError error) {
  if (options_.on_error) {
    std::move(options_.on_error).Run(error);
  }
  if (completion_callback_) {
    std::move(completion_callback_).Run(&*instance_, this);
  }
}

mojom::InvokeOptionsPtr GlicInvokeHandler::CreateMojoOptions() {
  auto mojo_options = mojom::InvokeOptions::New();
  mojo_options->invocation_source = options_.invocation_source;

  if (!options_.prompts.empty()) {
    mojo_options->prompts = options_.prompts;
  }

  if (options_.additional_context) {
    mojo_options->context = std::move(options_.additional_context);
  }

  mojo_options->auto_submit = options_.auto_submit;
  mojo_options->feature_mode =
      options_.feature_mode.value_or(mojom::FeatureMode::kUnspecified);
  mojo_options->disable_zero_state_suggestions = options_.disable_zss;

  if (options_.skill_id) {
    mojo_options->skill_id = *options_.skill_id;
  }

  return mojo_options;
}

}  // namespace glic
