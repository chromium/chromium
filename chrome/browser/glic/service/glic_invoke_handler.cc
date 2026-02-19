// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_invoke_handler.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"

namespace glic {

GlicInvokeHandler::GlicInvokeHandler(
    GlicInstanceImpl& instance,
    GlicInvokeOptions options,
    InvokeCompleteCallback invoke_complete_callback)
    : instance_(instance),
      options_(std::move(options)),
      invoke_complete_callback_(std::move(invoke_complete_callback)) {
  // TODO(crbug.com/483387751): Add polling logic and timeout handling.
  SendToClient();
}

GlicInvokeHandler::~GlicInvokeHandler() = default;

void GlicInvokeHandler::SendToClient() {
  base::OnceClosure callback =
      options_.on_success ? std::move(options_.on_success) : base::DoNothing();

  instance_->host().Invoke(
      CreateMojoOptions(),
      base::BindOnce(&GlicInvokeHandler::OnSendToClientComplete,
                     base::Unretained(this), std::move(callback)));
}

void GlicInvokeHandler::OnSendToClientComplete(base::OnceClosure callback) {
  std::move(callback).Run();
  if (invoke_complete_callback_) {
    std::move(invoke_complete_callback_).Run(instance_->id(), this);
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
