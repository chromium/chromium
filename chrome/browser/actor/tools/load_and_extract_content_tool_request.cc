// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/load_and_extract_content_tool_request.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/strings/string_util.h"
#include "chrome/browser/actor/tools/load_and_extract_content_tool.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor/action_result.h"
#include "url/gurl.h"

namespace actor {

LoadAndExtractContentToolRequest::LoadAndExtractContentToolRequest(
    std::vector<GURL> urls)
    : urls_(std::move(urls)) {}

LoadAndExtractContentToolRequest::~LoadAndExtractContentToolRequest() = default;

LoadAndExtractContentToolRequest::LoadAndExtractContentToolRequest(
    const LoadAndExtractContentToolRequest&) = default;
LoadAndExtractContentToolRequest& LoadAndExtractContentToolRequest::operator=(
    const LoadAndExtractContentToolRequest&) = default;

ToolRequest::CreateToolResult LoadAndExtractContentToolRequest::CreateTool(
    TaskId task_id,
    actor::ToolDelegate& tool_delegate) const {
  // TODO(b/443954134): Could we avoid the copy of the URLs?
  return {std::make_unique<LoadAndExtractContentTool>(task_id, tool_delegate,
                                                      urls_),
          MakeOkResult()};
}

void LoadAndExtractContentToolRequest::Apply(
    ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view LoadAndExtractContentToolRequest::Name() const {
  return kName;
}

}  // namespace actor
