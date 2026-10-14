// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/bookmark_management_tool_request.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "chrome/browser/actor/tools/bookmark_management_tool.h"
#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_definition_builder.h"
#include "chrome/browser/actor/tools/registry/tool_schema_builder.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "url/gurl.h"

namespace actor {

namespace {

// Default description for the `add_bookmark` tool.
constexpr std::string_view kAddBookmarkToolDescription =
    "Adds a bookmark for the specified URL and title.";

// Description for the `url` parameter of the `add_bookmark` tool.
constexpr std::string_view kAddBookmarkUrlParamDescription =
    "The URL of the webpage to bookmark.";

// Description for the `title` parameter of the `add_bookmark` tool.
constexpr std::string_view kAddBookmarkTitleParamDescription =
    "The title for the bookmark.";

// Default description for the `remove_bookmark` tool.
constexpr std::string_view kRemoveBookmarkToolDescription =
    "Removes bookmarks matching the specified URL.";

// Description for the `url` parameter of the `remove_bookmark` tool.
constexpr std::string_view kRemoveBookmarkUrlParamDescription =
    "The URL of the bookmark to remove.";

}  // namespace

AddBookmarkToolRequest::AddBookmarkToolRequest(GURL url, std::u16string title)
    : url_(std::move(url)), title_(std::move(title)) {}

AddBookmarkToolRequest::~AddBookmarkToolRequest() = default;

// static
std::optional<ToolDefinition> AddBookmarkToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kAddBookmark, kModelFacingName,
                               kAddBookmarkToolDescription)
      .SetToolParameterSchema(
          ToolSchemaBuilder()
              .AddStringProperty(kUrlParam, kAddBookmarkUrlParamDescription,
                                 ToolSchemaBuilder::kFormatUri)
              .AddStringProperty(kTitleParam,
                                 kAddBookmarkTitleParamDescription))
      .Build();
}

AddBookmarkToolRequest::AddBookmarkToolRequest(
    const AddBookmarkToolRequest& other) = default;

AddBookmarkToolRequest& AddBookmarkToolRequest::operator=(
    const AddBookmarkToolRequest& other) = default;

ToolRequest::CreateToolResult AddBookmarkToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  return {/*tool=*/std::make_unique<BookmarkManagementTool>(
              task_id, tool_delegate, BookmarkManagementTool::Action::kAdd,
              url_, title_),
          MakeOkResult()};
}

void AddBookmarkToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view AddBookmarkToolRequest::Name() const {
  return kName;
}

RemoveBookmarkToolRequest::RemoveBookmarkToolRequest(GURL url)
    : url_(std::move(url)) {}

RemoveBookmarkToolRequest::~RemoveBookmarkToolRequest() = default;

// static
std::optional<ToolDefinition> RemoveBookmarkToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kRemoveBookmark, kModelFacingName,
                               kRemoveBookmarkToolDescription)
      .SetToolParameterSchema(ToolSchemaBuilder().AddStringProperty(
          kUrlParam, kRemoveBookmarkUrlParamDescription,
          ToolSchemaBuilder::kFormatUri))
      .Build();
}

RemoveBookmarkToolRequest::RemoveBookmarkToolRequest(
    const RemoveBookmarkToolRequest& other) = default;

RemoveBookmarkToolRequest& RemoveBookmarkToolRequest::operator=(
    const RemoveBookmarkToolRequest& other) = default;

ToolRequest::CreateToolResult RemoveBookmarkToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  return {/*tool=*/std::make_unique<BookmarkManagementTool>(
              task_id, tool_delegate, BookmarkManagementTool::Action::kRemove,
              url_),
          MakeOkResult()};
}

void RemoveBookmarkToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view RemoveBookmarkToolRequest::Name() const {
  return kName;
}

}  // namespace actor
