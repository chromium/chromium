// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_AI_MODE_CONTEXT_LIBRARY_CONVERTER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_AI_MODE_CONTEXT_LIBRARY_CONVERTER_H_

#include <vector>

#include "components/contextual_tasks/public/contextual_task.h"

namespace contextual_search {
struct FileInfo;
}  // namespace contextual_search

namespace lens {
class UpdateThreadContextLibrary;
class SearchToClientMessage_UpdateThreadContextLibrary;
}  // namespace lens

namespace contextual_tasks {

// Converts an UpdateThreadContextLibrary message from AI mode into a vector of
// UrlResource objects, enriching them with local tab information from
// `local_contexts` based on matching context IDs.
//
// For methods from original contextual tasks architecture communication:
// received as `AimToClientMessage.update_thread_context_library`
// (aim_communication.proto).
// The original contextual tasks was Chrome-focused, with AIM webpage
// being embedded inside Chrome components.
std::vector<UrlResource> ConvertAiModeContextToUrlResources(
    const lens::UpdateThreadContextLibrary& message,
    const std::vector<contextual_search::FileInfo>& local_contexts);

// Same as above, but for the new contextual tasks re-architecture
// communication: received as
// `SearchToClientMessage.update_thread_context_library`
// (search_communication.proto).
// The new contextual tasks is search/AIM-focused, with Chrome components
// being embedded inside AIM webpage.
std::vector<UrlResource> ConvertAiModeContextToUrlResources(
    const lens::SearchToClientMessage_UpdateThreadContextLibrary& message,
    const std::vector<contextual_search::FileInfo>& local_contexts);

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_AI_MODE_CONTEXT_LIBRARY_CONVERTER_H_
