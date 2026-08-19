// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/bookmark_management_tool_request.h"

#include <string>
#include <string_view>
#include <utility>

#include "base/notimplemented.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "url/gurl.h"

namespace actor {

AddBookmarkToolRequest::AddBookmarkToolRequest(GURL url,
                                               std::u16string title)
    : url_(std::move(url)), title_(std::move(title)) {}

AddBookmarkToolRequest::~AddBookmarkToolRequest() = default;

AddBookmarkToolRequest::AddBookmarkToolRequest(
    const AddBookmarkToolRequest& other) = default;

AddBookmarkToolRequest& AddBookmarkToolRequest::operator=(
    const AddBookmarkToolRequest& other) = default;

ToolRequest::CreateToolResult AddBookmarkToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  NOTIMPLEMENTED();
  return {/*tool=*/nullptr,
          MakeResult(mojom::ActionResultCode::kToolUnknown,
                     /*requires_page_stabilization=*/false,
                     "AddBookmarkTool is not yet implemented.")};
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

RemoveBookmarkToolRequest::RemoveBookmarkToolRequest(
    const RemoveBookmarkToolRequest& other) = default;

RemoveBookmarkToolRequest& RemoveBookmarkToolRequest::operator=(
    const RemoveBookmarkToolRequest& other) = default;

ToolRequest::CreateToolResult RemoveBookmarkToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  NOTIMPLEMENTED();
  return {/*tool=*/nullptr,
          MakeResult(mojom::ActionResultCode::kToolUnknown,
                     /*requires_page_stabilization=*/false,
                     "RemoveBookmarkTool is not yet implemented.")};
}

void RemoveBookmarkToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view RemoveBookmarkToolRequest::Name() const {
  return kName;
}

}  // namespace actor
