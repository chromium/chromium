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
}  // namespace lens

namespace contextual_tasks {

// Converts an UpdateThreadContextLibrary message from AI mode into a vector of
// UrlResource objects, enriching them with local tab information from
// `local_contexts` based on matching context IDs.
std::vector<UrlResource> ConvertAiModeContextToUrlResources(
    const lens::UpdateThreadContextLibrary& message,
    const std::vector<contextual_search::FileInfo>& local_contexts);

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_AI_MODE_CONTEXT_LIBRARY_CONVERTER_H_
