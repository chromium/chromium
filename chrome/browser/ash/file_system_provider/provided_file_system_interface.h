// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_INTERFACE_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_INTERFACE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_system_provider/abort_callback.h"
#include "chrome/browser/ash/file_system_provider/cloud_file_info.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_observer.h"
#include "chrome/browser/ash/file_system_provider/watcher.h"
#include "storage/browser/file_system/async_file_util.h"
#include "storage/browser/file_system/watcher_manager.h"
#include "url/gurl.h"

namespace base {
class Time;
}  // namespace base

namespace net {
class IOBuffer;
}  // namespace net

namespace ash::file_system_provider {

class ProvidedFileSystemInfo;
class OperationRequestManager;

// Represents a file or directory in cloud storage.
struct CloudIdentifier {
  std::string provider_name;
  std::string id;

  CloudIdentifier(const std::string& provider_name, const std::string& id);
  bool operator==(const CloudIdentifier&) const;
};

// Represents metadata for either a file or a directory.
struct EntryMetadata {
  EntryMetadata();

  EntryMetadata(const EntryMetadata&) = delete;
  EntryMetadata& operator=(const EntryMetadata&) = delete;

  ~EntryMetadata();

  // All of the metadata fields are optional. All strings which are set, are
  // non-empty.
  std::unique_ptr<bool> is_directory;
  std::unique_ptr<std::string> name;
  std::unique_ptr<int64_t> size;
  std::unique_ptr<base::Time> modification_time;
  std::unique_ptr<std::string> mime_type;
  std::unique_ptr<std::string> thumbnail;
  std::unique_ptr<CloudIdentifier> cloud_identifier;
  std::unique_ptr<CloudFileInfo> cloud_file_info;
};

// Represents actions for either a file or a directory.
struct Action {
  std::string id;
  std::string title;
};

typedef std::vector<Action> Actions;

// Mode of opening a file. Used by OpenFile().
enum OpenFileMode { OPEN_FILE_MODE_READ, OPEN_FILE_MODE_WRITE };

// Contains information about an opened file.
struct OpenedFile {
  OpenedFile(const base::FilePath& file_path, OpenFileMode mode);
  OpenedFile();
  ~OpenedFile();

  base::FilePath file_path;
  OpenFileMode mode;
};

// Map from a file handle to an OpenedFile struct.
typedef std::map<int, OpenedFile> OpenedFiles;

class ScopedUserInteraction {
 public:
  virtual ~ScopedUserInteraction();
  ScopedUserInteraction(const ScopedUserInteraction&) = delete;
  ScopedUserInteraction& operator=(const ScopedUserInteraction&) = delete;
  ScopedUserInteraction(ScopedUserInteraction&&);
  ScopedUserInteraction& operator=(ScopedUserInteraction&&);

 protected:
  ScopedUserInteraction();
};

// Interface for a provided file system. Acts as a proxy between providers
// and clients. All of the request methods return an abort callback in order to
// terminate it while running. They must be called on the same thread as the
// request methods. The cancellation callback may be null if the operation
// fails synchronously. It must not be called once the operation is completed
// with either a success or an error.
class ProvidedFileSystemInterface {
 public:
  // Fields to be fetched with metadata.
  enum MetadataField {
    METADATA_FIELD_NONE = 0,
    METADATA_FIELD_IS_DIRECTORY = 1 << 0,
    METADATA_FIELD_NAME = 1 << 1,
    METADATA_FIELD_SIZE = 1 << 2,
    METADATA_FIELD_MODIFICATION_TIME = 1 << 3,
    METADATA_FIELD_MIME_TYPE = 1 << 4,
    METADATA_FIELD_THUMBNAIL = 1 << 5,
    METADATA_FIELD_CLOUD_IDENTIFIER = 1 << 6,
    METADATA_FIELD_CLOUD_FILE_INFO = 1 << 7
  };

