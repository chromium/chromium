// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_merged_surface_ordering_storage.h"

#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder_children.h"
#include "components/bookmarks/browser/bookmark_node.h"

BookmarkMergedSurfaceOrderingStorage::BookmarkMergedSurfaceOrderingStorage(
    const BookmarkMergedSurfaceService* service,
    const base::FilePath& file_path)
    : service_(service),
      path_(file_path),
      backend_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      writer_(file_path,
              backend_task_runner_,
              kSaveDelay,
              "BookmarkMergedSurfaceOrderingStorage") {
  CHECK(!file_path.empty());
  CHECK(service);
}

base::ImportantFileWriter::BackgroundDataProducerCallback
BookmarkMergedSurfaceOrderingStorage::
    GetSerializedDataProducerForBackgroundSequence() {
  base::Value::Dict value = EncodeOrderingToDict();
  return base::BindOnce(
      [](base::Value::Dict value) -> std::optional<std::string> {
        // This runs on the background sequence.
        return base::WriteJsonWithOptions(
            value, base::JSONWriter::OPTIONS_PRETTY_PRINT);
      },
      std::move(value));
}

BookmarkMergedSurfaceOrderingStorage::~BookmarkMergedSurfaceOrderingStorage() {
  SaveNowIfScheduled();
}

void BookmarkMergedSurfaceOrderingStorage::ScheduleSave() {
  writer_.ScheduleWriteWithBackgroundDataSerializer(this);
}

void BookmarkMergedSurfaceOrderingStorage::SaveNowIfScheduled() {
  if (writer_.HasPendingWrite()) {
    writer_.DoScheduledWrite();
  }
}

base::Value::Dict BookmarkMergedSurfaceOrderingStorage::EncodeOrderingToDict()
    const {
  const std::vector<std::pair<BookmarkParentFolder, std::string_view>>
      permanent_folders{
          {BookmarkParentFolder::BookmarkBarFolder(),
           kBookmarkBarFolderNameKey},
          {BookmarkParentFolder::OtherFolder(), kOtherBookmarkFolderNameKey},
          {BookmarkParentFolder::MobileFolder(), kMobileFolderNameKey}};
  base::Value::Dict main;
  for (const auto& [folder, key] : permanent_folders) {
    if (!service_->IsNonDefaultOrderingTracked(folder)) {
      continue;
    }
    BookmarkParentFolderChildren children = service_->GetChildren(folder);
    CHECK(children.size());
    main.Set(key, EncodeChildren(children));
  }
  return main;
}

// static
base::Value::List BookmarkMergedSurfaceOrderingStorage::EncodeChildren(
    const BookmarkParentFolderChildren& children) {
  base::Value::List child_values;
  for (const bookmarks::BookmarkNode* child : children) {
    child_values.Append(base::NumberToString(child->id()));
  }
  return child_values;
}

bool BookmarkMergedSurfaceOrderingStorage::HasScheduledSaveForTesting() const {
  return writer_.HasPendingWrite();
}
