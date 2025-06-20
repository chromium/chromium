// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_PAGE_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_PAGE_TOOL_REQUEST_H_

#include <memory>
#include <string>
#include <string_view>
#include <variant>

#include "chrome/browser/actor/tools/tool_request.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/weak_document_ptr.h"
#include "url/gurl.h"

namespace actor {

class AggregatedJournal;

// Tool requests targeting a specific, existing document should inherit from
// this subclass. Being page-scoped implies also being tab-scoped since a page
// exists inside a tab.
//
// Note: A page tool is scoped to a specific (local root) document, however,
// until tool invocation time it isn't valid to dereference the RenderFrameHost
// from the request. This is because the final frame that will be used isn't
// known until the request goes through TimeOfUseValidation and the tool is
// ready to invoke.
class PageToolRequest : public TabToolRequest {
 public:
  // Page tool requests must specify a target in the page. This must be
  // one of (mutually exclusive):
  //   * A main-frame relative coordinate
  //   * A specific node, specified by DOMNodeId and document identifier pair.
  //     DOMNodeId can be the kRootElementDomNodeId special value to target the
  //     viewport.
  using CoordinateTarget = gfx::Point;
  struct NodeTarget {
    int dom_node_id;
    std::string document_identifier;
  };

  class Target {
   public:
    explicit Target(const NodeTarget& node_target);
    explicit Target(const CoordinateTarget& coordinate_target);
    Target(const Target& other);
    ~Target();

    bool is_coordinate() const {
      return std::holds_alternative<CoordinateTarget>(impl_);
    }
    bool is_node() const { return std::holds_alternative<NodeTarget>(impl_); }

    const CoordinateTarget& coordinate() const {
      return std::get<CoordinateTarget>(impl_);
    }
    const NodeTarget& node() const { return std::get<NodeTarget>(impl_); }

   private:
    std::variant<NodeTarget, CoordinateTarget> impl_;
  };

  PageToolRequest(tabs::TabHandle tab_handle, const Target& target);
  ~PageToolRequest() override;
  PageToolRequest(const PageToolRequest& other);

  // Converts this request into the ToolAction mojo message which can be
  // executed in the renderer.
  virtual mojom::ToolActionPtr ToMojoToolAction() const = 0;

  virtual std::unique_ptr<PageToolRequest> Clone() const = 0;

  // ToolRequest
  CreateToolResult CreateTool(TaskId task_id,
                              AggregatedJournal& journal) const override;

  // Returns what in the page the tool should act upon.
  const Target& GetTarget() const;

 protected:
  // Helper usable by child classes when implementing ToMojoToolAction.
  // Constructs an actor::mojom::ToolTarget from a PageToolRequest::Target.
  static mojom::ToolTargetPtr ToMojoToolTarget(const Target& target);

 private:
  std::optional<std::string> document_identifier_;
  Target target_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_PAGE_TOOL_REQUEST_H_
