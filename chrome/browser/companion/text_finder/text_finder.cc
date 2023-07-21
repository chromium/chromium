// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/text_finder/text_finder.h"

#include <string>

#include "third_party/blink/public/mojom/annotation/annotation.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace companion {

TextFinder::TextFinder(
    const std::string& text_directive,
    mojo::Remote<blink::mojom::AnnotationAgentContainer>& agent_container,
    FinishedCallback callback,
    AgentDisconnectHandler agent_disconnect_handler)
    : text_directive_(text_directive), receiver_(this) {
  DCHECK(!text_directive.empty());
  InitializeAndBindToAnnotationAgent(agent_container, std::move(callback));
  SetAgentDisconnectHandler(std::move(agent_disconnect_handler));
}

TextFinder::TextFinder(const std::string& text_directive)
    : text_directive_(text_directive), receiver_(this) {}

TextFinder::~TextFinder() = default;

void TextFinder::InitializeAndBindToAnnotationAgent(
    mojo::Remote<blink::mojom::AnnotationAgentContainer>& agent_container,
    FinishedCallback callback) {
  CHECK(!agent_.is_bound());

  SetDidFinishHandler(std::move(callback));

  // Create an annotation agent for text finder, and bind to it.
  agent_container->CreateAgent(
      receiver_.BindNewPipeAndPassRemote(), agent_.BindNewPipeAndPassReceiver(),
      blink::mojom::AnnotationType::kTextFinder, text_directive_);
}

void TextFinder::SetAgentDisconnectHandler(AgentDisconnectHandler handler) {
  agent_.set_disconnect_handler(std::move(handler));
}

void TextFinder::DidFinishAttachment(const gfx::Rect& rect) {
  is_found_ = !rect.IsEmpty();

  if (did_finish_callback_) {
    std::move(did_finish_callback_)
        .Run(std::make_pair(text_directive_, is_found_));
  }

  // Close the mojo binding to remove the annotation agent in the renderer
  // process (via its disconnect handler), which also removes the visual
  // highlight effect.
  agent_.reset();
}

void TextFinder::SetDidFinishHandler(FinishedCallback callback) {
  did_finish_callback_ = std::move(callback);
}

}  // namespace companion
