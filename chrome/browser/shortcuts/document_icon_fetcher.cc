// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/document_icon_fetcher.h"

#include <iterator>
#include <memory>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "chrome/browser/shortcuts/fetch_icons_from_document_task.h"
#include "chrome/browser/shortcuts/shortcut_icon_generator.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace shortcuts {

// static
void DocumentIconFetcher::FetchIcons(content::WebContents& web_contents,
                                     FetchIconsFromDocumentCallback callback) {
  DocumentIconFetcher* fetch_manager =
      DocumentIconFetcher::GetOrCreateForCurrentDocument(
          web_contents.GetPrimaryMainFrame());
  fetch_manager->RunTask(GenerateIconLetterFromName(web_contents.GetTitle()),
                         std::move(callback));
}

DocumentIconFetcher::~DocumentIconFetcher() {
  in_destruction_ = true;
  // All of the tasks callbacks call `DocumentIconFetcher::OnTaskComplete` below
  // directly. Save them & call them synchronously after the tasks are
  // destroyed to prevent map modification during iteration.
  std::vector<FetchIconsFromDocumentCallback> pending_callbacks;
  for (const auto& [id, task] : fetch_tasks_) {
    pending_callbacks.push_back(task->TakeCallback());
  }
  fetch_tasks_.clear();
  for (FetchIconsFromDocumentCallback& callback : pending_callbacks) {
    CHECK(callback);
    // This calls `DocumentIconFetcher::OnTaskComplete` below.
    std::move(callback).Run(
        base::unexpected(FetchIconsForDocumentError::kDocumentDestroyed));
  }
}

DOCUMENT_USER_DATA_KEY_IMPL(DocumentIconFetcher);

DocumentIconFetcher::DocumentIconFetcher(content::RenderFrameHost* rfh)
    : content::DocumentUserData<DocumentIconFetcher>(rfh) {}

void DocumentIconFetcher::RunTask(char32_t fallback_letter,
                                  FetchIconsFromDocumentCallback callback) {
  std::unique_ptr<FetchIconsFromDocumentTask> task =
      std::make_unique<FetchIconsFromDocumentTask>(
          base::PassKey<DocumentIconFetcher>(), render_frame_host());
  int task_id = next_task_id_;
  next_task_id_++;
  const auto& [iter, _] = fetch_tasks_.emplace(task_id, std::move(task));
  // Note: the callback may be called synchronously.
  iter->second->Start(base::BindOnce(&DocumentIconFetcher::OnTaskComplete,
                                     weak_factory_.GetWeakPtr(), task_id,
                                     fallback_letter, std::move(callback)));
}

void DocumentIconFetcher::OnTaskComplete(
    int id,
    char32_t fallback_letter,
    FetchIconsFromDocumentCallback original_callback,
    FetchIconsFromDocumentResult result) {
  int num_erased = fetch_tasks_.erase(id);
  CHECK(num_erased > 0 || in_destruction_);

  if (result.has_value() && result->empty()) {
    result->push_back(GenerateBitmap(128, fallback_letter));
  }

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(original_callback), result));
}

}  // namespace shortcuts