  // Callback for OpenFile(). In case of an error, file_handle is equal to 0
  // and result is set to an error code.
  typedef base::OnceCallback<void(int file_handle,
                                  base::File::Error result,
                                  std::unique_ptr<EntryMetadata> metadata)>
      OpenFileCallback;

  typedef base::RepeatingCallback<
      void(int chunk_length, bool has_more, base::File::Error result)>
      ReadChunkReceivedCallback;

  typedef base::OnceCallback<void(std::unique_ptr<EntryMetadata> entry_metadata,
                                  base::File::Error result)>
      GetMetadataCallback;

  typedef base::OnceCallback<void(const Actions& actions,
                                  base::File::Error result)>
      GetActionsCallback;

  // Mask of fields requested from the GetMetadata() call.
  typedef int MetadataFieldMask;

  virtual ~ProvidedFileSystemInterface() = default;

  // Requests unmounting of the file system. The callback is called when the
  // request is accepted or rejected, with an error code.
  virtual AbortCallback RequestUnmount(
      storage::AsyncFileUtil::StatusCallback callback) = 0;

  // Requests metadata of the passed |entry_path|. It can be either a file
  // or a directory. All |fields| will be returned if supported. Note, that
  // default fields are always returned.
  virtual AbortCallback GetMetadata(const base::FilePath& entry_path,
                                    MetadataFieldMask fields,
                                    GetMetadataCallback callback) = 0;

  // Requests list of actions for the passed list of entries at |entry_paths|.
  // They can be either files or directories.
  virtual AbortCallback GetActions(
      const std::vector<base::FilePath>& entry_paths,
      GetActionsCallback callback) = 0;

  // Executes the |action_id| action on the list of entries at |entry_paths|.
  virtual AbortCallback ExecuteAction(
      const std::vector<base::FilePath>& entry_paths,
      const std::string& action_id,
      storage::AsyncFileUtil::StatusCallback callback) = 0;

  // Requests enumerating entries from the passed |directory_path|. The callback
  // can be called multiple times until |has_more| is set to false.
  virtual AbortCallback ReadDirectory(
      const base::FilePath& directory_path,
      storage::AsyncFileUtil::ReadDirectoryCallback callback) = 0;

  // Requests opening a file at |file_path|. If the file doesn't exist, then the
  // operation will fail. In case of any error, the returned file handle is 0.
  virtual AbortCallback OpenFile(const base::FilePath& file_path,
                                 OpenFileMode mode,
                                 OpenFileCallback callback) = 0;

  // Requests closing a file, previously opened with OpenFile() as a file with
  // |file_handle|. The |callback| must be called.
  virtual AbortCallback CloseFile(
      int file_handle,
      storage::AsyncFileUtil::StatusCallback callback) = 0;

  // Requests reading a file previously opened with |file_handle|. The callback
  // can be called multiple times until |has_more| is set to false. On success
  // it should return |length| bytes starting from |offset| in total. It can
  // return less only in case EOF is encountered.
  virtual AbortCallback ReadFile(int file_handle,
                                 net::IOBuffer* buffer,
                                 int64_t offset,
                                 int length,
                                 ReadChunkReceivedCallback callback) = 0;

  // Requests creating a directory. If |recursive| is passed, then all non
  // existing directories on the path will be created. The operation will fail
  // if the target directory already exists.
  virtual AbortCallback CreateDirectory(
      const base::FilePath& directory_path,
      bool recursive,
      storage::AsyncFileUtil::StatusCallback callback) = 0;

  // Requests creating a file. If the entry already exists, then the
  // FILE_ERROR_EXISTS error must be returned.
  virtual AbortCallback CreateFile(
      const base::FilePath& file_path,
      storage::AsyncFileUtil::StatusCallback callback) = 0;

  // Requests deleting a directory. If |recursive| is passed and the entry is
  // a directory, then all contents of it (recursively) will be deleted too.
  virtual AbortCallback DeleteEntry(
      const base::FilePath& entry_path,
      bool recursive,
      storage::AsyncFileUtil::StatusCallback callback) = 0;

