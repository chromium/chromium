// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_SCROLL_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_SCROLL_TOOL_REQUEST_H_

#include <memory>
#include <string>

#include "chrome/browser/actor/tools/page_tool_request.h"
#include "chrome/common/actor.mojom-forward.h"

namespace actor {
class ToolRequestVisitorFunctor;

// Scrolls an element or viewport in the page a given distance.
class ScrollToolRequest : public PageToolRequest {
 public:
  static constexpr char kName[] = "Scroll";

  enum class Direction { kLeft, kRight, kUp, kDown };

  // Programmatically scrolls the scroller specified by target a given distance.
  // If Target is a nullopt ContentNodeId, the root viewport is scrolled.
  // Distance is specified in DIPs.
  ScrollToolRequest(tabs::TabHandle tab_handle,
                    const PageTarget& target,
                    Direction direction,
                    float distance);
  ~ScrollToolRequest() override;

  void Apply(ToolRequestVisitorFunctor& f) const override;

  // ToolRequest
  std::string_view Name() const override;

  // PageToolRequest
  mojom::ToolActionPtr ToMojoToolAction(
      content::RenderFrameHost& frame) const override;
  std::unique_ptr<PageToolRequest> Clone() const override;

 private:
  Direction direction_;
  float distance_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_SCROLL_TOOL_REQUEST_H_
