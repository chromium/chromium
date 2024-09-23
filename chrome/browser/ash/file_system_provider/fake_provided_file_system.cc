// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/file_system_provider/fake_provided_file_system.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::file_system_provider {

const char kFakeFileText[] =
    "This is a testing file. Lorem ipsum dolor sit amet est.";

namespace {

const char kFakeFileName[] = "hello.txt";
const size_t kFakeFileSize = sizeof(kFakeFileText) - 1u;
const char kFakeFileModificationTime[] = "Fri, 25 Apr 2014 01:47:53";
const char kFakeFileMimeType[] = "text/plain";

constexpr base::FilePath::CharType kBadFakeEntryPath1[] =
    FILE_PATH_LITERAL("/bad1");
constexpr char kBadFakeEntryName1[] = "/bad1";
constexpr base::FilePath::CharType kBadFakeEntryPath2[] =
    FILE_PATH_LITERAL("/bad2");
constexpr char kBadFakeEntryName2[] = "bad2";

}  // namespace

const base::FilePath::CharType kFakeFilePath[] =
    FILE_PATH_LITERAL("/hello.txt");

constexpr char kFakeFileVersionTag[] = "versionA";

FakeEntry::FakeEntry() = default;

FakeEntry::FakeEntry(std::unique_ptr<EntryMetadata> metadata,
                     const std::string& contents)
    : metadata(std::move(metadata)), contents(contents) {}

FakeEntry::~FakeEntry() = default;

FakeProvidedFileSystem::FakeProvidedFileSystem(
    const ProvidedFileSystemInfo& file_system_info)
    : file_system_info_(file_system_info), last_file_handle_(0) {
  AddEntry(base::FilePath(FILE_PATH_LITERAL("/")), /*is_directory=*/true,
           /*name=*/"", /*size=*/0, /*modification_time=*/base::Time(),
           /*mime_type=*/"", /*cloud_file_info=*/nullptr, /*contents=*/"");

  base::Time modification_time;
  EXPECT_TRUE(
      base::Time::FromUTCString(kFakeFileModificationTime, &modification_time));
  AddEntry(base::FilePath(kFakeFilePath), false, kFakeFileName, kFakeFileSize,
           modification_time, kFakeFileMimeType,
           std::make_unique<CloudFileInfo>(kFakeFileVersionTag), kFakeFileText);

  // Add a set of bad entries, in the root directory, which should be filtered
  // out.
  AddEntry(base::FilePath(kBadFakeEntryPath1), false, kBadFakeEntryName1,
           kFakeFileSize, modification_time, kFakeFileMimeType,
           /*cloud_file_info=*/nullptr, kFakeFileText);
  AddEntry(base::FilePath(kBadFakeEntryPath2), false, kBadFakeEntryName2,
           kFakeFileSize, modification_time, kFakeFileMimeType,
           /*cloud_file_info=*/nullptr, kFakeFileText);
}

FakeProvidedFileSystem::~FakeProvidedFileSystem() = default;

void FakeProvidedFileSystem::AddEntry(
    const base::FilePath& entry_path,
    bool is_directory,
    const std::string& name,
    int64_t size,
    base::Time modification_time,
    std::string mime_type,
    std::unique_ptr<CloudFileInfo> cloud_file_info,
    std::string contents) {
  DCHECK(entries_.find(entry_path) == entries_.end())
      << "Already present " << entry_path;
  std::unique_ptr<EntryMetadata> metadata(new EntryMetadata);

  metadata->is_directory = std::make_unique<bool>(is_directory);
  metadata->name = std::make_unique<std::string>(name);
  metadata->size = std::make_unique<int64_t>(size);
  metadata->modification_time = std::make_unique<base::Time>(modification_time);
  metadata->mime_type = std::make_unique<std::string>(mime_type);
  metadata->cloud_file_info = std::move(cloud_file_info);

  entries_[entry_path] =
      std::make_unique<FakeEntry>(std::move(metadata), contents);
}

