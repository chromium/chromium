// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_SCROLL_TO_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_SCROLL_TO_TOOL_REQUEST_H_

#include <memory>
#include <string>

#include "chrome/browser/actor/tools/page_tool_request.h"
#include "chrome/common/actor.mojom-forward.h"

namespace actor {
class ToolRequestVisitorFunctor;

// Scrolls to the given target.
class ScrollToToolRequest : public PageToolRequest {
 public:
  static constexpr char kName[] = "ScrollTo";

  ScrollToToolRequest(tabs::TabHandle tab_handle, const PageTarget& target);
  ~ScrollToToolRequest() override;

  void Apply(ToolRequestVisitorFunctor& f) const override;

  // ToolRequest
  std::string_view Name() const override;

  // PageToolRequest
  mojom::ToolActionPtr ToMojoToolAction(
      content::RenderFrameHost& frame) const override;
  std::unique_ptr<PageToolRequest> Clone() const override;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_SCROLL_TO_TOOL_REQUEST_H_
