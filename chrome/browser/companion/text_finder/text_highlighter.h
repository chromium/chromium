// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPANION_TEXT_FINDER_TEXT_HIGHLIGHTER_H_
#define CHROME_BROWSER_COMPANION_TEXT_FINDER_TEXT_HIGHLIGHTER_H_

#include <string>

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace companion {
namespace internal {

// Class for highlighting and scrolling-to texts on the current page via the
// annotation agent in the renderer process.
class TextHighlighter : public blink::mojom::AnnotationAgentHost {
 public:
  TextHighlighter(const std::string& text_directive,
                  const mojo::Remote<blink::mojom::AnnotationAgentContainer>&
                      agent_container);
  ~TextHighlighter() override;
  TextHighlighter(const TextHighlighter&) = delete;
  TextHighlighter& operator=(const TextHighlighter&) = delete;

  // blink::mojom::AnnotationAgentHost implementation.
  // Called when the annotation agent finishes finding the text.
  // Scrolls to the highlighted text via the mojo method of the agent.
  void DidFinishAttachment(const gfx::Rect& rect) override;

  const std::string& GetTextDirective() const { return text_directive_; }

 private:
  // Creates an annotation agent for text search and highlighting, and sets up
  // the Mojo connection with the renderer process. The annotation agent
  // performs the search and calls `DidFinishAttachment`. This method should
  // only be called once during initialization.
  void InitializeAndBindToAnnotationAgent(
      const mojo::Remote<blink::mojom::AnnotationAgentContainer>&
          agent_container);

  // Text directive to search for, in the format of "prefix-,start,end,suffix-".
  // Ref: https://wicg.github.io/scroll-to-text-fragment/#syntax.
  const std::string text_directive_;

  // Receiver and agent for communication with the renderer process.
  mojo::Receiver<blink::mojom::AnnotationAgentHost> receiver_;
  mojo::Remote<blink::mojom::AnnotationAgent> agent_;
};

}  // namespace internal
}  // namespace companion

#endif  // CHROME_BROWSER_COMPANION_TEXT_FINDER_TEXT_HIGHLIGHTER_H_
