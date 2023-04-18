// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPANION_TEXT_FINDER_TEXT_FINDER_H_
#define CHROME_BROWSER_COMPANION_TEXT_FINDER_TEXT_FINDER_H_

#include <string>

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace companion {

// Class for initiating text search via the annotation agent in the renderer
// process. Note that the current implementation relies on the existing
// annotation agent implementation:
//  `third_party/blink/renderer/core/annotation/annotation_agent_impl.h`,
// which will both perform text search and highlight the text.
// To remove the highlight effect, this `TextFinder` immediately resets
// the mojo connection upon finishing searching, to remove the annotation
// agent. In the follow-up, we will implement a new type of annotation agent
// that only performs text search without highlighting text.
// See crbug.com/1430306.
class TextFinder : public blink::mojom::AnnotationAgentHost {
 public:
  // The callback type invoked when text search in the renderer is finished. The
  // input param is a pair of a text string and its bool search result (true if
  // found).
  using FinishedCallback =
      base::OnceCallback<void(std::pair<std::string, bool>)>;
  using AgentDisconnectHandler = base::OnceClosure;

  TextFinder(
      const std::string& text_directive,
      mojo::Remote<blink::mojom::AnnotationAgentContainer>& agent_container,
      FinishedCallback callback,
      AgentDisconnectHandler agent_disconnect_handler);
  // Test only constructor.
  explicit TextFinder(const std::string& text_directive);
  ~TextFinder() override;
  TextFinder(const TextFinder&) = delete;
  TextFinder& operator=(const TextFinder&) = delete;

  // blink::mojom::AnnotationAgentHost implementation.
  // Calls `did_finish_callback_` and then removes the annotation agent in the
  // renderer process to remove the visual highlight effect.
  // If not found, `rect` is empty.
  void DidFinishAttachment(const gfx::Rect& rect) override;

  // Set a callback called upon finishing finding.
  void SetDidFinishHandler(FinishedCallback callback);

  const std::string& GetTextDirective() const { return text_directive_; }

 private:
  // Creates an annotation agent for text search (and highlight), and sets up
  // the Mojo connection with the renderer process. The annotation agent
  // performs the search and calls `DidFinishAttachment`.
  void InitializeAndBindToAnnotationAgent(
      mojo::Remote<blink::mojom::AnnotationAgentContainer>& agent_container,
      FinishedCallback callback);

  // The handler is invoked when the agent receiver in the renderer process
  // closes the pipe.
  void SetAgentDisconnectHandler(AgentDisconnectHandler handler);

  // Text directive to search for, in the format of "prefix-,start,end,suffix-".
  // Ref: https://wicg.github.io/scroll-to-text-fragment/#syntax.
  const std::string text_directive_;
  bool is_found_ = false;

  // Callback to invoke after the renderer agent has initialized.
  FinishedCallback did_finish_callback_;

  // Receiver and agent for communication with the renderer process.
  mojo::Receiver<blink::mojom::AnnotationAgentHost> receiver_;
  mojo::Remote<blink::mojom::AnnotationAgent> agent_;
};

}  // namespace companion

#endif  // CHROME_BROWSER_COMPANION_TEXT_FINDER_TEXT_FINDER_H_