FakeEntry* FakeProvidedFileSystem::GetEntry(
    const base::FilePath& entry_path) const {
  const Entries::const_iterator entry_it = entries_.find(entry_path);
  if (entry_it == entries_.end())
    return nullptr;

  return entry_it->second.get();
}

base::File::Error FakeProvidedFileSystem::CopyOrMoveEntry(
    const base::FilePath& source_path,
    const base::FilePath& target_path,
    bool is_move) {
  // Check that the source entry exists.
  const FakeEntry* source_entry = GetEntry(source_path);
  if (!source_entry) {
    return base::File::FILE_ERROR_NOT_FOUND;
  }
  // Delete `target_path` if it already exists, since AddEntry DCHECKs that
  // there's no existing entry.
  switch (auto err = DoDeleteEntry(target_path, /*recursive=*/false)) {
    case base::File::Error::FILE_OK:
    case base::File::Error::FILE_ERROR_NOT_FOUND:
      break;
    default:
      return err;
  }
  // Copy source entry.
  DCHECK_NE(source_entry->metadata, nullptr);
  DCHECK_NE(source_entry->metadata->is_directory, nullptr);
  DCHECK_NE(source_entry->metadata->name, nullptr);
  DCHECK_NE(source_entry->metadata->size, nullptr);
  DCHECK_NE(source_entry->metadata->modification_time, nullptr);
  DCHECK_NE(source_entry->metadata->mime_type, nullptr);

  auto cloud_file_info =
      (source_entry->metadata->cloud_file_info)
          ? std::make_unique<CloudFileInfo>(
                source_entry->metadata->cloud_file_info->version_tag)
          : nullptr;

  AddEntry(target_path, *(source_entry->metadata->is_directory),
           *(source_entry->metadata->name), *(source_entry->metadata->size),
           *(source_entry->metadata->modification_time),
           *(source_entry->metadata->mime_type), std::move(cloud_file_info),
           source_entry->contents);
  if (is_move) {
    entries_.erase(source_path);
  }
  return base::File::FILE_OK;
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
    metadata->is_directory =
        std::make_unique<bool>(*entry_it->second->metadata->is_directory);
  }
  if (fields & ProvidedFileSystemInterface::METADATA_FIELD_NAME)
    metadata->name =
        std::make_unique<std::string>(*entry_it->second->metadata->name);
  if (fields & ProvidedFileSystemInterface::METADATA_FIELD_SIZE)
    metadata->size =
        std::make_unique<int64_t>(*entry_it->second->metadata->size);
  if (fields & ProvidedFileSystemInterface::METADATA_FIELD_MODIFICATION_TIME) {
    metadata->modification_time = std::make_unique<base::Time>(
        *entry_it->second->metadata->modification_time);
  }
  if (fields & ProvidedFileSystemInterface::METADATA_FIELD_MIME_TYPE &&
      entry_it->second->metadata->mime_type.get()) {
    metadata->mime_type =
        std::make_unique<std::string>(*entry_it->second->metadata->mime_type);
  }
  if (fields & ProvidedFileSystemInterface::METADATA_FIELD_THUMBNAIL &&
      entry_it->second->metadata->thumbnail.get()) {
    metadata->thumbnail =
        std::make_unique<std::string>(*entry_it->second->metadata->thumbnail);
  }
  // Make a copy of the `CloudFileInfo` to pass to the callback.
  if (fields & ProvidedFileSystemInterface::METADATA_FIELD_CLOUD_FILE_INFO &&
      entry_it->second->metadata->cloud_file_info.get()) {
    metadata->cloud_file_info = std::make_unique<CloudFileInfo>(
        entry_it->second->metadata->cloud_file_info->version_tag);
  }

  return PostAbortableTask(base::BindOnce(
      std::move(callback), std::move(metadata), base::File::FILE_OK));
}

