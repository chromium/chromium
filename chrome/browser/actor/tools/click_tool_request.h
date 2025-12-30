// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_CLICK_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_CLICK_TOOL_REQUEST_H_

#include <memory>
#include <string>

#include "chrome/browser/actor/shared_types.h"
#include "chrome/browser/actor/tools/page_tool_request.h"
#include "chrome/common/actor.mojom-forward.h"

namespace actor {
class ToolRequestVisitorFunctor;

class ClickToolRequest : public PageToolRequest {
 public:
  static constexpr char kName[] = "Click";

  ClickToolRequest(tabs::TabHandle tab_handle,
                   const PageTarget& target,
                   MouseClickType type,
                   MouseClickCount count);
  ~ClickToolRequest() override;

  void Apply(ToolRequestVisitorFunctor& f) const override;

  MouseClickType GetClickType() const { return click_type_; }
  MouseClickCount GetClickCount() const { return click_count_; }

  // ToolRequest
  std::string_view Name() const override;
  ObservationDelayController::PageStabilityConfig
  GetObservationPageStabilityConfig() const override;

  // PageToolRequest
  mojom::ToolActionPtr ToMojoToolAction(
      content::RenderFrameHost& frame) const override;
  std::unique_ptr<PageToolRequest> Clone() const override;

  void WillSendToRenderer(
      content::RenderWidgetHost* render_widget_host) override;

 private:
  MouseClickType click_type_;
  MouseClickCount click_count_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_CLICK_TOOL_REQUEST_H_
