// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/file_upload_tool_request.h"

#include <ostream>
#include <utility>

#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/tools/file_upload_tool.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "net/base/url_util.h"

namespace actor {

FileUploadSource::FileUploadSource(GURL url, std::string file_name)
    : url(std::move(url)), file_name(std::move(file_name)) {}

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
  ActorSurface* const actor_surface = GetActorSurfaceHandle().Get();
  if (!actor_surface) {
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
    switch (file.type) {
      case FileUploadSource::Type::kUrl:
        // Only URLs the browser can fetch over the network are accepted;
        // notably this excludes file:// and other local schemes.
        if (!file.url.is_valid() || !file.url.SchemeIsHTTPOrHTTPS() ||
            net::IsLocalhost(file.url)) {
          return CreateToolResult(
              nullptr,
              MakeResult(mojom::ActionResultCode::kFileUploadUnauthorizedFile,
                         /*requires_page_stabilization=*/false,
                         "Invalid or unauthorized file URL"));
        }
        break;
    }
  }

  return CreateToolResult(
      std::make_unique<FileUploadTool>(task_id, tool_delegate, *actor_surface,
                                       target_, files_),
      MakeOkResult(/*requires_page_stabilization=*/false));
}

std::ostream& operator<<(std::ostream& out,
                         const FileUploadSource& file_source) {
  switch (file_source.type) {
    case FileUploadSource::Type::kUrl:
      // Only the origin is logged; the full URL may contain sensitive data
      // such as capability tokens in the path or query.
      out << "Url(" << file_source.url.DeprecatedGetOriginAsURL().spec()
          << "<redacted>)";
      break;
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