AbortCallback FakeProvidedFileSystem::GetActions(
    const std::vector<base::FilePath>& entry_paths,
    ProvidedFileSystemInterface::GetActionsCallback callback) {
  const std::vector<Action> actions;
  return PostAbortableTask(
      base::BindOnce(std::move(callback), actions, base::File::FILE_OK));
}

AbortCallback FakeProvidedFileSystem::ExecuteAction(
    const std::vector<base::FilePath>& entry_paths,
    const std::string& action_id,
    storage::AsyncFileUtil::StatusCallback callback) {
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
    if (directory_path == file_path.DirName()) {
      const EntryMetadata* const metadata = it->second->metadata.get();
      filesystem::mojom::FsFileType entry_type =
          *metadata->is_directory ? filesystem::mojom::FsFileType::DIRECTORY
                                  : filesystem::mojom::FsFileType::REGULAR_FILE;
      if (*metadata->name == kBadFakeEntryName2) {
        entry_type = static_cast<filesystem::mojom::FsFileType>(7);
      }
      entry_list.emplace_back(base::FilePath(*metadata->name), base::FilePath(),
                              entry_type);
    }
  }

  return PostAbortableTask(base::BindOnce(callback, base::File::FILE_OK,
                                          entry_list, /*has_more=*/false));
}

AbortCallback FakeProvidedFileSystem::OpenFile(const base::FilePath& entry_path,
                                               OpenFileMode mode,
                                               OpenFileCallback callback) {
  const Entries::const_iterator entry_it = entries_.find(entry_path);

  if (entry_it == entries_.end()) {
    return PostAbortableTask(base::BindOnce(
        std::move(callback), /*file_handle=*/0,
        base::File::FILE_ERROR_NOT_FOUND, /*cloud_file_info=*/nullptr));
  }

  FakeEntry& entry = *entry_it->second;
  if (mode == OPEN_FILE_MODE_WRITE && flush_required_) {
    DCHECK(!entry.write_buffer);
    entry.write_buffer = entry.contents;
  }

  // Make a copy of the `EntryMetadata` to pass to the callback.
  std::unique_ptr<EntryMetadata> metadata =
      (entry.metadata) ? std::make_unique<EntryMetadata>() : nullptr;
  if (entry.metadata && entry.metadata->cloud_file_info) {
    metadata->cloud_file_info = std::make_unique<CloudFileInfo>(
        entry.metadata->cloud_file_info->version_tag);
  }
  if (entry.metadata && entry.metadata->size) {
    metadata->size = std::make_unique<int64_t>(*entry.metadata->size);
  }

  const int file_handle = ++last_file_handle_;
  opened_files_[file_handle] = OpenedFile(entry_path, mode);
  return PostAbortableTask(base::BindOnce(std::move(callback), file_handle,
                                          base::File::FILE_OK,
                                          std::move(metadata)));
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
        base::BindOnce(callback, /*chunk_length=*/0, /*has_more=*/false,
                       base::File::FILE_ERROR_INVALID_OPERATION));
  }

  const Entries::const_iterator entry_it =
      entries_.find(opened_file_it->second.file_path);
  if (entry_it == entries_.end()) {
    return PostAbortableTask(
        base::BindOnce(callback, /*chunk_length=*/0, /*has_more=*/false,
                       base::File::FILE_ERROR_INVALID_OPERATION));
  }

  // Send the response byte by byte.
  int64_t current_offset = offset;
  int current_length = length;

  // Reading behind EOF is fine, it will just return 0 bytes.
  if (current_offset >= *entry_it->second->metadata->size || !current_length) {
    return PostAbortableTask(base::BindOnce(callback, /*chunk_length=*/0,
                                            /*has_more=*/false,
                                            base::File::FILE_OK));
  }

  const FakeEntry* const entry = entry_it->second.get();
  std::vector<int> task_ids;
  while (current_offset < *entry->metadata->size && current_length) {
    buffer->data()[current_offset - offset] = entry->contents[current_offset];
    const bool has_more =
        (current_offset + 1 < *entry->metadata->size) && (current_length - 1);
    const int task_id = tracker_.PostTask(
        base::SingleThreadTaskRunner::GetCurrentDefault().get(), FROM_HERE,
        base::BindOnce(callback, /*chunk_length=*/1, has_more,
                       base::File::FILE_OK));
    task_ids.push_back(task_id);
    current_offset++;
    current_length--;
  }

  return base::BindOnce(&FakeProvidedFileSystem::AbortMany,
                        weak_ptr_factory_.GetWeakPtr(), task_ids);
}

