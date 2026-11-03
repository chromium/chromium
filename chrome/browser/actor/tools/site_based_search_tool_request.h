// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_SITE_BASED_SEARCH_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_SITE_BASED_SEARCH_TOOL_REQUEST_H_

#include <optional>
#include <string>
#include <string_view>

#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/tool_request.h"

namespace actor {

class ToolRequestVisitorFunctor;

// Searches within the currently active website using its OpenSearch Description
// Document (OSDD) template or a discoverable DOM search input.
class SiteBasedSearchToolRequest : public TabToolRequest {
 public:
  static constexpr char kName[] = "SiteBasedSearch";

  SiteBasedSearchToolRequest(ActorSurfaceHandle actor_surface_handle,
                             std::string query,
                             std::string document_identifier);

  // JSON argument key for the search query string parameter.
  static constexpr std::string_view kQueryParam = "query";

  ~SiteBasedSearchToolRequest() override;

  SiteBasedSearchToolRequest(const SiteBasedSearchToolRequest&);
  SiteBasedSearchToolRequest& operator=(const SiteBasedSearchToolRequest&);

  // Model-facing tool name for registry lookup.
  static constexpr std::string_view kModelFacingName = "site_based_search";

  // Returns the `ToolId::kSiteBasedSearch` tool schema definition.
  static std::optional<ToolDefinition> GetToolDefinition();

  // ToolRequest:
  bool RequiresUrlCheckInCurrentTab() const override;
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;
  std::string_view Name() const override;

  const std::string& query() const { return query_; }
  const std::string& document_identifier() const {
    return document_identifier_;
  }

 private:
  std::string query_;
  std::string document_identifier_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_SITE_BASED_SEARCH_TOOL_REQUEST_H_
