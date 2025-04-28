// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/text_finder/text_highlighter.h"

#include <string>

#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace companion {
namespace internal {

TextHighlighter::TextHighlighter(
    const std::string& text_directive,
    const mojo::Remote<blink::mojom::AnnotationAgentContainer>& agent_container)
    : text_directive_(text_directive), receiver_(this) {
  DCHECK(!text_directive.empty());
  InitializeAndBindToAnnotationAgent(agent_container);
}

TextHighlighter::~TextHighlighter() = default;

void TextHighlighter::InitializeAndBindToAnnotationAgent(
    const mojo::Remote<blink::mojom::AnnotationAgentContainer>&
        agent_container) {
  // Create an annotation agent and bind to it.
  agent_container->CreateAgent(
      receiver_.BindNewPipeAndPassRemote(), agent_.BindNewPipeAndPassReceiver(),
      blink::mojom::AnnotationType::kSharedHighlight,
      blink::mojom::Selector::NewSerializedSelector(text_directive_),
      /*search_range_start_node_id=*/std::nullopt);
}

void TextHighlighter::DidFinishAttachment(
    const gfx::Rect& rect,
    blink::mojom::AttachmentResult attachment_result) {
  base::UmaHistogramBoolean("Companion.CQ.TextHighlight.Success",
                            !rect.IsEmpty());

  if (attachment_result != blink::mojom::AttachmentResult::kSuccess) {
    return;
  }

  // Call the mojo method to scroll to the highlighted text.
  agent_->ScrollIntoView(/*applies_focus=*/true);
}

}  // namespace internal
}  // namespace companion