AbortCallback FakeProvidedFileSystem::CreateDirectory(
    const base::FilePath& directory_path,
    bool recursive,
    storage::AsyncFileUtil::StatusCallback callback) {
  // No-op if the directory already exists.
  if (GetEntry(directory_path)) {
    return PostAbortableTask(
        base::BindOnce(std::move(callback), base::File::FILE_OK));
  }
  // Create directories recursively.
  std::vector<base::FilePath::StringType> components =
      directory_path.GetComponents();
  CHECK_EQ(components[0], "/");
  base::FilePath path(components[0]);
  for (size_t i = 1; i < components.size(); ++i) {
    path = path.AppendASCII(components[i]);
    if (GetEntry(path)) {
      continue;
    }
    // If `recursive` is set to false, only the last component of the provided
    // path should be new.
    if (!recursive && i < components.size() - 1) {
      return PostAbortableTask(base::BindOnce(
          std::move(callback), base::File::FILE_ERROR_INVALID_OPERATION));
    }
    AddEntry(path, /*is_directory=*/true, path.BaseName().value(), /*size=*/0,
             base::Time(), /*mime_type=*/"", /*cloud_file_info=*/nullptr,
             /*contents=*/"");
  }
  return PostAbortableTask(
      base::BindOnce(std::move(callback), base::File::FILE_OK));
}

AbortCallback FakeProvidedFileSystem::DeleteEntry(
    const base::FilePath& entry_path,
    bool recursive,
    storage::AsyncFileUtil::StatusCallback callback) {
  auto err = DoDeleteEntry(entry_path, recursive);
  return PostAbortableTask(base::BindOnce(std::move(callback), err));
}

base::File::Error FakeProvidedFileSystem::DoDeleteEntry(
    const base::FilePath& entry_path,
    bool recursive) {
  // Check that the entry to remove exists.
  if (!GetEntry(entry_path)) {
    return base::File::FILE_ERROR_NOT_FOUND;
  }
  // If `recursive` is false, check that `entry_path` is not parent of any entry
  // path in `entries_`.
  if (!recursive) {
    const Entries::const_iterator it =
        base::ranges::find_if(entries_, [entry_path](auto& entry_it) {
          return entry_path.IsParent(entry_it.first);
        });
    if (it != entries_.end()) {
      return base::File::FILE_ERROR_INVALID_OPERATION;
    }
  }
  // Erase the entry with path `entry_path`, as well as all child entries.
  for (Entries::const_iterator entry_it = entries_.begin();
       entry_it != entries_.end();) {
    if (entry_path == entry_it->first || entry_path.IsParent(entry_it->first)) {
      entry_it = entries_.erase(entry_it);
    } else {
      ++entry_it;
    }
  }
  return base::File::FILE_OK;
}

AbortCallback FakeProvidedFileSystem::CreateFile(
    const base::FilePath& file_path,
    storage::AsyncFileUtil::StatusCallback callback) {
  if (GetEntry(file_path)) {
    return PostAbortableTask(
        base::BindOnce(std::move(callback), base::File::FILE_ERROR_EXISTS));
  }
  AddEntry(file_path, /*is_directory=*/false, file_path.BaseName().value(),
           /*size=*/0, base::Time(), kFakeFileMimeType,
           /*cloud_file_info=*/nullptr, std::string());
  return PostAbortableTask(
      base::BindOnce(std::move(callback), base::File::FILE_OK));
}

