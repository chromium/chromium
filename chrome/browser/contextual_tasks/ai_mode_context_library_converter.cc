// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/ai_mode_context_library_converter.h"

#include <vector>

#include "base/time/time.h"
#include "base/uuid.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "third_party/lens_server_proto/search_communication.pb.h"
#include "url/gurl.h"

namespace {
const contextual_search::FileInfo* GetFileInfoFromContext(
    int64_t context_id,
    const std::vector<contextual_search::FileInfo>& contexts) {
  for (auto& file_info : contexts) {
    // TODO(nyquist): Remove this cast when we roll in the new request ID proto.
    if (file_info.GetContextId().has_value() &&
        static_cast<int64_t>(file_info.GetContextId().value()) == context_id) {
      return &file_info;
    }
  }
  return nullptr;
}

// Shared implementation for lens::UpdateThreadContextLibrary (AIM protocol) and
// lens::SearchToClientMessage::UpdateThreadContextLibrary (Search in Chrome
// protocol), which have identical field layouts.
template <typename UpdateThreadContextLibraryT>
std::vector<contextual_tasks::UrlResource> ConvertImpl(
    const UpdateThreadContextLibraryT& message,
    const std::vector<contextual_search::FileInfo>& local_contexts) {
  using contextual_tasks::ResourceType;
  using contextual_tasks::UrlResource;

  std::vector<UrlResource> result;
  // Iterate through the contexts in the message and attempt to find matching
  // local file info (e.g. tab URL) to build the UrlResource list.
  for (const auto& context : message.contexts()) {
    std::optional<UrlResource> url_resource;
    if (context.has_webpage()) {
      url_resource.emplace(GURL(context.webpage().url()),
                           ResourceType::kWebpage);
      url_resource->context_id = context.context_id();
      url_resource->title = context.webpage().title();
    } else if (context.has_pdf()) {
      url_resource.emplace(GURL(context.pdf().url()), ResourceType::kPdf);
      url_resource->context_id = context.context_id();
      url_resource->title = context.pdf().title();
    } else if (context.has_image()) {
      url_resource.emplace(GURL(context.image().url()), ResourceType::kImage);
      url_resource->context_id = context.context_id();
      url_resource->title = context.image().title();
    } else {
      // Unknown context type. This client does not support representing it.
      url_resource.emplace(GURL::EmptyGURL(), ResourceType::kUnknown);
      url_resource->context_id = context.context_id();
    }

    url_resource->has_chrome_tab_data = context.has_chrome_tab_data();

    if (url_resource) {
      const contextual_search::FileInfo* file_info =
          GetFileInfoFromContext(context.context_id(), local_contexts);
      if (file_info) {
        // Tab-derived inputs (e.g. Lens overlay selections) may be returned as
        // an Image by the server. Preserve the underlying tab's webpage URL
        // and type so tab strip underlines and restored tab state stay active.
        if (file_info->tab_url.has_value() &&
            file_info->tab_url.value().is_valid() &&
            (url_resource->url.is_empty() ||
             file_info->tab_session_id.has_value())) {
          url_resource->url = *file_info->tab_url;
        }
        if (!url_resource->tab_id.has_value()) {
          url_resource->tab_id = file_info->tab_session_id;
        }
        if (file_info->tab_session_id.has_value() &&
            (file_info->mime_type == lens::MimeType::kAnnotatedPageContent ||
             file_info->mime_type == lens::MimeType::kHtml)) {
          url_resource->resource_type = ResourceType::kWebpage;
        }
        if (file_info->tab_title.has_value() &&
            (!url_resource->title.has_value() ||
             file_info->tab_session_id.has_value())) {
          url_resource->title = file_info->tab_title;
        }
        if (file_info->request_id.has_value() &&
            file_info->request_id->has_time_usec()) {
          url_resource->timestamp =
              base::Time::UnixEpoch() +
              base::Microseconds(file_info->request_id->time_usec());
        }
      }
      result.push_back(*url_resource);
    }
  }
  return result;
}
}  // namespace

namespace contextual_tasks {

// Chrome-driven (controlled) cobrowsing communication:
// `AimToClientMessage.update_thread_context_library`
// (aim_communication.proto). The message type is top-level in that proto, so
// it is `lens::UpdateThreadContextLibrary` rather than a nested type.
std::vector<UrlResource> ConvertAiModeContextToUrlResources(
    const lens::UpdateThreadContextLibrary& message,
    const std::vector<contextual_search::FileInfo>& local_contexts) {
  return ConvertImpl(message, local_contexts);
}

// AIM-search-driven (controlled) cobrowsing communication:
// `SearchToClientMessage.update_thread_context_library`
// (search_communication.proto).
std::vector<UrlResource> ConvertAiModeContextToUrlResources(
    const lens::SearchToClientMessage::UpdateThreadContextLibrary& message,
    const std::vector<contextual_search::FileInfo>& local_contexts) {
  return ConvertImpl(message, local_contexts);
}

}  // namespace contextual_tasks
