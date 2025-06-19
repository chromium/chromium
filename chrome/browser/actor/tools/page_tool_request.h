// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_PAGE_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_PAGE_TOOL_REQUEST_H_

#include <memory>
#include <string>
#include <string_view>

#include "chrome/browser/actor/tools/tool_request.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/weak_document_ptr.h"
#include "url/gurl.h"

namespace actor {

class AggregatedJournal;

// Tool requests targeting a specific, existing document should inherit from
// this subclass. Being page-scoped implies also being tab-scoped since a page
// exists inside a tab.
class PageToolRequest : public TabToolRequest {
 public:
  // Page tool requests must specify a target within the document. This must be
  // one of:
  //   * A coordinate, relative to the local root origin
  //   * A specific node, specified by DOMNodeId. If not set, targets the root
  //     element / viewport.
  using NodeTarget = std::optional<int>;
  using CoordinateTarget = gfx::Point;
  using Target = std::variant<NodeTarget, CoordinateTarget>;

  // A document identifier is optional if a CoordinateTarget. It is required
  // when using a NodeTarget.
  // TODO(crbug.com/411462297): Put document identifier into the Target type.
  PageToolRequest(tabs::TabHandle tab_handle,
                  std::string_view document_identifier,
                  const Target& target);
  ~PageToolRequest() override;
  PageToolRequest(const PageToolRequest& other);

  // Converts this request into the ToolAction mojo message which can be
  // executed in the renderer.
  virtual mojom::ToolActionPtr ToMojoToolAction() const = 0;

  virtual std::unique_ptr<PageToolRequest> Clone() const = 0;

  // ToolRequest
  CreateToolResult CreateTool(AggregatedJournal& journal) const override;

  // The provided document identifier in which this request should act. nullopt
  // if using coordinates in which case the target document must be hit tested
  // from coordinates.
  const std::optional<std::string>& DocumentIdentifier() const;

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