AbortCallback FakeProvidedFileSystem::CopyEntry(
    const base::FilePath& source_path,
    const base::FilePath& target_path,
    storage::AsyncFileUtil::StatusCallback callback) {
  return PostAbortableTask(base::BindOnce(
      std::move(callback),
      CopyOrMoveEntry(source_path, target_path, /*is_move=*/false)));
}

AbortCallback FakeProvidedFileSystem::MoveEntry(
    const base::FilePath& source_path,
    const base::FilePath& target_path,
    storage::AsyncFileUtil::StatusCallback callback) {
  return PostAbortableTask(base::BindOnce(
      std::move(callback),
      CopyOrMoveEntry(source_path, target_path, /*is_move=*/true)));
}

AbortCallback FakeProvidedFileSystem::Truncate(
    const base::FilePath& file_path,
    int64_t length,
    storage::AsyncFileUtil::StatusCallback callback) {
  FakeEntry* const entry = GetEntry(file_path);
  if (!entry) {
    return PostAbortableTask(
        base::BindOnce(std::move(callback), base::File::FILE_ERROR_NOT_FOUND));
  }
  *entry->metadata->size = length;
  entry->contents.resize(*entry->metadata->size);
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
      !GetEntry(opened_file_it->second.file_path)) {
    return PostAbortableTask(base::BindOnce(
        std::move(callback), base::File::FILE_ERROR_INVALID_OPERATION));
  }

  FakeEntry* const entry = GetEntry(opened_file_it->second.file_path);
  if (!entry) {
    return PostAbortableTask(
        base::BindOnce(std::move(callback), base::File::FILE_ERROR_NOT_FOUND));
  }
  std::string& write_buffer =
      entry->write_buffer ? *entry->write_buffer : entry->contents;
  int64_t buffer_size = static_cast<int64_t>(write_buffer.size());
  if (offset > buffer_size) {
    return PostAbortableTask(base::BindOnce(
        std::move(callback), base::File::FILE_ERROR_INVALID_OPERATION));
  }

  // Allocate the string size in advance.
  if (offset + length > buffer_size) {
    if (!entry->write_buffer) {
      // Only update metadata if we are writing contents directly.
      *entry->metadata->size = offset + length;
      // Update the version when the contents change.
      if (entry->metadata->cloud_file_info.get()) {
        entry->metadata->cloud_file_info->version_tag += "1";
      }
    }
    write_buffer.resize(*entry->metadata->size);
  }

  write_buffer.replace(offset, length, buffer->data(), length);

  return PostAbortableTask(
      base::BindOnce(std::move(callback), base::File::FILE_OK));
}

AbortCallback FakeProvidedFileSystem::FlushFile(
    int file_handle,
    storage::AsyncFileUtil::StatusCallback callback) {
  const auto opened_file_it = opened_files_.find(file_handle);

  if (opened_file_it == opened_files_.end() ||
      !GetEntry(opened_file_it->second.file_path)) {
    return PostAbortableTask(base::BindOnce(
        std::move(callback), base::File::FILE_ERROR_INVALID_OPERATION));
  }

  FakeEntry* const entry = GetEntry(opened_file_it->second.file_path);
  if (!entry) {
    return PostAbortableTask(
        base::BindOnce(std::move(callback), base::File::FILE_ERROR_NOT_FOUND));
  }

  if (entry->write_buffer) {
    *entry->metadata->size = entry->write_buffer->size();
    entry->contents = std::move(*entry->write_buffer);
    entry->write_buffer = std::nullopt;
  }
  return PostAbortableTask(
      base::BindOnce(std::move(callback), base::File::FILE_OK));
}

