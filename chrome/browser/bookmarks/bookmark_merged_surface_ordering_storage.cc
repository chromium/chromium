// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_merged_surface_ordering_storage.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_parent_folder_children.h"
#include "components/bookmarks/browser/bookmark_node.h"

namespace {

// Loads and deserializes a JSON file determined by `file_path` and returns it
// in the form of a `base::Value` or an error code if something fails.
base::expected<base::Value, int> LoadFileToDict(
    const base::FilePath& file_path) {
  int error_code;
  JSONFileValueDeserializer deserializer(file_path,
                                         base::JSON_REPLACE_INVALID_CHARACTERS);
  std::unique_ptr<base::Value> root =
      deserializer.Deserialize(&error_code, /*error_message=*/nullptr);
  if (!root) {
    return base::unexpected<int>(error_code);
  }

  return std::move(*root);
}

std::vector<int64_t> DecodeNodeIds(const base::Value::List& list) {
  std::vector<int64_t> ids;
  for (const base::Value& id_value : list) {
    int64_t id = 0;
    const std::string* id_string = id_value.GetIfString();
    if (!id_string || !base::StringToInt64(*id_string, &id)) {
      continue;
    }
    ids.push_back(id);
  }
  return ids;
}

}  // namespace

// Loader

// static
std::unique_ptr<BookmarkMergedSurfaceOrderingStorage::Loader>
BookmarkMergedSurfaceOrderingStorage::Loader::Create(
    const base::FilePath& file_path,
    base::OnceCallback<void(LoadResult)> on_load_complete) {
  CHECK(on_load_complete);
  auto loader =
      base::WrapUnique(new BookmarkMergedSurfaceOrderingStorage::Loader(
          std::move(on_load_complete)));
  loader->LoadOnBackgroundThread(file_path);
  return loader;
}

BookmarkMergedSurfaceOrderingStorage::Loader::Loader(
    base::OnceCallback<void(LoadResult)> on_load_complete)
    : load_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      on_load_complete_(std::move(on_load_complete)) {
  CHECK(on_load_complete_);
}

BookmarkMergedSurfaceOrderingStorage::Loader::~Loader() = default;

void BookmarkMergedSurfaceOrderingStorage::Loader::LoadOnBackgroundThread(
    const base::FilePath& file_path) {
  load_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&LoadFileToDict, file_path),
      base::BindOnce(&Loader::OnLoadedComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BookmarkMergedSurfaceOrderingStorage::Loader::OnLoadedComplete(
    base::expected<base::Value, int> result_or_error) {
  // TODO(crbug.com/393047033): record file load metrics.
  if (!result_or_error.has_value() || !result_or_error.value().is_dict()) {
    std::move(on_load_complete_).Run(LoadResult());
    return;
  }

  base::Value::Dict& result = result_or_error.value().GetDict();
  LoadResult loaded_results;

  for (const auto [name_key, type] :
       base::flat_map<std::string_view,
                      BookmarkParentFolder::PermanentFolderType>{
           {kBookmarkBarFolderNameKey,
            BookmarkParentFolder::PermanentFolderType::kBookmarkBarNode},
           {kOtherBookmarkFolderNameKey,
            BookmarkParentFolder::PermanentFolderType::kOtherNode},
           {kMobileFolderNameKey,
            BookmarkParentFolder::PermanentFolderType::kMobileNode}}) {
    if (auto* list = result.FindList(name_key); list) {
      loaded_results.emplace(type, DecodeNodeIds(*list));
    }
  }
  std::move(on_load_complete_).Run(std::move(loaded_results));
}

// BookmarkMergedSurfaceOrderingStorage

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
