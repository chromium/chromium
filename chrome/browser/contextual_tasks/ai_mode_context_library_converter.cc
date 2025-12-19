// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/ai_mode_context_library_converter.h"

#include <vector>

#include "base/uuid.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "url/gurl.h"

namespace {
const contextual_search::FileInfo* GetFileInfoFromContext(
    int64_t context_id,
    const std::vector<contextual_search::FileInfo>& contexts) {
  for (auto& file_info : contexts) {
    // TODO(nyquist): Remove this cast when we roll in the new request ID proto.
    if (static_cast<int64_t>(file_info.GetContextId()) == context_id) {
      return &file_info;
    }
  }
  return nullptr;
}
}  // namespace

namespace contextual_tasks {

std::vector<UrlResource> ConvertAiModeContextToUrlResources(
    const lens::UpdateThreadContextLibrary& message,
    const std::vector<contextual_search::FileInfo>& local_contexts) {
  std::vector<UrlResource> result;
  // Iterate through the contexts in the message and attempt to find matching
  // local file info (e.g. tab URL) to build the UrlResource list.
  for (const auto& context : message.contexts()) {
    if (context.has_webpage()) {
      UrlResource url_resource(GURL(context.webpage().url()));
      url_resource.context_id = context.context_id();
      url_resource.title = context.webpage().title();

      const contextual_search::FileInfo* file_info =
          GetFileInfoFromContext(context.context_id(), local_contexts);
      if (file_info) {
        if (url_resource.url.is_empty() && file_info->tab_url.has_value() &&
            file_info->tab_url.value().is_valid()) {
          url_resource.url = *file_info->tab_url;
        }
        if (!url_resource.tab_id.has_value()) {
          url_resource.tab_id = file_info->tab_session_id;
        }
        if (!url_resource.title.has_value()) {
          url_resource.title = file_info->tab_title;
        }
      }
      result.push_back(url_resource);
    } else if (context.has_pdf()) {
      // TODO(nyquist): Add special handling for PDFs.
      UrlResource url_resource(GURL(context.pdf().url()));
      url_resource.context_id = context.context_id();
      url_resource.title = context.pdf().title();
      result.push_back(url_resource);
    } else if (context.has_image()) {
      // TODO(nyquist): Add special handling for images.
      UrlResource url_resource(GURL(context.image().url()));
      url_resource.context_id = context.context_id();
      url_resource.title = context.image().title();
      result.push_back(url_resource);
    } else {
      // Unknown context type. This client does not support representing it.
      UrlResource url_resource(GURL::EmptyGURL());
      url_resource.context_id = context.context_id();
      result.push_back(url_resource);
    }
  }
  return result;
}

}  // namespace contextual_tasks
