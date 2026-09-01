// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/file_upload_tool_request.h"

#include <ostream>
#include <utility>

#include "chrome/browser/actor/tools/file_upload_tool.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"

namespace actor {

FileUploadToolRequest::FileUploadToolRequest(
    tabs::TabHandle tab_handle,
    PageTarget target,
    std::vector<FileUploadSource> files)
    : TabToolRequest(tab_handle),
      target_(std::move(target)),
      files_(std::move(files)) {}

FileUploadToolRequest::FileUploadToolRequest(const FileUploadToolRequest&) =
    default;

FileUploadToolRequest& FileUploadToolRequest::operator=(
    const FileUploadToolRequest&) = default;

FileUploadToolRequest::~FileUploadToolRequest() = default;

std::string_view FileUploadToolRequest::Name() const {
  return kName;
}

void FileUploadToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

ToolRequest::CreateToolResult FileUploadToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  tabs::TabInterface* const tab = GetTabHandle().Get();
  if (!tab) {
    return CreateToolResult(
        nullptr,
        MakeResult(mojom::ActionResultCode::kTabWentAway,
                   /*requires_page_stabilization=*/false, "Tab went away"));
  }

  if (files_.empty()) {
    return CreateToolResult(
        nullptr, MakeResult(mojom::ActionResultCode::kFileUploadEmptyFileList,
                            /*requires_page_stabilization=*/false,
                            "No files provided for file upload"));
  }

  for (const auto& file : files_) {
    if (file.type == FileUploadSource::Type::kLocalPath) {
      if (file.local_path.empty() || !file.local_path.IsAbsolute() ||
          file.local_path.ReferencesParent()) {
        return CreateToolResult(
            nullptr,
            MakeResult(mojom::ActionResultCode::kFileUploadUnauthorizedFile,
                       /*requires_page_stabilization=*/false,
                       "Invalid or unauthorized local file path"));
      }
    }
  }

  return CreateToolResult(std::make_unique<FileUploadTool>(
                              task_id, tool_delegate, *tab, target_, files_),
                          nullptr);
}

std::ostream& operator<<(std::ostream& out,
                         const FileUploadSource& file_source) {
  if (file_source.type == FileUploadSource::Type::kLocalPath) {
    out << "LocalPath(<redacted>)";
  }
  return out;
}

std::ostream& operator<<(std::ostream& out,
                         const std::vector<FileUploadSource>& file_sources) {
  out << "[";
  for (size_t i = 0; i < file_sources.size(); ++i) {
    if (i > 0) {
      out << ", ";
    }
    out << file_sources[i];
  }
  out << "]";
  return out;
}

}  // namespace actor