AbortCallback FakeProvidedFileSystem::AddWatcher(
    const GURL& origin,
    const base::FilePath& entry_watcher,
    bool recursive,
    bool persistent,
    storage::AsyncFileUtil::StatusCallback callback,
    storage::WatcherManager::NotificationCallback notification_callback) {
  // Check if watcher already exists.
  const WatcherKey key(entry_watcher, recursive);
  const Watchers::iterator it = watchers_.find(key);
  if (it != watchers_.end()) {
    return PostAbortableTask(
        base::BindOnce(std::move(callback), base::File::FILE_OK));
  }

  Subscriber subscriber;
  subscriber.origin = origin;
  subscriber.persistent = persistent;
  subscriber.notification_callback = std::move(notification_callback);

  // Add watcher.
  Watcher* const watcher = &watchers_[key];
  watcher->entry_path = entry_watcher;
  watcher->recursive = recursive;
  watcher->subscribers[origin] = subscriber;

  // Notify observers.
  for (auto& observer : observers_) {
    observer.OnWatcherListChanged(file_system_info_, watchers_);
  }

  return PostAbortableTask(
      base::BindOnce(std::move(callback), base::File::FILE_OK));
}

void FakeProvidedFileSystem::RemoveWatcher(
    const GURL& origin,
    const base::FilePath& entry_path,
    bool recursive,
    storage::AsyncFileUtil::StatusCallback callback) {
  // Remove watcher.
  const WatcherKey key(entry_path, recursive);
  const auto it = watchers_.find(key);
  if (it != watchers_.end()) {
    watchers_.erase(it);
  }

  // Notify observers.
  for (auto& observer : observers_) {
    observer.OnWatcherListChanged(file_system_info_, watchers_);
  }

  std::move(callback).Run(base::File::FILE_OK);
}

const ProvidedFileSystemInfo& FakeProvidedFileSystem::GetFileSystemInfo()
    const {
  return file_system_info_;
}

OperationRequestManager* FakeProvidedFileSystem::GetRequestManager() {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
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
  // Very simple implementation that unconditionally calls notification
  // callbacks and notifies observers of the change.

  const WatcherKey key(entry_path, recursive);
  const auto& watcher_it = watchers_.find(key);
  if (watcher_it == watchers_.end()) {
    std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  const ProvidedFileSystemObserver::Changes& changes_ref = *changes.get();

  // Call all notification callbacks (if any).
  for (const auto& subscriber_it : watcher_it->second.subscribers) {
    const storage::WatcherManager::NotificationCallback& notification_callback =
        subscriber_it.second.notification_callback;
    if (!notification_callback.is_null()) {
      notification_callback.Run(change_type);
    }
  }

  // Notify all observers.
  for (auto& observer : observers_) {
    observer.OnWatcherChanged(file_system_info_, watcher_it->second,
                              change_type, changes_ref, base::DoNothing());
  }
}

void FakeProvidedFileSystem::Configure(
    storage::AsyncFileUtil::StatusCallback callback) {
  NOTREACHED_IN_MIGRATION();
  std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
}

base::WeakPtr<ProvidedFileSystemInterface>
FakeProvidedFileSystem::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::unique_ptr<ScopedUserInteraction>
FakeProvidedFileSystem::StartUserInteraction() {
  return nullptr;
}

base::WeakPtr<FakeProvidedFileSystem> FakeProvidedFileSystem::GetFakeWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

AbortCallback FakeProvidedFileSystem::PostAbortableTask(
    base::OnceClosure callback) {
  const int task_id =
      tracker_.PostTask(base::SingleThreadTaskRunner::GetCurrentDefault().get(),
                        FROM_HERE, std::move(callback));
  return base::BindOnce(&FakeProvidedFileSystem::Abort,
                        weak_ptr_factory_.GetWeakPtr(), task_id);
}

void FakeProvidedFileSystem::Abort(int task_id) {
  tracker_.TryCancel(task_id);
}

void FakeProvidedFileSystem::AbortMany(const std::vector<int>& task_ids) {
  for (int task_id : task_ids) {
    tracker_.TryCancel(task_id);
  }
}

}  // namespace ash::file_system_provider
