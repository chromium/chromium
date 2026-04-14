// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_invoke_options.h"

namespace glic {

ConversationId::ConversationId(std::string conversation_id)
    : conversation_id(std::move(conversation_id)) {}
ConversationId::ConversationId(std::string conversation_id,
                               std::optional<std::string> turn_id)
    : conversation_id(std::move(conversation_id)),
      turn_id(std::move(turn_id)) {}
ConversationId::~ConversationId() = default;
ConversationId::ConversationId(const ConversationId&) = default;
ConversationId& ConversationId::operator=(const ConversationId&) = default;

GlicInvokeOptions::GlicInvokeOptions(
    glic::mojom::InvocationSource invocation_source)
    : invocation_source(invocation_source) {}

GlicInvokeOptions::~GlicInvokeOptions() = default;

GlicInvokeOptions::GlicInvokeOptions(GlicInvokeOptions&&) = default;

GlicInvokeOptions& GlicInvokeOptions::operator=(GlicInvokeOptions&&) = default;

}  // namespace glic
