// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/fake_provided_file_system.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "components/services/filesystem/public/interfaces/types.mojom.h"
#include "net/base/io_buffer.h"

namespace chromeos {
namespace file_system_provider {
namespace {

const char kFakeFileName[] = "hello.txt";
const char kFakeFileText[] =
    "This is a testing file. Lorem ipsum dolor sit amet est.";
const size_t kFakeFileSize = sizeof(kFakeFileText) - 1u;
const char kFakeFileModificationTime[] = "Fri Apr 25 01:47:53 UTC 2014";
const char kFakeFileMimeType[] = "text/plain";

}  // namespace

const base::FilePath::CharType kFakeFilePath[] =
    FILE_PATH_LITERAL("/hello.txt");

FakeEntry::FakeEntry() {
}

FakeEntry::FakeEntry(std::unique_ptr<EntryMetadata> metadata,
                     const std::string& contents)
    : metadata(std::move(metadata)), contents(contents) {}

FakeEntry::~FakeEntry() {
}

FakeProvidedFileSystem::FakeProvidedFileSystem(
    const ProvidedFileSystemInfo& file_system_info)
    : file_system_info_(file_system_info),
      last_file_handle_(0),
      weak_ptr_factory_(this) {
  AddEntry(base::FilePath(FILE_PATH_LITERAL("/")), true, "", 0, base::Time(),
           "", "");

  base::Time modification_time;
  DCHECK(base::Time::FromString(kFakeFileModificationTime, &modification_time));
  AddEntry(base::FilePath(kFakeFilePath), false, kFakeFileName, kFakeFileSize,
           modification_time, kFakeFileMimeType, kFakeFileText);
}

FakeProvidedFileSystem::~FakeProvidedFileSystem() {}

void FakeProvidedFileSystem::AddEntry(const base::FilePath& entry_path,
                                      bool is_directory,
                                      const std::string& name,
                                      int64_t size,
                                      base::Time modification_time,
                                      std::string mime_type,
                                      std::string contents) {
  DCHECK(entries_.find(entry_path) == entries_.end());
  std::unique_ptr<EntryMetadata> metadata(new EntryMetadata);

  metadata->is_directory.reset(new bool(is_directory));
  metadata->name.reset(new std::string(name));
  metadata->size.reset(new int64_t(size));
  metadata->modification_time.reset(new base::Time(modification_time));
  metadata->mime_type.reset(new std::string(mime_type));

  entries_[entry_path] =
      std::make_unique<FakeEntry>(std::move(metadata), contents);
}

const FakeEntry* FakeProvidedFileSystem::GetEntry(
    const base::FilePath& entry_path) const {
  const Entries::const_iterator entry_it = entries_.find(entry_path);
  if (entry_it == entries_.end())
    return NULL;

  return entry_it->second.get();
}

AbortCallback FakeProvidedFileSystem::RequestUnmount(
    storage::AsyncFileUtil::StatusCallback callback) {
  return PostAbortableTask(
      base::BindOnce(std::move(callback), base::File::FILE_OK));
}

AbortCallback FakeProvidedFileSystem::GetMetadata(
    const base::FilePath& entry_path,
    ProvidedFileSystemInterface::MetadataFieldMask fields,
    ProvidedFileSystemInterface::GetMetadataCallback callback) {
  const Entries::const_iterator entry_it = entries_.find(entry_path);

  if (entry_it == entries_.end()) {
    return PostAbortableTask(base::BindOnce(std::move(callback), nullptr,
                                            base::File::FILE_ERROR_NOT_FOUND));
  }

  std::unique_ptr<EntryMetadata> metadata(new EntryMetadata);
  if (fields & ProvidedFileSystemInterface::METADATA_FIELD_IS_DIRECTORY) {
    metadata->is_directory.reset(
        new bool(*entry_it->second->metadata->is_directory));
  }
  if (fields & ProvidedFileSystemInterface::METADATA_FIELD_NAME)
    metadata->name.reset(new std::string(*entry_it->second->metadata->name));
  if (fields & ProvidedFileSystemInterface::METADATA_FIELD_SIZE)
    metadata->size.reset(new int64_t(*entry_it->second->metadata->size));
  if (fields & ProvidedFileSystemInterface::METADATA_FIELD_MODIFICATION_TIME) {
    metadata->modification_time.reset(
        new base::Time(*entry_it->second->metadata->modification_time));
  }
  if (fields & ProvidedFileSystemInterface::METADATA_FIELD_MIME_TYPE &&
      entry_it->second->metadata->mime_type.get()) {
    metadata->mime_type.reset(
        new std::string(*entry_it->second->metadata->mime_type));
  }
  if (fields & ProvidedFileSystemInterface::METADATA_FIELD_THUMBNAIL &&
      entry_it->second->metadata->thumbnail.get()) {
    metadata->thumbnail.reset(
        new std::string(*entry_it->second->metadata->thumbnail));
  }

  return PostAbortableTask(base::BindOnce(
      std::move(callback), base::Passed(&metadata), base::File::FILE_OK));
}

AbortCallback FakeProvidedFileSystem::GetActions(
    const std::vector<base::FilePath>& entry_paths,
    ProvidedFileSystemInterface::GetActionsCallback callback) {
  // TODO(mtomasz): Implement it once needed.
  const std::vector<Action> actions;
  return PostAbortableTask(
      base::BindOnce(std::move(callback), actions, base::File::FILE_OK));
}

AbortCallback FakeProvidedFileSystem::ExecuteAction(
    const std::vector<base::FilePath>& entry_paths,
    const std::string& action_id,
    storage::AsyncFileUtil::StatusCallback callback) {
  // TODO(mtomasz): Implement it once needed.
  return PostAbortableTask(
      base::BindOnce(std::move(callback), base::File::FILE_OK));
}

AbortCallback FakeProvidedFileSystem::ReadDirectory(
    const base::FilePath& directory_path,
    storage::AsyncFileUtil::ReadDirectoryCallback callback) {
  storage::AsyncFileUtil::EntryList entry_list;

  for (Entries::const_iterator it = entries_.begin(); it != entries_.end();
       ++it) {
    const base::FilePath file_path = it->first;
    if (file_path == directory_path || directory_path.IsParent(file_path)) {
      const EntryMetadata* const metadata = it->second->metadata.get();
      entry_list.emplace_back(
          base::FilePath(*metadata->name),
          *metadata->is_directory
              ? filesystem::mojom::FsFileType::DIRECTORY
              : filesystem::mojom::FsFileType::REGULAR_FILE);
    }
  }

  return PostAbortableTask(base::Bind(
      callback, base::File::FILE_OK, entry_list, false /* has_more */));
}

AbortCallback FakeProvidedFileSystem::OpenFile(const base::FilePath& entry_path,
                                               OpenFileMode mode,
                                               OpenFileCallback callback) {
  const Entries::const_iterator entry_it = entries_.find(entry_path);

  if (entry_it == entries_.end()) {
    return PostAbortableTask(base::BindOnce(std::move(callback),
                                            0 /* file_handle */,
                                            base::File::FILE_ERROR_NOT_FOUND));
  }

  const int file_handle = ++last_file_handle_;
  opened_files_[file_handle] = OpenedFile(entry_path, mode);
  return PostAbortableTask(
      base::BindOnce(std::move(callback), file_handle, base::File::FILE_OK));
}

AbortCallback FakeProvidedFileSystem::CloseFile(
    int file_handle,
    storage::AsyncFileUtil::StatusCallback callback) {
  const auto opened_file_it = opened_files_.find(file_handle);

  if (opened_file_it == opened_files_.end()) {
    return PostAbortableTask(
        base::BindOnce(std::move(callback), base::File::FILE_ERROR_NOT_FOUND));
  }

  opened_files_.erase(opened_file_it);
  return PostAbortableTask(
      base::BindOnce(std::move(callback), base::File::FILE_OK));
}

AbortCallback FakeProvidedFileSystem::ReadFile(
    int file_handle,
    net::IOBuffer* buffer,
    int64_t offset,
    int length,
    ProvidedFileSystemInterface::ReadChunkReceivedCallback callback) {
  const auto opened_file_it = opened_files_.find(file_handle);

  if (opened_file_it == opened_files_.end() ||
      opened_file_it->second.file_path.AsUTF8Unsafe() != kFakeFilePath) {
    return PostAbortableTask(
        base::Bind(callback,
                   0 /* chunk_length */,
                   false /* has_more */,
                   base::File::FILE_ERROR_INVALID_OPERATION));
  }

  const Entries::const_iterator entry_it =
      entries_.find(opened_file_it->second.file_path);
  if (entry_it == entries_.end()) {
    return PostAbortableTask(
        base::Bind(callback,
                   0 /* chunk_length */,
                   false /* has_more */,
                   base::File::FILE_ERROR_INVALID_OPERATION));
  }

  // Send the response byte by byte.
  int64_t current_offset = offset;
  int current_length = length;

  // Reading behind EOF is fine, it will just return 0 bytes.
  if (current_offset >= *entry_it->second->metadata->size || !current_length) {
    return PostAbortableTask(base::Bind(callback,
                                        0 /* chunk_length */,
                                        false /* has_more */,
                                        base::File::FILE_OK));
  }

  const FakeEntry* const entry = entry_it->second.get();
  std::vector<int> task_ids;
  while (current_offset < *entry->metadata->size && current_length) {
    buffer->data()[current_offset - offset] = entry->contents[current_offset];
    const bool has_more =
        (current_offset + 1 < *entry->metadata->size) && (current_length - 1);
    const int task_id =
        tracker_.PostTask(base::ThreadTaskRunnerHandle::Get().get(), FROM_HERE,
                          base::BindOnce(callback, 1 /* chunk_length */,
                                         has_more, base::File::FILE_OK));
    task_ids.push_back(task_id);
    current_offset++;
    current_length--;
  }

  return base::Bind(&FakeProvidedFileSystem::AbortMany,
                    weak_ptr_factory_.GetWeakPtr(),
                    task_ids);
}

AbortCallback FakeProvidedFileSystem::CreateDirectory(
    const base::FilePath& directory_path,
    bool recursive,
    storage::AsyncFileUtil::StatusCallback callback) {
  // TODO(mtomasz): Implement it once needed.
  return PostAbortableTask(
      base::BindOnce(std::move(callback), base::File::FILE_OK));
}

AbortCallback FakeProvidedFileSystem::DeleteEntry(
    const base::FilePath& entry_path,
    bool recursive,
    storage::AsyncFileUtil::StatusCallback callback) {
  // TODO(mtomasz): Implement it once needed.
  return PostAbortableTask(
      base::BindOnce(std::move(callback), base::File::FILE_OK));
}

AbortCallback FakeProvidedFileSystem::CreateFile(
    const base::FilePath& file_path,
    storage::AsyncFileUtil::StatusCallback callback) {
  const base::File::Error result = file_path.AsUTF8Unsafe() != kFakeFilePath
                                       ? base::File::FILE_ERROR_EXISTS
                                       : base::File::FILE_OK;

  return PostAbortableTask(base::BindOnce(std::move(callback), result));
}

AbortCallback FakeProvidedFileSystem::CopyEntry(
    const base::FilePath& source_path,
    const base::FilePath& target_path,
    storage::AsyncFileUtil::StatusCallback callback) {
  // TODO(mtomasz): Implement it once needed.
  return PostAbortableTask(
      base::BindOnce(std::move(callback), base::File::FILE_OK));
}

AbortCallback FakeProvidedFileSystem::MoveEntry(
    const base::FilePath& source_path,
    const base::FilePath& target_path,
    storage::AsyncFileUtil::StatusCallback callback) {
  // TODO(mtomasz): Implement it once needed.
  return PostAbortableTask(
      base::BindOnce(std::move(callback), base::File::FILE_OK));
}

AbortCallback FakeProvidedFileSystem::Truncate(
    const base::FilePath& file_path,
    int64_t length,
    storage::AsyncFileUtil::StatusCallback callback) {
  // TODO(mtomasz): Implement it once needed.
  return PostAbortableTask(
      base::BindOnce(std::move(callback), base::File::FILE_OK));
}

AbortCallback FakeProvidedFileSystem::WriteFile(
    int file_handle,
    net::IOBuffer* buffer,
    int64_t offset,
    int length,
    storage::AsyncFileUtil::StatusCallback callback) {
  const auto opened_file_it = opened_files_.find(file_handle);

  if (opened_file_it == opened_files_.end() ||
      opened_file_it->second.file_path.AsUTF8Unsafe() != kFakeFilePath) {
    return PostAbortableTask(base::BindOnce(
        std::move(callback), base::File::FILE_ERROR_INVALID_OPERATION));
  }

  const Entries::iterator entry_it =
      entries_.find(opened_file_it->second.file_path);
  if (entry_it == entries_.end()) {
    return PostAbortableTask(base::BindOnce(
        std::move(callback), base::File::FILE_ERROR_INVALID_OPERATION));
  }

  FakeEntry* const entry = entry_it->second.get();
  if (offset > *entry->metadata->size) {
    return PostAbortableTask(base::BindOnce(
        std::move(callback), base::File::FILE_ERROR_INVALID_OPERATION));
  }

  // Allocate the string size in advance.
  if (offset + length > *entry->metadata->size) {
    *entry->metadata->size = offset + length;
    entry->contents.resize(*entry->metadata->size);
  }

  entry->contents.replace(offset, length, buffer->data(), length);

  return PostAbortableTask(
      base::BindOnce(std::move(callback), base::File::FILE_OK));
}

AbortCallback FakeProvidedFileSystem::AddWatcher(
    const GURL& origin,
    const base::FilePath& entry_watcher,
    bool recursive,
    bool persistent,
    storage::AsyncFileUtil::StatusCallback callback,
    const storage::WatcherManager::NotificationCallback&
        notification_callback) {
  // TODO(mtomasz): Implement it once needed.
  return PostAbortableTask(
      base::BindOnce(std::move(callback), base::File::FILE_OK));
}

void FakeProvidedFileSystem::RemoveWatcher(
    const GURL& origin,
    const base::FilePath& entry_path,
    bool recursive,
    storage::AsyncFileUtil::StatusCallback callback) {
  // TODO(mtomasz): Implement it once needed.
  std::move(callback).Run(base::File::FILE_OK);
}

const ProvidedFileSystemInfo& FakeProvidedFileSystem::GetFileSystemInfo()
    const {
  return file_system_info_;
}

RequestManager* FakeProvidedFileSystem::GetRequestManager() {
  NOTREACHED();
  return NULL;
}

Watchers* FakeProvidedFileSystem::GetWatchers() {
  return &watchers_;
}

const OpenedFiles& FakeProvidedFileSystem::GetOpenedFiles() const {
  return opened_files_;
}

void FakeProvidedFileSystem::AddObserver(ProvidedFileSystemObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void FakeProvidedFileSystem::RemoveObserver(
    ProvidedFileSystemObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void FakeProvidedFileSystem::Notify(
    const base::FilePath& entry_path,
    bool recursive,
    storage::WatcherManager::ChangeType change_type,
    std::unique_ptr<ProvidedFileSystemObserver::Changes> changes,
    const std::string& tag,
    storage::AsyncFileUtil::StatusCallback callback) {
  NOTREACHED();
  std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
}

void FakeProvidedFileSystem::Configure(
    storage::AsyncFileUtil::StatusCallback callback) {
  NOTREACHED();
  std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
}

base::WeakPtr<ProvidedFileSystemInterface>
FakeProvidedFileSystem::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

AbortCallback FakeProvidedFileSystem::PostAbortableTask(
    base::OnceClosure callback) {
  const int task_id =
      tracker_.PostTask(base::ThreadTaskRunnerHandle::Get().get(), FROM_HERE,
                        std::move(callback));
  return base::Bind(
      &FakeProvidedFileSystem::Abort, weak_ptr_factory_.GetWeakPtr(), task_id);
}

void FakeProvidedFileSystem::Abort(int task_id) {
  tracker_.TryCancel(task_id);
}

void FakeProvidedFileSystem::AbortMany(const std::vector<int>& task_ids) {
  for (size_t i = 0; i < task_ids.size(); ++i) {
    tracker_.TryCancel(task_ids[i]);
  }
}

}  // namespace file_system_provider
}  // namespace chromeos
