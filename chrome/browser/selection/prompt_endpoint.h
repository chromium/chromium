// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SELECTION_PROMPT_ENDPOINT_H_
#define CHROME_BROWSER_SELECTION_PROMPT_ENDPOINT_H_

#include <string>

#include "chrome/browser/selection/mojom/action.mojom-shared.h"
#include "chrome/browser/selection/page_context.h"

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace selection {

// Interface implemented by prompt submission destinations for Smart Selection
// (e.g. Gemini in Chrome, Google Search, and future AI endpoints).
class PromptEndpoint {
 public:
  virtual ~PromptEndpoint() = default;

  // Returns the stable identifier for this endpoint.
  virtual mojom::PromptEndpointId GetId() const = 0;

  // Returns whether this endpoint is currently eligible and available for
  // `tab`.
  virtual bool IsAvailable(tabs::TabInterface& tab) const = 0;

  // Returns the localized display name for this endpoint (e.g. "Gemini",
  // "Google Search").
  virtual std::u16string GetDisplayName() const = 0;

  // Returns the localized placeholder text for the prompt input (e.g.
  // "Ask Gemini", "Ask Google").
  virtual std::u16string GetPlaceholder() const = 0;

  // Optional hook invoked when a selection session starts. Implementations may
  // warm resources or sync cookies, without uploading page content.
  virtual void Prepare(tabs::TabInterface& tab);

  // Cancels any pending asynchronous submission work when the tab navigates,
  // discards contents, or detaches.
  virtual void CancelPendingRequests();

  // Submits `prompt` with `context` to this endpoint's destination. When
  // `prompt` is empty, invokes the endpoint without auto-submitting text (e.g.
  // opening the side panel or performing a visual-only search).
  virtual void Submit(tabs::TabInterface& tab,
                      std::string prompt,
                      PageContextWithSelection context) = 0;
};

}  // namespace selection

#endif  // CHROME_BROWSER_SELECTION_PROMPT_ENDPOINT_H_