  // Requests copying an entry (recursively in case of a directory) within the
  // same file system.
  virtual AbortCallback CopyEntry(
      const base::FilePath& source_path,
      const base::FilePath& target_path,
      storage::AsyncFileUtil::StatusCallback callback) = 0;

  // Requests moving an entry (recursively in case of a directory) within the
  // same file system.
  virtual AbortCallback MoveEntry(
      const base::FilePath& source_path,
      const base::FilePath& target_path,
      storage::AsyncFileUtil::StatusCallback callback) = 0;

  // Requests truncating a file to the desired length.
  virtual AbortCallback Truncate(
      const base::FilePath& file_path,
      int64_t length,
      storage::AsyncFileUtil::StatusCallback callback) = 0;

  // Requests writing to a file previously opened with |file_handle|.
  virtual AbortCallback WriteFile(
      int file_handle,
      net::IOBuffer* buffer,
      int64_t offset,
      int length,
      storage::AsyncFileUtil::StatusCallback callback) = 0;

  // Requests flushing data written to a file previously opened with
  // `file_handle`. This is currently only called after the last write
  // operation.
  virtual AbortCallback FlushFile(
      int file_handle,
      storage::AsyncFileUtil::StatusCallback callback) = 0;

  // Requests adding a watcher on an entry. |recursive| must not be true for
  // files. |callback| is optional, but it can't be used for persistent
  // watchers.
  virtual AbortCallback AddWatcher(
      const GURL& origin,
      const base::FilePath& entry_path,
      bool recursive,
      bool persistent,
      storage::AsyncFileUtil::StatusCallback callback,
      storage::WatcherManager::NotificationCallback notification_callback) = 0;

  // Requests removing a watcher, which is immediately deleted from the internal
  // list, hence the operation is not abortable.
  virtual void RemoveWatcher(
      const GURL& origin,
      const base::FilePath& entry_path,
      bool recursive,
      storage::AsyncFileUtil::StatusCallback callback) = 0;

  // Notifies about changes related to the watcher within the file system.
  // Invoked by the file system implementation. Returns an error code via the
  // callback if the notification arguments are malformed or the entry is not
  // watched anymore. On success, returns base::File::FILE_OK.
  // TODO(mtomasz): Replace [entry_path, recursive] with a watcher id.
  virtual void Notify(
      const base::FilePath& entry_path,
      bool recursive,
      storage::WatcherManager::ChangeType change_type,
      std::unique_ptr<ProvidedFileSystemObserver::Changes> changes,
      const std::string& tag,
      storage::AsyncFileUtil::StatusCallback callback) = 0;

  // Requests showing UI for configuring the file system by user. Once the
  // configuration process is completed, base::File::FILE_OK or an error code is
  // returned via the |callback|.
  virtual void Configure(storage::AsyncFileUtil::StatusCallback callback) = 0;

  // Returns a provided file system info for this file system.
  virtual const ProvidedFileSystemInfo& GetFileSystemInfo() const = 0;

  // Returns a mutable list of watchers.
  virtual Watchers* GetWatchers() = 0;

  // Returns a list of opened files.
  virtual const OpenedFiles& GetOpenedFiles() const = 0;

  // Returns a request manager for the file system.
  virtual OperationRequestManager* GetRequestManager() = 0;

  // Adds an observer on the file system.
  virtual void AddObserver(ProvidedFileSystemObserver* observer) = 0;

  // Removes an observer.
  virtual void RemoveObserver(ProvidedFileSystemObserver* observer) = 0;

  // Returns a weak pointer to this object.
  virtual base::WeakPtr<ProvidedFileSystemInterface> GetWeakPtr() = 0;

  // Starts a user interaction with the file system, during which "unresponsive
  // operation" notifications won't be created.
  virtual std::unique_ptr<ScopedUserInteraction> StartUserInteraction() = 0;
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_PROVIDED_FILE_SYSTEM_INTERFACE_H_
