// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_BOOKMARK_MANAGEMENT_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_BOOKMARK_MANAGEMENT_TOOL_REQUEST_H_

#include <optional>
#include <string>
#include <string_view>

#include "chrome/browser/actor/tools/tool_request.h"
#include "url/gurl.h"

namespace actor {

class ToolRequestVisitorFunctor;

// Request to add a bookmark for the specified URL and title.
class AddBookmarkToolRequest : public ToolRequest {
 public:
  static constexpr char kName[] = "AddBookmark";
  static constexpr std::string_view kModelFacingName = "add_bookmark";
  static constexpr std::string_view kUrlParam = "url";
  static constexpr std::string_view kTitleParam = "title";

  AddBookmarkToolRequest(GURL url, std::u16string title);
  ~AddBookmarkToolRequest() override;

  // Returns the `ToolId::kAddBookmark` tool schema definition.
  static std::optional<ToolDefinition> GetToolDefinition();
  AddBookmarkToolRequest(const AddBookmarkToolRequest& other);
  AddBookmarkToolRequest& operator=(const AddBookmarkToolRequest& other);

  // ToolRequest:
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;
  std::string_view Name() const override;

  const GURL& url() const { return url_; }
  const std::u16string& title() const { return title_; }

 private:
  GURL url_;
  std::u16string title_;
};

// Request to remove bookmark(s) matching the specified URL.
class RemoveBookmarkToolRequest : public ToolRequest {
 public:
  static constexpr char kName[] = "RemoveBookmark";
  static constexpr std::string_view kModelFacingName = "remove_bookmark";
  static constexpr std::string_view kUrlParam = "url";

  explicit RemoveBookmarkToolRequest(GURL url);
  ~RemoveBookmarkToolRequest() override;

  // Returns the `ToolId::kRemoveBookmark` tool schema definition.
  static std::optional<ToolDefinition> GetToolDefinition();
  RemoveBookmarkToolRequest(const RemoveBookmarkToolRequest& other);
  RemoveBookmarkToolRequest& operator=(const RemoveBookmarkToolRequest& other);

  // ToolRequest:
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;
  std::string_view Name() const override;

  const GURL& url() const { return url_; }

 private:
  GURL url_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_BOOKMARK_MANAGEMENT_TOOL_REQUEST_H_
