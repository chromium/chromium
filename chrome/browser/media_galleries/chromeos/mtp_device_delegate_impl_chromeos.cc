// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/chromeos/mtp_device_delegate_impl_chromeos.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/circular_deque.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/media_galleries/chromeos/mtp_device_task_helper_map_service.h"
#include "chrome/browser/media_galleries/chromeos/snapshot_file_details.h"
#include "components/services/filesystem/public/interfaces/types.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "net/base/io_buffer.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

// File path separator constant.
const char kRootPath[] = "/";

// Helper function to create |MTPDeviceDelegateImplLinux::storage_name_|.
std::string CreateStorageName(const std::string& device_location) {
  std::string storage_name;
  base::RemoveChars(device_location, kRootPath, &storage_name);
  return storage_name;
}

// Returns the device relative file path given |file_path|.
// E.g.: If the |file_path| is "/usb:2,2:12345/DCIM" and |registered_dev_path|
// is "/usb:2,2:12345", this function returns the device relative path which is
// "DCIM".
// In the special case when |registered_dev_path| and |file_path| are the same,
// return |kRootPath|.
std::string GetDeviceRelativePath(const base::FilePath& registered_dev_path,
                                  const base::FilePath& file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!registered_dev_path.empty());
  DCHECK(!file_path.empty());
  std::string result;
  if (registered_dev_path == file_path) {
    result = kRootPath;
  } else {
    base::FilePath relative_path;
    if (registered_dev_path.AppendRelativePath(file_path, &relative_path)) {
      DCHECK(!relative_path.empty());
      result = relative_path.value();
    }
  }
  return result;
}

// Returns the MTPDeviceTaskHelper object associated with the MTP device
// storage.
//
// |storage_name| specifies the name of the storage device.
// |read_only| specifies the mode of the storage device.
// Returns NULL if the |storage_name| is no longer valid (e.g. because the
// corresponding storage device is detached, etc).
MTPDeviceTaskHelper* GetDeviceTaskHelperForStorage(
    const std::string& storage_name,
    const bool read_only) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return MTPDeviceTaskHelperMapService::GetInstance()->GetDeviceTaskHelper(
      storage_name,
      read_only);
}

// Opens the storage device for communication.
//
// Called on the UI thread to dispatch the request to the MTPDeviceTaskHelper.
//
// |storage_name| specifies the name of the storage device.
// |read_only| specifies the mode of the storage device.
// |reply_callback| is called when the OpenStorage request completes.
// |reply_callback| runs on the IO thread.
void OpenStorageOnUIThread(
    const std::string& storage_name,
    const bool read_only,
    const MTPDeviceTaskHelper::OpenStorageCallback& reply_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name, read_only);
  if (!task_helper) {
    task_helper =
        MTPDeviceTaskHelperMapService::GetInstance()->CreateDeviceTaskHelper(
            storage_name, read_only);
  }
  task_helper->OpenStorage(storage_name, read_only, reply_callback);
}

// Creates |directory_name| on |parent_id|.
//
// |storage_name| specifies the name of the storage device.
// |read_only| specifies the mode of the storage device.
// |parent_id| is an object id of the parent directory.
// |directory_name| is name of the new directory.
// |success_callback| is called when the directory is created successfully.
// |error_callback| is called when it fails to create a directory.
// |success_callback| and |error_callback| runs on the IO thread.
void CreateDirectoryOnUIThread(
    const std::string& storage_name,
    const bool read_only,
    const uint32_t parent_id,
    const std::string& directory_name,
    const MTPDeviceTaskHelper::CreateDirectorySuccessCallback& success_callback,
    const MTPDeviceTaskHelper::ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name, read_only);
  if (!task_helper)
    return;
  task_helper->CreateDirectory(parent_id, directory_name, success_callback,
                               error_callback);
}

// Enumerates the |directory_id| directory file entries.
//
// Called on the UI thread to dispatch the request to the MTPDeviceTaskHelper.
//
// |storage_name| specifies the name of the storage device.
// |read_only| specifies the mode of the storage device.
// |directory_id| is an id of a directory to read.
// |success_callback| is called when the ReadDirectory request succeeds.
// |error_callback| is called when the ReadDirectory request fails.
// |success_callback| and |error_callback| runs on the IO thread.
void ReadDirectoryOnUIThread(
    const std::string& storage_name,
    const bool read_only,
    const uint32_t directory_id,
    const MTPDeviceTaskHelper::ReadDirectorySuccessCallback& success_callback,
    const MTPDeviceTaskHelper::ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name, read_only);
  if (!task_helper)
    return;
  task_helper->ReadDirectory(directory_id, success_callback, error_callback);
}

// Checks if the |directory_id| directory is empty.
//
// Called on the UI thread to dispatch the request to the MTPDeviceTaskHelper.
//
// |storage_name| specifies the name of the storage device.
// |read_only| specifies the mode of the storage device.
// |directory_id| is an id of a directory to check.
// |success_callback| is called when the CheckDirectoryEmpty request succeeds.
// |error_callback| is called when the CheckDirectoryEmpty request fails.
// |success_callback| and |error_callback| runs on the IO thread.
void CheckDirectoryEmptyOnUIThread(
    const std::string& storage_name,
    bool read_only,
    uint32_t directory_id,
    MTPDeviceTaskHelper::CheckDirectoryEmptySuccessCallback success_callback,
    const MTPDeviceTaskHelper::ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name, read_only);
  if (!task_helper)
    return;
  task_helper->CheckDirectoryEmpty(directory_id, std::move(success_callback),
                                   error_callback);
}

// Gets the |file_path| details.
//
// Called on the UI thread to dispatch the request to the MTPDeviceTaskHelper.
//
// |storage_name| specifies the name of the storage device.
// |read_only| specifies the mode of the storage device.
// |success_callback| is called when the GetFileInfo request succeeds.
// |error_callback| is called when the GetFileInfo request fails.
// |success_callback| and |error_callback| runs on the IO thread.
void GetFileInfoOnUIThread(
    const std::string& storage_name,
    const bool read_only,
    uint32_t file_id,
    const MTPDeviceTaskHelper::GetFileInfoSuccessCallback& success_callback,
    const MTPDeviceTaskHelper::ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name, read_only);
  if (!task_helper)
    return;
  task_helper->GetFileInfo(file_id, success_callback, error_callback);
}

// Copies the contents of |device_file_path| to |snapshot_file_path|.
//
// Called on the UI thread to dispatch the request to the MTPDeviceTaskHelper.
//
// |storage_name| specifies the name of the storage device.
// |read_only| specifies the mode of the storage device.
// |device_file_path| specifies the media device file path.
// |snapshot_file_path| specifies the platform path of the snapshot file.
// |file_size| specifies the number of bytes that will be written to the
// snapshot file.
// |success_callback| is called when the copy operation succeeds.
// |error_callback| is called when the copy operation fails.
// |success_callback| and |error_callback| runs on the IO thread.
void WriteDataIntoSnapshotFileOnUIThread(
    const std::string& storage_name,
    const bool read_only,
    const SnapshotRequestInfo& request_info,
    const base::File::Info& snapshot_file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name, read_only);
  if (!task_helper)
    return;
  task_helper->WriteDataIntoSnapshotFile(request_info, snapshot_file_info);
}

// Copies the contents of |device_file_path| to |snapshot_file_path|.
//
// Called on the UI thread to dispatch the request to the MTPDeviceTaskHelper.
//
// |storage_name| specifies the name of the storage device.
// |read_only| specifies the mode of the storage device.
// |request| is a struct containing details about the byte read request.
void ReadBytesOnUIThread(
    const std::string& storage_name,
    const bool read_only,
    const MTPDeviceAsyncDelegate::ReadBytesRequest& request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name, read_only);
  if (!task_helper)
    return;
  task_helper->ReadBytes(request);
}

// Renames |object_id| to |new_name|.
//
// |storage_name| specifies the name of the storage device.
// |read_only| specifies the mode of the storage device.
// |object_id| is an id of object to be renamed.
// |new_name| is new name of the object.
// |success_callback| is called when the object is renamed successfully.
// |error_callback| is called when it fails to rename the object.
// |success_callback| and |error_callback| runs on the IO thread.
void RenameObjectOnUIThread(
    const std::string& storage_name,
    const bool read_only,
    const uint32_t object_id,
    const std::string& new_name,
    const MTPDeviceTaskHelper::RenameObjectSuccessCallback& success_callback,
    const MTPDeviceTaskHelper::ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name, read_only);
  if (!task_helper)
    return;
  task_helper->RenameObject(object_id, new_name, success_callback,
                            error_callback);
}

// Copies the file |source_file_descriptor| to |file_name| in |parent_id|.
//
// |storage_name| specifies the name of the storage device.
// |read_only| specifies the mode of the storage device.
// |source_file_descriptor| file descriptor of source file.
// |parent_id| object id of a target directory.
// |file_name| file name of a target file.
// |success_callback| is called when the file is copied successfully.
// |error_callback| is called when it fails to copy file.
// Since this method does not close the file descriptor, callbacks are
// responsible for closing it.
void CopyFileFromLocalOnUIThread(
    const std::string& storage_name,
    const bool read_only,
    const int source_file_descriptor,
    const uint32_t parent_id,
    const std::string& file_name,
    const MTPDeviceTaskHelper::CopyFileFromLocalSuccessCallback&
        success_callback,
    const MTPDeviceTaskHelper::ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name, read_only);
  if (!task_helper)
    return;
  task_helper->CopyFileFromLocal(storage_name, source_file_descriptor,
                                 parent_id, file_name, success_callback,
                                 error_callback);
}

// Deletes |object_id|.
//
// Called on the UI thread to dispatch the request to the MTPDeviceTaskHelper.
//
// |storage_name| specifies the name of the storage device.
// |read_only| specifies the mode of the storage device.
// |object_id| is the object to be deleted.
// |success_callback| is called when the object is deleted successfully.
// |error_callback| is called when it fails to delete the object.
// |success_callback| and |error_callback| runs on the IO thread.
void DeleteObjectOnUIThread(
    const std::string storage_name,
    const bool read_only,
    const uint32_t object_id,
    const MTPDeviceTaskHelper::DeleteObjectSuccessCallback success_callback,
    const MTPDeviceTaskHelper::ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name, read_only);
  if (!task_helper)
    return;
  task_helper->DeleteObject(object_id, success_callback, error_callback);
}

// Closes the device storage specified by the |storage_name| and destroys the
// MTPDeviceTaskHelper object associated with the device storage.
//
// Called on the UI thread to dispatch the request to the MTPDeviceTaskHelper.
void CloseStorageAndDestroyTaskHelperOnUIThread(
    const std::string& storage_name,
    const bool read_only) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name, read_only);
  if (!task_helper)
    return;
  task_helper->CloseStorage();
  MTPDeviceTaskHelperMapService::GetInstance()->DestroyDeviceTaskHelper(
      storage_name, read_only);
}

// Opens |file_path| with |flags|. Returns the result as a pair.
// first is file descriptor.
// second is base::File::Error. This value is set as following.
// - When it succeeds to open a file descriptor, base::File::FILE_OK is set.
// - When |file_path| is a directory, base::File::FILE_ERROR_NOT_A_FILE is set.
// - When |file_path| does not exist, base::File::FILE_ERROR_NOT_FOUND is set.
// - For other error cases, base::File::FILE_ERROR_FAILED is set.
std::pair<int, base::File::Error> OpenFileDescriptor(
    const base::FilePath& file_path,
    const int flags) {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);

  if (base::DirectoryExists(file_path))
    return std::make_pair(-1, base::File::FILE_ERROR_NOT_A_FILE);
  int file_descriptor = open(file_path.value().c_str(), flags);
  if (file_descriptor >= 0)
    return std::make_pair(file_descriptor, base::File::FILE_OK);
  if (errno == ENOENT)
    return std::make_pair(file_descriptor, base::File::FILE_ERROR_NOT_FOUND);
  return std::make_pair(file_descriptor, base::File::FILE_ERROR_FAILED);
}

// Closes |file_descriptor| on a background task runner.
void CloseFileDescriptor(const int file_descriptor) {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::MAY_BLOCK);

  IGNORE_EINTR(close(file_descriptor));
}

// Deletes a temporary file |file_path|.
void DeleteTemporaryFile(const base::FilePath& file_path) {
  base::PostTaskWithTraits(FROM_HERE,
                           {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
                           base::Bind(base::IgnoreResult(base::DeleteFile),
                                      file_path, false /* not recursive*/));
}

// A fake callback to be passed as CopyFileProgressCallback.
void FakeCopyFileProgressCallback(int64_t size) {}

}  // namespace

MTPDeviceDelegateImplLinux::PendingTaskInfo::PendingTaskInfo(
    const base::FilePath& path,
    content::BrowserThread::ID thread_id,
    const base::Location& location,
    const base::Closure& task)
    : path(path), thread_id(thread_id), location(location), task(task) {}

MTPDeviceDelegateImplLinux::PendingTaskInfo::PendingTaskInfo(
    const PendingTaskInfo& other) = default;

MTPDeviceDelegateImplLinux::PendingTaskInfo::~PendingTaskInfo() {
}

// Represents a file on the MTP device.
// Lives on the IO thread.
class MTPDeviceDelegateImplLinux::MTPFileNode {
 public:
  MTPFileNode(uint32_t file_id,
              const std::string& file_name,
              MTPFileNode* parent,
              FileIdToMTPFileNodeMap* file_id_to_node_map);
  ~MTPFileNode();

  const MTPFileNode* GetChild(const std::string& name) const;

  void EnsureChildExists(const std::string& name, uint32_t id);

  // Clears all the children, except those in |children_to_keep|.
  void ClearNonexistentChildren(
      const std::set<std::string>& children_to_keep);

  bool DeleteChild(uint32_t file_id);

  bool HasChildren() const;

  uint32_t file_id() const { return file_id_; }
  const std::string& file_name() const { return file_name_; }
  MTPFileNode* parent() { return parent_; }

 private:
  // Container for holding a node's children.
  using ChildNodes =
      std::unordered_map<std::string, std::unique_ptr<MTPFileNode>>;

  const uint32_t file_id_;
  const std::string file_name_;

  ChildNodes children_;
  MTPFileNode* const parent_;
  FileIdToMTPFileNodeMap* file_id_to_node_map_;

  DISALLOW_COPY_AND_ASSIGN(MTPFileNode);
};

MTPDeviceDelegateImplLinux::MTPFileNode::MTPFileNode(
    uint32_t file_id,
    const std::string& file_name,
    MTPFileNode* parent,
    FileIdToMTPFileNodeMap* file_id_to_node_map)
    : file_id_(file_id),
      file_name_(file_name),
      parent_(parent),
      file_id_to_node_map_(file_id_to_node_map) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(file_id_to_node_map_);
  DCHECK(!base::ContainsKey(*file_id_to_node_map_, file_id_));
  (*file_id_to_node_map_)[file_id_] = this;
}

MTPDeviceDelegateImplLinux::MTPFileNode::~MTPFileNode() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  size_t erased = file_id_to_node_map_->erase(file_id_);
  DCHECK_EQ(1U, erased);
}

const MTPDeviceDelegateImplLinux::MTPFileNode*
MTPDeviceDelegateImplLinux::MTPFileNode::GetChild(
    const std::string& name) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  auto it = children_.find(name);
  if (it == children_.end())
    return nullptr;
  return it->second.get();
}

void MTPDeviceDelegateImplLinux::MTPFileNode::EnsureChildExists(
    const std::string& name,
    uint32_t id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  const MTPFileNode* child = GetChild(name);
  if (child && child->file_id() == id)
    return;

  children_[name] =
      std::make_unique<MTPFileNode>(id, name, this, file_id_to_node_map_);
}

void MTPDeviceDelegateImplLinux::MTPFileNode::ClearNonexistentChildren(
    const std::set<std::string>& children_to_keep) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::vector<std::string> children_to_erase;
  for (const auto& child : children_) {
    if (base::ContainsKey(children_to_keep, child.first))
      continue;
    children_to_erase.push_back(child.first);
  }
  for (const auto& child : children_to_erase)
    children_.erase(child);
}

bool MTPDeviceDelegateImplLinux::MTPFileNode::DeleteChild(uint32_t file_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  for (auto it = children_.begin(); it != children_.end(); ++it) {
    if (it->second->file_id() == file_id) {
      DCHECK(!it->second->HasChildren());
      children_.erase(it);
      return true;
    }
  }
  return false;
}

bool MTPDeviceDelegateImplLinux::MTPFileNode::HasChildren() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return !children_.empty();
}

MTPDeviceDelegateImplLinux::MTPDeviceDelegateImplLinux(
    const std::string& device_location,
    const bool read_only)
    : device_path_(device_location),
      storage_name_(CreateStorageName(device_location)),
      read_only_(read_only),
      root_node_(std::make_unique<MTPFileNode>(mtpd::kRootFileId,
                                               "",  // Root node has no name.
                                               nullptr,  // And no parent node.
                                               &file_id_to_node_map_)),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!device_path_.empty());
  DCHECK(!storage_name_.empty());
}

MTPDeviceDelegateImplLinux::~MTPDeviceDelegateImplLinux() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void MTPDeviceDelegateImplLinux::CreateDirectory(
    const base::FilePath& directory_path,
    const bool exclusive,
    const bool recursive,
    const CreateDirectorySuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!directory_path.empty());

  // If |directory_path| is not the path in this device, fails with error.
  if (!device_path_.IsParent(directory_path)) {
    error_callback.Run(base::File::FILE_ERROR_FAILED);
    return;
  }

  // Decomposes |directory_path| to components. CreateDirectoryInternal creates
  // directories by reading |components| from back.
  std::vector<base::FilePath> components;
  if (recursive) {
    for (base::FilePath path = directory_path; path != device_path_;
         path = path.DirName()) {
      components.push_back(path);
    }
  } else {
    components.push_back(directory_path);
  }

  const base::Closure closure =
      base::Bind(&MTPDeviceDelegateImplLinux::CreateDirectoryInternal,
                 weak_ptr_factory_.GetWeakPtr(), components, exclusive,
                 success_callback, error_callback);
  EnsureInitAndRunTask(PendingTaskInfo(
      directory_path, content::BrowserThread::IO, FROM_HERE, closure));
}

void MTPDeviceDelegateImplLinux::GetFileInfo(
    const base::FilePath& file_path,
    const GetFileInfoSuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!file_path.empty());

  // If a ReadDirectory operation is in progress, the file info may already be
  // cached.
  FileInfoCache::const_iterator it = file_info_cache_.find(file_path);
  if (it != file_info_cache_.end()) {
    // TODO(thestig): This code is repeated in several places. Combine them.
    // e.g. c/b/media_galleries/win/mtp_device_operations_util.cc
    const MTPDeviceTaskHelper::MTPEntry& cached_file_entry = it->second;
    success_callback.Run(cached_file_entry.file_info);
    return;
  }
  base::Closure closure =
      base::Bind(&MTPDeviceDelegateImplLinux::GetFileInfoInternal,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_path,
                 success_callback,
                 error_callback);
  EnsureInitAndRunTask(PendingTaskInfo(file_path,
                                       content::BrowserThread::IO,
                                       FROM_HERE,
                                       closure));
}

void MTPDeviceDelegateImplLinux::ReadDirectory(
    const base::FilePath& root,
    const ReadDirectorySuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!root.empty());
  base::Closure closure =
      base::Bind(&MTPDeviceDelegateImplLinux::ReadDirectoryInternal,
                 weak_ptr_factory_.GetWeakPtr(),
                 root,
                 success_callback,
                 error_callback);
  EnsureInitAndRunTask(PendingTaskInfo(root,
                                       content::BrowserThread::IO,
                                       FROM_HERE,
                                       closure));
}

void MTPDeviceDelegateImplLinux::CreateSnapshotFile(
    const base::FilePath& device_file_path,
    const base::FilePath& local_path,
    const CreateSnapshotFileSuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!device_file_path.empty());
  DCHECK(!local_path.empty());
  base::Closure closure =
      base::Bind(&MTPDeviceDelegateImplLinux::CreateSnapshotFileInternal,
                 weak_ptr_factory_.GetWeakPtr(),
                 device_file_path,
                 local_path,
                 success_callback,
                 error_callback);
  EnsureInitAndRunTask(PendingTaskInfo(device_file_path,
                                       content::BrowserThread::IO,
                                       FROM_HERE,
                                       closure));
}

bool MTPDeviceDelegateImplLinux::IsStreaming() {
  return true;
}

void MTPDeviceDelegateImplLinux::ReadBytes(
    const base::FilePath& device_file_path,
    const scoped_refptr<net::IOBuffer>& buf,
    int64_t offset,
    int buf_len,
    const ReadBytesSuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!device_file_path.empty());
  base::Closure closure = base::Bind(
      &MTPDeviceDelegateImplLinux::ReadBytesInternal,
      weak_ptr_factory_.GetWeakPtr(), device_file_path, base::RetainedRef(buf),
      offset, buf_len, success_callback, error_callback);
  EnsureInitAndRunTask(PendingTaskInfo(device_file_path,
                                       content::BrowserThread::IO,
                                       FROM_HERE,
                                       closure));
}

bool MTPDeviceDelegateImplLinux::IsReadOnly() const {
  return read_only_;
}

void MTPDeviceDelegateImplLinux::CopyFileLocal(
    const base::FilePath& source_file_path,
    const base::FilePath& device_file_path,
    const CreateTemporaryFileCallback& create_temporary_file_callback,
    const CopyFileProgressCallback& progress_callback,
    const CopyFileLocalSuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!source_file_path.empty());
  DCHECK(!device_file_path.empty());

  // Create a temporary file for creating a copy of source file on local.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      create_temporary_file_callback,
      base::Bind(
          &MTPDeviceDelegateImplLinux::OnDidCreateTemporaryFileToCopyFileLocal,
          weak_ptr_factory_.GetWeakPtr(), source_file_path, device_file_path,
          progress_callback, success_callback, error_callback));
}

void MTPDeviceDelegateImplLinux::MoveFileLocal(
    const base::FilePath& source_file_path,
    const base::FilePath& device_file_path,
    const CreateTemporaryFileCallback& create_temporary_file_callback,
    const MoveFileLocalSuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!source_file_path.empty());
  DCHECK(!device_file_path.empty());

  // Get file info to move file on local.
  const GetFileInfoSuccessCallback success_callback_wrapper = base::Bind(
      &MTPDeviceDelegateImplLinux::MoveFileLocalInternal,
      weak_ptr_factory_.GetWeakPtr(), source_file_path, device_file_path,
      create_temporary_file_callback, success_callback, error_callback);
  const base::Closure closure =
      base::Bind(&MTPDeviceDelegateImplLinux::GetFileInfoInternal,
                 weak_ptr_factory_.GetWeakPtr(), source_file_path,
                 success_callback_wrapper, error_callback);
  EnsureInitAndRunTask(PendingTaskInfo(
      source_file_path, content::BrowserThread::IO, FROM_HERE, closure));
}

void MTPDeviceDelegateImplLinux::CopyFileFromLocal(
    const base::FilePath& source_file_path,
    const base::FilePath& device_file_path,
    const CopyFileFromLocalSuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!source_file_path.empty());
  DCHECK(!device_file_path.empty());

  // Get file info of destination file path.
  const GetFileInfoSuccessCallback success_callback_wrapper = base::Bind(
      &MTPDeviceDelegateImplLinux::OnDidGetDestFileInfoToCopyFileFromLocal,
      weak_ptr_factory_.GetWeakPtr(), error_callback);
  const ErrorCallback error_callback_wrapper = base::Bind(
      &MTPDeviceDelegateImplLinux::OnGetDestFileInfoErrorToCopyFileFromLocal,
      weak_ptr_factory_.GetWeakPtr(), source_file_path, device_file_path,
      success_callback, error_callback);
  const base::Closure closure =
      base::Bind(&MTPDeviceDelegateImplLinux::GetFileInfoInternal,
                 weak_ptr_factory_.GetWeakPtr(), device_file_path,
                 success_callback_wrapper, error_callback_wrapper);
  EnsureInitAndRunTask(PendingTaskInfo(
      device_file_path, content::BrowserThread::IO, FROM_HERE, closure));
}

void MTPDeviceDelegateImplLinux::DeleteFile(
    const base::FilePath& file_path,
    const DeleteFileSuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!file_path.empty());

  const GetFileInfoSuccessCallback& success_callback_wrapper =
      base::Bind(&MTPDeviceDelegateImplLinux::DeleteFileInternal,
                 weak_ptr_factory_.GetWeakPtr(), file_path, success_callback,
                 error_callback);

  const base::Closure closure =
      base::Bind(&MTPDeviceDelegateImplLinux::GetFileInfoInternal,
                 weak_ptr_factory_.GetWeakPtr(), file_path,
                 success_callback_wrapper, error_callback);
  EnsureInitAndRunTask(PendingTaskInfo(file_path, content::BrowserThread::IO,
                                       FROM_HERE, closure));
}

void MTPDeviceDelegateImplLinux::DeleteDirectory(
    const base::FilePath& file_path,
    const DeleteDirectorySuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!file_path.empty());

  const GetFileInfoSuccessCallback& success_callback_wrapper =
      base::Bind(&MTPDeviceDelegateImplLinux::DeleteDirectoryInternal,
                 weak_ptr_factory_.GetWeakPtr(), file_path, success_callback,
                 error_callback);

  const base::Closure closure =
      base::Bind(&MTPDeviceDelegateImplLinux::GetFileInfoInternal,
                 weak_ptr_factory_.GetWeakPtr(), file_path,
                 success_callback_wrapper, error_callback);
  EnsureInitAndRunTask(PendingTaskInfo(file_path, content::BrowserThread::IO,
                                       FROM_HERE, closure));
}

void MTPDeviceDelegateImplLinux::AddWatcher(
    const GURL& origin,
    const base::FilePath& file_path,
    const bool recursive,
    const storage::WatcherManager::StatusCallback& callback,
    const storage::WatcherManager::NotificationCallback&
        notification_callback) {
  if (recursive) {
    callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  const auto it = subscribers_.find(file_path);
  if (it != subscribers_.end()) {
    // Adds to existing origin callback map.
    if (ContainsKey(it->second, origin)) {
      callback.Run(base::File::FILE_ERROR_EXISTS);
      return;
    }

    it->second.insert(std::make_pair(origin, notification_callback));
  } else {
    // Creates new origin callback map.
    OriginNotificationCallbackMap callback_map;
    callback_map.insert(std::make_pair(origin, notification_callback));
    subscribers_.insert(std::make_pair(file_path, callback_map));
  }

  callback.Run(base::File::FILE_OK);
}

void MTPDeviceDelegateImplLinux::RemoveWatcher(
    const GURL& origin,
    const base::FilePath& file_path,
    const bool recursive,
    const storage::WatcherManager::StatusCallback& callback) {
  if (recursive) {
    callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  const auto it = subscribers_.find(file_path);
  if (it == subscribers_.end()) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  if (it->second.erase(origin) == 0) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  if (it->second.empty())
    subscribers_.erase(it);

  callback.Run(base::File::FILE_OK);
}

void MTPDeviceDelegateImplLinux::NotifyFileChange(
    const base::FilePath& file_path,
    const storage::WatcherManager::ChangeType change_type) {
  const auto it = subscribers_.find(file_path);
  if (it != subscribers_.end()) {
    for (const auto& origin_callback : it->second) {
      origin_callback.second.Run(change_type);
    }
  }
}

void MTPDeviceDelegateImplLinux::CancelPendingTasksAndDeleteDelegate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // To cancel all the pending tasks, destroy the MTPDeviceTaskHelper object.
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::Bind(&CloseStorageAndDestroyTaskHelperOnUIThread, storage_name_,
                 read_only_));
  delete this;
}

void MTPDeviceDelegateImplLinux::GetFileInfoInternal(
    const base::FilePath& file_path,
    const GetFileInfoSuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  base::Optional<uint32_t> file_id = CachedPathToId(file_path);
  if (file_id) {
    GetFileInfoSuccessCallback success_callback_wrapper =
        base::Bind(&MTPDeviceDelegateImplLinux::OnDidGetFileInfo,
                   weak_ptr_factory_.GetWeakPtr(),
                   success_callback);
    ErrorCallback error_callback_wrapper =
        base::Bind(&MTPDeviceDelegateImplLinux::HandleDeviceFileError,
                   weak_ptr_factory_.GetWeakPtr(), error_callback, *file_id);

    base::Closure closure =
        base::Bind(&GetFileInfoOnUIThread, storage_name_, read_only_, *file_id,
                   success_callback_wrapper, error_callback_wrapper);
    EnsureInitAndRunTask(PendingTaskInfo(base::FilePath(),
                                         content::BrowserThread::UI,
                                         FROM_HERE,
                                         closure));
  } else {
    error_callback.Run(base::File::FILE_ERROR_NOT_FOUND);
  }
  PendingRequestDone();
}

void MTPDeviceDelegateImplLinux::CreateDirectoryInternal(
    const std::vector<base::FilePath>& components,
    const bool exclusive,
    const CreateDirectorySuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  const base::FilePath current_component = components.back();
  std::vector<base::FilePath> other_components = components;
  other_components.pop_back();

  if (other_components.empty()) {
    // Either we reached the last component in the recursive case, or this is
    // the non-recursive case.
    base::Optional<uint32_t> parent_id =
        CachedPathToId(current_component.DirName());
    if (parent_id) {
      const base::Closure closure =
          base::Bind(&MTPDeviceDelegateImplLinux::CreateSingleDirectory,
                     weak_ptr_factory_.GetWeakPtr(), current_component,
                     exclusive, success_callback, error_callback);
      EnsureInitAndRunTask(PendingTaskInfo(
          base::FilePath(), content::BrowserThread::IO, FROM_HERE, closure));
    } else {
      error_callback.Run(base::File::FILE_ERROR_NOT_FOUND);
    }
  } else {
    // Ensures that parent directories are created for recursive case.
    base::Optional<uint32_t> directory_id = CachedPathToId(current_component);
    if (directory_id) {
      // Parent directory |current_component| already exists, continue creating
      // directories.
      const base::Closure closure =
          base::Bind(&MTPDeviceDelegateImplLinux::CreateDirectoryInternal,
                     weak_ptr_factory_.GetWeakPtr(), other_components,
                     exclusive, success_callback, error_callback);
      EnsureInitAndRunTask(PendingTaskInfo(
          base::FilePath(), content::BrowserThread::IO, FROM_HERE, closure));
    } else {
      // If parent directory |current_component| does not exist, create it.
      const CreateDirectorySuccessCallback success_callback_wrapper =
          base::Bind(&MTPDeviceDelegateImplLinux::
                         OnDidCreateParentDirectoryToCreateDirectory,
                     weak_ptr_factory_.GetWeakPtr(), current_component,
                     other_components, exclusive, success_callback,
                     error_callback);
      // Wraps error callback to return all errors of creating parent
      // directories as FILE_ERROR_FAILED.
      const ErrorCallback error_callback_wrapper =
          base::Bind(&MTPDeviceDelegateImplLinux::
                         OnCreateParentDirectoryErrorToCreateDirectory,
                     weak_ptr_factory_.GetWeakPtr(), error_callback);
      const base::Closure closure =
          base::Bind(&MTPDeviceDelegateImplLinux::CreateSingleDirectory,
                     weak_ptr_factory_.GetWeakPtr(), current_component,
                     false /* not exclusive */, success_callback_wrapper,
                     error_callback_wrapper);
      EnsureInitAndRunTask(PendingTaskInfo(
          base::FilePath(), content::BrowserThread::IO, FROM_HERE, closure));
    }
  }

  PendingRequestDone();
}

void MTPDeviceDelegateImplLinux::ReadDirectoryInternal(
    const base::FilePath& root,
    const ReadDirectorySuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(task_in_progress_);

  base::Optional<uint32_t> dir_id = CachedPathToId(root);
  if (!dir_id) {
    error_callback.Run(base::File::FILE_ERROR_NOT_FOUND);
    PendingRequestDone();
    return;
  }

  GetFileInfoSuccessCallback success_callback_wrapper =
      base::Bind(&MTPDeviceDelegateImplLinux::OnDidGetFileInfoToReadDirectory,
                 weak_ptr_factory_.GetWeakPtr(), *dir_id, success_callback,
                 error_callback);
  ErrorCallback error_callback_wrapper =
      base::Bind(&MTPDeviceDelegateImplLinux::HandleDeviceFileError,
                 weak_ptr_factory_.GetWeakPtr(), error_callback, *dir_id);
  base::Closure closure =
      base::Bind(&GetFileInfoOnUIThread, storage_name_, read_only_, *dir_id,
                 success_callback_wrapper, error_callback_wrapper);

  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI}, closure);
}

void MTPDeviceDelegateImplLinux::CreateSnapshotFileInternal(
    const base::FilePath& device_file_path,
    const base::FilePath& local_path,
    const CreateSnapshotFileSuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  base::Optional<uint32_t> file_id = CachedPathToId(device_file_path);
  if (file_id) {
    auto request_info = std::make_unique<SnapshotRequestInfo>(
        *file_id, local_path, success_callback, error_callback);
    GetFileInfoSuccessCallback success_callback_wrapper =
        base::Bind(
            &MTPDeviceDelegateImplLinux::OnDidGetFileInfoToCreateSnapshotFile,
            weak_ptr_factory_.GetWeakPtr(),
            base::Passed(&request_info));
    ErrorCallback error_callback_wrapper =
        base::Bind(&MTPDeviceDelegateImplLinux::HandleDeviceFileError,
                   weak_ptr_factory_.GetWeakPtr(), error_callback, *file_id);
    base::Closure closure =
        base::Bind(&GetFileInfoOnUIThread, storage_name_, read_only_, *file_id,
                   success_callback_wrapper, error_callback_wrapper);
    EnsureInitAndRunTask(PendingTaskInfo(base::FilePath(),
                                         content::BrowserThread::UI,
                                         FROM_HERE,
                                         closure));
  } else {
    error_callback.Run(base::File::FILE_ERROR_NOT_FOUND);
  }
  PendingRequestDone();
}

void MTPDeviceDelegateImplLinux::ReadBytesInternal(
    const base::FilePath& device_file_path,
    net::IOBuffer* buf,
    int64_t offset,
    int buf_len,
    const ReadBytesSuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  base::Optional<uint32_t> file_id = CachedPathToId(device_file_path);
  if (file_id) {
    ReadBytesRequest request(
        *file_id, buf, offset, buf_len,
        base::Bind(&MTPDeviceDelegateImplLinux::OnDidReadBytes,
                   weak_ptr_factory_.GetWeakPtr(), success_callback),
        base::Bind(&MTPDeviceDelegateImplLinux::HandleDeviceFileError,
                   weak_ptr_factory_.GetWeakPtr(), error_callback, *file_id));

    base::Closure closure =
        base::Bind(&ReadBytesOnUIThread, storage_name_, read_only_, request);
    EnsureInitAndRunTask(PendingTaskInfo(base::FilePath(),
                                         content::BrowserThread::UI,
                                         FROM_HERE,
                                         closure));
  } else {
    error_callback.Run(base::File::FILE_ERROR_NOT_FOUND);
  }
  PendingRequestDone();
}

void MTPDeviceDelegateImplLinux::MoveFileLocalInternal(
    const base::FilePath& source_file_path,
    const base::FilePath& device_file_path,
    const CreateTemporaryFileCallback& create_temporary_file_callback,
    const MoveFileLocalSuccessCallback& success_callback,
    const ErrorCallback& error_callback,
    const base::File::Info& source_file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (source_file_info.is_directory) {
    error_callback.Run(base::File::FILE_ERROR_NOT_A_FILE);
    return;
  }

  if (source_file_path.DirName() == device_file_path.DirName()) {
    // If a file is moved in a same directory, rename the file.
    base::Optional<uint32_t> file_id = CachedPathToId(source_file_path);
    if (file_id) {
      const MTPDeviceTaskHelper::RenameObjectSuccessCallback
          success_callback_wrapper = base::Bind(
              &MTPDeviceDelegateImplLinux::OnDidMoveFileLocalWithRename,
              weak_ptr_factory_.GetWeakPtr(), success_callback,
              source_file_path, *file_id);
      const MTPDeviceTaskHelper::ErrorCallback error_callback_wrapper =
          base::Bind(&MTPDeviceDelegateImplLinux::HandleDeviceFileError,
                     weak_ptr_factory_.GetWeakPtr(), error_callback, *file_id);
      const base::Closure closure =
          base::Bind(&RenameObjectOnUIThread, storage_name_, read_only_,
                     *file_id, device_file_path.BaseName().value(),
                     success_callback_wrapper, error_callback_wrapper);
      EnsureInitAndRunTask(PendingTaskInfo(
          base::FilePath(), content::BrowserThread::UI, FROM_HERE, closure));
    } else {
      error_callback.Run(base::File::FILE_ERROR_NOT_FOUND);
    }
    return;
  }

  // If a file is moved to a different directory, create a copy to the
  // destination path, and remove source file.
  const CopyFileLocalSuccessCallback& success_callback_wrapper =
      base::Bind(&MTPDeviceDelegateImplLinux::DeleteFileInternal,
                 weak_ptr_factory_.GetWeakPtr(), source_file_path,
                 success_callback, error_callback, source_file_info);
  // TODO(yawano): Avoid to call external method from internal code.
  CopyFileLocal(source_file_path, device_file_path,
                create_temporary_file_callback,
                base::Bind(&FakeCopyFileProgressCallback),
                success_callback_wrapper, error_callback);
}

void MTPDeviceDelegateImplLinux::OnDidOpenFDToCopyFileFromLocal(
    const base::FilePath& device_file_path,
    const CopyFileFromLocalSuccessCallback& success_callback,
    const ErrorCallback& error_callback,
    const std::pair<int, base::File::Error>& open_fd_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (open_fd_result.second != base::File::FILE_OK) {
    error_callback.Run(open_fd_result.second);
    return;
  }

  const int source_file_descriptor = open_fd_result.first;
  base::Optional<uint32_t> parent_id =
      CachedPathToId(device_file_path.DirName());
  if (!parent_id) {
    HandleCopyFileFromLocalError(error_callback, source_file_descriptor,
                                 base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  CopyFileFromLocalSuccessCallback success_callback_wrapper =
      base::Bind(&MTPDeviceDelegateImplLinux::OnDidCopyFileFromLocal,
                 weak_ptr_factory_.GetWeakPtr(), success_callback,
                 device_file_path, source_file_descriptor);

  ErrorCallback error_callback_wrapper = base::Bind(
      &MTPDeviceDelegateImplLinux::HandleCopyFileFromLocalError,
      weak_ptr_factory_.GetWeakPtr(), error_callback, source_file_descriptor);

  base::Closure closure = base::Bind(
      &CopyFileFromLocalOnUIThread, storage_name_, read_only_,
      source_file_descriptor, *parent_id, device_file_path.BaseName().value(),
      success_callback_wrapper, error_callback_wrapper);

  EnsureInitAndRunTask(PendingTaskInfo(
      base::FilePath(), content::BrowserThread::UI, FROM_HERE, closure));
}

void MTPDeviceDelegateImplLinux::DeleteFileInternal(
    const base::FilePath& file_path,
    const DeleteFileSuccessCallback& success_callback,
    const ErrorCallback& error_callback,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (file_info.is_directory) {
    error_callback.Run(base::File::FILE_ERROR_NOT_A_FILE);
    return;
  }

  base::Optional<uint32_t> file_id = CachedPathToId(file_path);
  if (!file_id) {
    error_callback.Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  RunDeleteObjectOnUIThread(file_path, *file_id, success_callback,
                            error_callback);
}

void MTPDeviceDelegateImplLinux::DeleteDirectoryInternal(
    const base::FilePath& file_path,
    const DeleteDirectorySuccessCallback& success_callback,
    const ErrorCallback& error_callback,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!file_info.is_directory) {
    error_callback.Run(base::File::FILE_ERROR_NOT_A_DIRECTORY);
    return;
  }

  base::Optional<uint32_t> directory_id = CachedPathToId(file_path);
  if (!directory_id) {
    error_callback.Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  // Checks the cache first. If it has children in cache, the directory cannot
  // be empty.
  FileIdToMTPFileNodeMap::const_iterator it =
      file_id_to_node_map_.find(*directory_id);
  if (it != file_id_to_node_map_.end() && it->second->HasChildren()) {
    error_callback.Run(base::File::FILE_ERROR_NOT_EMPTY);
    return;
  }

  // Since the directory can contain a file even if the cache returns it as
  // empty, explicitly check the directory and confirm it is actually empty.
  MTPDeviceTaskHelper::CheckDirectoryEmptySuccessCallback
      success_callback_wrapper =
          base::BindOnce(&MTPDeviceDelegateImplLinux::
                             OnDidCheckDirectoryEmptyToDeleteDirectory,
                         weak_ptr_factory_.GetWeakPtr(), file_path,
                         *directory_id, success_callback, error_callback);
  const MTPDeviceTaskHelper::ErrorCallback error_callback_wrapper =
      base::Bind(&MTPDeviceDelegateImplLinux::HandleDeviceFileError,
                 weak_ptr_factory_.GetWeakPtr(), error_callback, *directory_id);
  const base::Closure closure = base::Bind(
      &CheckDirectoryEmptyOnUIThread, storage_name_, read_only_, *directory_id,
      base::Passed(&success_callback_wrapper), error_callback_wrapper);
  EnsureInitAndRunTask(PendingTaskInfo(
      base::FilePath(), content::BrowserThread::UI, FROM_HERE, closure));
}

void MTPDeviceDelegateImplLinux::CreateSingleDirectory(
    const base::FilePath& directory_path,
    const bool exclusive,
    const CreateDirectorySuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  const GetFileInfoSuccessCallback success_callback_wrapper = base::Bind(
      &MTPDeviceDelegateImplLinux::OnPathAlreadyExistsForCreateSingleDirectory,
      weak_ptr_factory_.GetWeakPtr(), exclusive, success_callback,
      error_callback);
  const ErrorCallback error_callback_wrapper = base::Bind(
      &MTPDeviceDelegateImplLinux::OnPathDoesNotExistForCreateSingleDirectory,
      weak_ptr_factory_.GetWeakPtr(), directory_path, success_callback,
      error_callback);
  const base::Closure closure =
      base::Bind(&MTPDeviceDelegateImplLinux::GetFileInfoInternal,
                 weak_ptr_factory_.GetWeakPtr(), directory_path,
                 success_callback_wrapper, error_callback_wrapper);
  EnsureInitAndRunTask(PendingTaskInfo(
      base::FilePath(), content::BrowserThread::IO, FROM_HERE, closure));
  PendingRequestDone();
}

void MTPDeviceDelegateImplLinux::OnDidReadDirectoryToCreateDirectory(
    const std::vector<base::FilePath>& components,
    const bool exclusive,
    const CreateDirectorySuccessCallback& success_callback,
    const ErrorCallback& error_callback,
    storage::AsyncFileUtil::EntryList /* entries */,
    const bool has_more) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (has_more)
    return;  // Wait until all entries have been read.

  const base::Closure closure =
      base::Bind(&MTPDeviceDelegateImplLinux::CreateDirectoryInternal,
                 weak_ptr_factory_.GetWeakPtr(), components, exclusive,
                 success_callback, error_callback);
  EnsureInitAndRunTask(PendingTaskInfo(
      base::FilePath(), content::BrowserThread::IO, FROM_HERE, closure));
}

void MTPDeviceDelegateImplLinux::OnDidCheckDirectoryEmptyToDeleteDirectory(
    const base::FilePath& directory_path,
    uint32_t directory_id,
    const DeleteDirectorySuccessCallback& success_callback,
    const ErrorCallback& error_callback,
    bool is_empty) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (is_empty) {
    RunDeleteObjectOnUIThread(directory_path, directory_id, success_callback,
                              error_callback);
  } else {
    error_callback.Run(base::File::FILE_ERROR_NOT_EMPTY);
  }

  PendingRequestDone();
}

void MTPDeviceDelegateImplLinux::RunDeleteObjectOnUIThread(
    const base::FilePath& object_path,
    const uint32_t object_id,
    const DeleteObjectSuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  const MTPDeviceTaskHelper::DeleteObjectSuccessCallback
      success_callback_wrapper =
          base::Bind(&MTPDeviceDelegateImplLinux::OnDidDeleteObject,
                     weak_ptr_factory_.GetWeakPtr(), object_path, object_id,
                     success_callback);

  const MTPDeviceTaskHelper::ErrorCallback error_callback_wrapper =
      base::Bind(&MTPDeviceDelegateImplLinux::HandleDeleteFileOrDirectoryError,
                 weak_ptr_factory_.GetWeakPtr(), error_callback);

  const base::Closure closure =
      base::Bind(&DeleteObjectOnUIThread, storage_name_, read_only_, object_id,
                 success_callback_wrapper, error_callback_wrapper);
  EnsureInitAndRunTask(PendingTaskInfo(
      base::FilePath(), content::BrowserThread::UI, FROM_HERE, closure));
}

void MTPDeviceDelegateImplLinux::EnsureInitAndRunTask(
    const PendingTaskInfo& task_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if ((init_state_ == INITIALIZED) && !task_in_progress_) {
    RunTask(task_info);
    return;
  }

  // Only *Internal functions have empty paths. Since they are the continuation
  // of the current running task, they get to cut in line.
  if (task_info.path.empty())
    pending_tasks_.push_front(task_info);
  else
    pending_tasks_.push_back(task_info);

  if (init_state_ == UNINITIALIZED) {
    init_state_ = PENDING_INIT;
    task_in_progress_ = true;
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::Bind(&OpenStorageOnUIThread, storage_name_, read_only_,
                   base::Bind(&MTPDeviceDelegateImplLinux::OnInitCompleted,
                              weak_ptr_factory_.GetWeakPtr())));
  }
}

void MTPDeviceDelegateImplLinux::RunTask(const PendingTaskInfo& task_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK_EQ(INITIALIZED, init_state_);
  DCHECK(!task_in_progress_);
  task_in_progress_ = true;

  bool need_to_check_cache = !task_info.path.empty();
  if (need_to_check_cache) {
    base::FilePath uncached_path =
        NextUncachedPathComponent(task_info.path, task_info.cached_path);
    if (!uncached_path.empty()) {
      // Save the current task and do a cache lookup first.
      pending_tasks_.push_front(task_info);
      FillFileCache(uncached_path);
      return;
    }
  }

  base::PostTaskWithTraits(task_info.location, {task_info.thread_id},
                           task_info.task);
}

void MTPDeviceDelegateImplLinux::WriteDataIntoSnapshotFile(
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(current_snapshot_request_info_.get());
  DCHECK_GT(file_info.size, 0);
  DCHECK(task_in_progress_);
  SnapshotRequestInfo request_info(
      current_snapshot_request_info_->file_id,
      current_snapshot_request_info_->snapshot_file_path,
      base::Bind(
          &MTPDeviceDelegateImplLinux::OnDidWriteDataIntoSnapshotFile,
          weak_ptr_factory_.GetWeakPtr()),
      base::Bind(
          &MTPDeviceDelegateImplLinux::OnWriteDataIntoSnapshotFileError,
          weak_ptr_factory_.GetWeakPtr()));

  base::Closure task_closure = base::Bind(&WriteDataIntoSnapshotFileOnUIThread,
                                          storage_name_,
                                          read_only_,
                                          request_info,
                                          file_info);
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           task_closure);
}

void MTPDeviceDelegateImplLinux::PendingRequestDone() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(task_in_progress_);
  task_in_progress_ = false;
  ProcessNextPendingRequest();
}

void MTPDeviceDelegateImplLinux::ProcessNextPendingRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!task_in_progress_);
  if (pending_tasks_.empty())
    return;

  PendingTaskInfo task_info = pending_tasks_.front();
  pending_tasks_.pop_front();
  RunTask(task_info);
}

void MTPDeviceDelegateImplLinux::OnInitCompleted(bool succeeded) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  init_state_ = succeeded ? INITIALIZED : UNINITIALIZED;
  PendingRequestDone();
}

void MTPDeviceDelegateImplLinux::OnDidGetFileInfo(
    const GetFileInfoSuccessCallback& success_callback,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  success_callback.Run(file_info);
  PendingRequestDone();
}

void MTPDeviceDelegateImplLinux::OnPathAlreadyExistsForCreateSingleDirectory(
    const bool exclusive,
    const CreateDirectorySuccessCallback& success_callback,
    const ErrorCallback& error_callback,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!file_info.is_directory || exclusive)
    error_callback.Run(base::File::FILE_ERROR_EXISTS);
  else
    success_callback.Run();
}

void MTPDeviceDelegateImplLinux::OnPathDoesNotExistForCreateSingleDirectory(
    const base::FilePath& directory_path,
    const CreateDirectorySuccessCallback& success_callback,
    const ErrorCallback& error_callback,
    const base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (error != base::File::FILE_ERROR_NOT_FOUND) {
    error_callback.Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  base::Optional<uint32_t> parent_id = CachedPathToId(directory_path.DirName());
  if (!parent_id) {
    error_callback.Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  const MTPDeviceTaskHelper::CreateDirectorySuccessCallback
      success_callback_wrapper = base::Bind(
          &MTPDeviceDelegateImplLinux::OnDidCreateSingleDirectory,
          weak_ptr_factory_.GetWeakPtr(), directory_path, success_callback);
  const MTPDeviceTaskHelper::ErrorCallback error_callback_wrapper =
      base::Bind(&MTPDeviceDelegateImplLinux::HandleDeviceFileError,
                 weak_ptr_factory_.GetWeakPtr(), error_callback, *parent_id);
  const base::Closure closure =
      base::Bind(&CreateDirectoryOnUIThread, storage_name_, read_only_,
                 *parent_id, directory_path.BaseName().value(),
                 success_callback_wrapper, error_callback_wrapper);
  EnsureInitAndRunTask(PendingTaskInfo(
      base::FilePath(), content::BrowserThread::UI, FROM_HERE, closure));
}

void MTPDeviceDelegateImplLinux::OnDidGetFileInfoToReadDirectory(
    uint32_t dir_id,
    const ReadDirectorySuccessCallback& success_callback,
    const ErrorCallback& error_callback,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(task_in_progress_);
  if (!file_info.is_directory) {
    return HandleDeviceFileError(error_callback,
                                 dir_id,
                                 base::File::FILE_ERROR_NOT_A_DIRECTORY);
  }

  base::Closure task_closure = base::Bind(
      &ReadDirectoryOnUIThread, storage_name_, read_only_, dir_id,
      base::Bind(&MTPDeviceDelegateImplLinux::OnDidReadDirectory,
                 weak_ptr_factory_.GetWeakPtr(), dir_id, success_callback),
      base::Bind(&MTPDeviceDelegateImplLinux::HandleDeviceFileError,
                 weak_ptr_factory_.GetWeakPtr(), error_callback, dir_id));
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           task_closure);
}

void MTPDeviceDelegateImplLinux::OnDidGetFileInfoToCreateSnapshotFile(
    std::unique_ptr<SnapshotRequestInfo> snapshot_request_info,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!current_snapshot_request_info_.get());
  DCHECK(snapshot_request_info.get());
  DCHECK(task_in_progress_);
  base::File::Error error = base::File::FILE_OK;
  if (file_info.is_directory)
    error = base::File::FILE_ERROR_NOT_A_FILE;
  else if (file_info.size < 0 ||
           file_info.size > std::numeric_limits<uint32_t>::max())
    error = base::File::FILE_ERROR_FAILED;

  if (error != base::File::FILE_OK)
    return HandleDeviceFileError(snapshot_request_info->error_callback,
                                 snapshot_request_info->file_id,
                                 error);

  base::File::Info snapshot_file_info(file_info);
  // Modify the last modified time to null. This prevents the time stamp
  // verfication in LocalFileStreamReader.
  snapshot_file_info.last_modified = base::Time();

  current_snapshot_request_info_ = std::move(snapshot_request_info);
  if (file_info.size == 0) {
    // Empty snapshot file.
    return OnDidWriteDataIntoSnapshotFile(
        snapshot_file_info, current_snapshot_request_info_->snapshot_file_path);
  }
  WriteDataIntoSnapshotFile(snapshot_file_info);
}

void MTPDeviceDelegateImplLinux::OnDidGetDestFileInfoToCopyFileFromLocal(
    const ErrorCallback& error_callback,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (file_info.is_directory)
    error_callback.Run(base::File::FILE_ERROR_INVALID_OPERATION);
  else
    error_callback.Run(base::File::FILE_ERROR_FAILED);
}

void MTPDeviceDelegateImplLinux::OnGetDestFileInfoErrorToCopyFileFromLocal(
    const base::FilePath& source_file_path,
    const base::FilePath& device_file_path,
    const CopyFileFromLocalSuccessCallback& success_callback,
    const ErrorCallback& error_callback,
    const base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (error != base::File::FILE_ERROR_NOT_FOUND) {
    error_callback.Run(error);
    return;
  }

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::Bind(&OpenFileDescriptor, source_file_path, O_RDONLY),
      base::Bind(&MTPDeviceDelegateImplLinux::OnDidOpenFDToCopyFileFromLocal,
                 weak_ptr_factory_.GetWeakPtr(), device_file_path,
                 success_callback, error_callback));
}

void MTPDeviceDelegateImplLinux::OnDidCreateSingleDirectory(
    const base::FilePath& directory_path,
    const CreateDirectorySuccessCallback& success_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  success_callback.Run();
  NotifyFileChange(directory_path.DirName(),
                   storage::WatcherManager::ChangeType::CHANGED);
  PendingRequestDone();
}

void MTPDeviceDelegateImplLinux::OnDidCreateParentDirectoryToCreateDirectory(
    const base::FilePath& created_directory,
    const std::vector<base::FilePath>& components,
    const bool exclusive,
    const CreateDirectorySuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // Calls ReadDirectoryInternal to fill the cache for created directory.
  // Calls ReadDirectoryInternal in this method to call it via
  // EnsureInitAndRunTask.
  const ReadDirectorySuccessCallback& success_callback_wrapper = base::Bind(
      &MTPDeviceDelegateImplLinux::OnDidReadDirectoryToCreateDirectory,
      weak_ptr_factory_.GetWeakPtr(), components, exclusive, success_callback,
      error_callback);
  const base::Closure closure =
      base::Bind(&MTPDeviceDelegateImplLinux::ReadDirectoryInternal,
                 weak_ptr_factory_.GetWeakPtr(), created_directory.DirName(),
                 success_callback_wrapper, error_callback);
  EnsureInitAndRunTask(PendingTaskInfo(
      base::FilePath(), content::BrowserThread::IO, FROM_HERE, closure));
}

void MTPDeviceDelegateImplLinux::OnCreateParentDirectoryErrorToCreateDirectory(
    const ErrorCallback& callback,
    const base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  callback.Run(base::File::FILE_ERROR_FAILED);
}

void MTPDeviceDelegateImplLinux::OnDidReadDirectory(
    uint32_t dir_id,
    const ReadDirectorySuccessCallback& success_callback,
    const MTPDeviceTaskHelper::MTPEntries& mtp_entries,
    bool has_more) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  FileIdToMTPFileNodeMap::iterator it = file_id_to_node_map_.find(dir_id);
  DCHECK(it != file_id_to_node_map_.end());
  MTPFileNode* dir_node = it->second;

  // Traverse the MTPFileNode tree to reconstuct the full path for |dir_id|.
  base::circular_deque<std::string> dir_path_parts;
  MTPFileNode* parent_node = dir_node;
  while (parent_node->parent()) {
    dir_path_parts.push_front(parent_node->file_name());
    parent_node = parent_node->parent();
  }
  base::FilePath dir_path = device_path_;
  for (const auto& dir_path_part : dir_path_parts)
    dir_path = dir_path.Append(dir_path_part);

  storage::AsyncFileUtil::EntryList file_list;
  for (const auto& mtp_entry : mtp_entries) {
    filesystem::mojom::DirectoryEntry entry;
    entry.name = base::FilePath(mtp_entry.name);
    entry.type = mtp_entry.file_info.is_directory
                     ? filesystem::mojom::FsFileType::DIRECTORY
                     : filesystem::mojom::FsFileType::REGULAR_FILE;
    file_list.push_back(entry);

    // Refresh the in memory tree.
    dir_node->EnsureChildExists(entry.name.value(), mtp_entry.file_id);
    child_nodes_seen_.insert(entry.name.value());

    // Add to |file_info_cache_|.
    file_info_cache_[dir_path.Append(entry.name)] = mtp_entry;
  }

  success_callback.Run(file_list, has_more);
  if (has_more)
    return;  // Wait to be called again.

  // Last call, finish book keeping and continue with the next request.
  dir_node->ClearNonexistentChildren(child_nodes_seen_);
  child_nodes_seen_.clear();
  file_info_cache_.clear();

  PendingRequestDone();
}

void MTPDeviceDelegateImplLinux::OnDidWriteDataIntoSnapshotFile(
    const base::File::Info& file_info,
    const base::FilePath& snapshot_file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(current_snapshot_request_info_.get());
  current_snapshot_request_info_->success_callback.Run(
      file_info, snapshot_file_path);
  current_snapshot_request_info_.reset();
  PendingRequestDone();
}

void MTPDeviceDelegateImplLinux::OnWriteDataIntoSnapshotFileError(
    base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(current_snapshot_request_info_.get());
  current_snapshot_request_info_->error_callback.Run(error);
  current_snapshot_request_info_.reset();
  PendingRequestDone();
}

void MTPDeviceDelegateImplLinux::OnDidReadBytes(
    const ReadBytesSuccessCallback& success_callback,
    const base::File::Info& file_info, int bytes_read) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  success_callback.Run(file_info, bytes_read);
  PendingRequestDone();
}

void MTPDeviceDelegateImplLinux::OnDidFillFileCache(
    const base::FilePath& path,
    storage::AsyncFileUtil::EntryList /* entries */,
    bool has_more) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(path.IsParent(pending_tasks_.front().path));
  if (has_more)
    return;  // Wait until all entries have been read.
  pending_tasks_.front().cached_path = path;
}

void MTPDeviceDelegateImplLinux::OnFillFileCacheFailed(
    base::File::Error /* error */) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // When filling the cache fails for the task at the front of the queue, clear
  // the path of the task so it will not try to do any more caching. Instead,
  // the task will just run and fail the CachedPathToId() lookup.
  pending_tasks_.front().path.clear();
}

void MTPDeviceDelegateImplLinux::OnDidCreateTemporaryFileToCopyFileLocal(
    const base::FilePath& source_file_path,
    const base::FilePath& device_file_path,
    const CopyFileProgressCallback& progress_callback,
    const CopyFileLocalSuccessCallback& success_callback,
    const ErrorCallback& error_callback,
    const base::FilePath& temporary_file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (temporary_file_path.empty()) {
    error_callback.Run(base::File::FILE_ERROR_FAILED);
    return;
  }

  CreateSnapshotFile(
      source_file_path, temporary_file_path,
      base::Bind(
          &MTPDeviceDelegateImplLinux::OnDidCreateSnapshotFileOfCopyFileLocal,
          weak_ptr_factory_.GetWeakPtr(), device_file_path, progress_callback,
          success_callback, error_callback),
      base::Bind(&MTPDeviceDelegateImplLinux::HandleCopyFileLocalError,
                 weak_ptr_factory_.GetWeakPtr(), error_callback,
                 temporary_file_path));
}

void MTPDeviceDelegateImplLinux::OnDidCreateSnapshotFileOfCopyFileLocal(
    const base::FilePath& device_file_path,
    const CopyFileProgressCallback& progress_callback,
    const CopyFileLocalSuccessCallback& success_callback,
    const ErrorCallback& error_callback,
    const base::File::Info& file_info,
    const base::FilePath& temporary_file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // Consider that half of copy is completed by creating a temporary file.
  progress_callback.Run(file_info.size / 2);

  // TODO(yawano): Avoid to call external method from internal code.
  CopyFileFromLocal(
      temporary_file_path, device_file_path,
      base::Bind(
          &MTPDeviceDelegateImplLinux::OnDidCopyFileFromLocalOfCopyFileLocal,
          weak_ptr_factory_.GetWeakPtr(), success_callback,
          temporary_file_path),
      base::Bind(&MTPDeviceDelegateImplLinux::HandleCopyFileLocalError,
                 weak_ptr_factory_.GetWeakPtr(), error_callback,
                 temporary_file_path));
}

void MTPDeviceDelegateImplLinux::OnDidCopyFileFromLocalOfCopyFileLocal(
    const CopyFileFromLocalSuccessCallback success_callback,
    const base::FilePath& temporary_file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  DeleteTemporaryFile(temporary_file_path);
  success_callback.Run();
}

void MTPDeviceDelegateImplLinux::OnDidMoveFileLocalWithRename(
    const MoveFileLocalSuccessCallback& success_callback,
    const base::FilePath& source_file_path,
    const uint32_t file_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  EvictCachedPathToId(file_id);
  success_callback.Run();
  NotifyFileChange(source_file_path,
                   storage::WatcherManager::ChangeType::DELETED);
  NotifyFileChange(source_file_path.DirName(),
                   storage::WatcherManager::ChangeType::CHANGED);
  PendingRequestDone();
}

void MTPDeviceDelegateImplLinux::OnDidCopyFileFromLocal(
    const CopyFileFromLocalSuccessCallback& success_callback,
    const base::FilePath& file_path,
    const int source_file_descriptor) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  const base::Closure closure = base::Bind(&CloseFileDescriptor,
                                           source_file_descriptor);

  base::PostTaskWithTraits(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT}, closure);

  success_callback.Run();
  NotifyFileChange(file_path.DirName(),
                   storage::WatcherManager::ChangeType::CHANGED);
  PendingRequestDone();
}

void MTPDeviceDelegateImplLinux::HandleCopyFileLocalError(
    const ErrorCallback& error_callback,
    const base::FilePath& temporary_file_path,
    const base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  DeleteTemporaryFile(temporary_file_path);
  error_callback.Run(error);
}

void MTPDeviceDelegateImplLinux::HandleCopyFileFromLocalError(
    const ErrorCallback& error_callback,
    const int source_file_descriptor,
    base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  const base::Closure closure = base::Bind(&CloseFileDescriptor,
                                           source_file_descriptor);

  base::PostTaskWithTraits(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT}, closure);

  error_callback.Run(error);
  PendingRequestDone();
}

void MTPDeviceDelegateImplLinux::OnDidDeleteObject(
    const base::FilePath& object_path,
    const uint32_t object_id,
    const DeleteObjectSuccessCallback success_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  EvictCachedPathToId(object_id);
  success_callback.Run();
  NotifyFileChange(object_path, storage::WatcherManager::ChangeType::DELETED);
  NotifyFileChange(object_path.DirName(),
                   storage::WatcherManager::ChangeType::CHANGED);
  PendingRequestDone();
}

void MTPDeviceDelegateImplLinux::HandleDeleteFileOrDirectoryError(
    const ErrorCallback& error_callback,
    base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  error_callback.Run(error);
  PendingRequestDone();
}

void MTPDeviceDelegateImplLinux::HandleDeviceFileError(
    const ErrorCallback& error_callback,
    uint32_t file_id,
    base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  EvictCachedPathToId(file_id);
  error_callback.Run(error);
  PendingRequestDone();
}

base::FilePath MTPDeviceDelegateImplLinux::NextUncachedPathComponent(
    const base::FilePath& path,
    const base::FilePath& cached_path) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(cached_path.empty() || cached_path.IsParent(path));

  base::FilePath uncached_path;
  std::string device_relpath = GetDeviceRelativePath(device_path_, path);
  if (!device_relpath.empty() && device_relpath != kRootPath) {
    uncached_path = device_path_;
    std::vector<std::string> device_relpath_components = base::SplitString(
        device_relpath, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    DCHECK(!device_relpath_components.empty());
    bool all_components_cached = true;
    const MTPFileNode* current_node = root_node_.get();
    for (const std::string& component : device_relpath_components) {
      current_node = current_node->GetChild(component);
      if (!current_node) {
        // With a cache miss, check if it is a genuine failure. If so, pretend
        // the entire |path| is cached, so there is no further attempt to do
        // more caching. The actual operation will then fail.
        all_components_cached =
            !cached_path.empty() && (uncached_path == cached_path);
        break;
      }
      uncached_path = uncached_path.Append(component);
    }
    if (all_components_cached)
      uncached_path.clear();
  }
  return uncached_path;
}

void MTPDeviceDelegateImplLinux::FillFileCache(
    const base::FilePath& uncached_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(task_in_progress_);

  ReadDirectorySuccessCallback success_callback =
      base::Bind(&MTPDeviceDelegateImplLinux::OnDidFillFileCache,
                 weak_ptr_factory_.GetWeakPtr(),
                 uncached_path);
  ErrorCallback error_callback =
      base::Bind(&MTPDeviceDelegateImplLinux::OnFillFileCacheFailed,
                 weak_ptr_factory_.GetWeakPtr());
  ReadDirectoryInternal(uncached_path, success_callback, error_callback);
}

base::Optional<uint32_t> MTPDeviceDelegateImplLinux::CachedPathToId(
    const base::FilePath& path) const {
  std::string device_relpath = GetDeviceRelativePath(device_path_, path);
  if (device_relpath.empty())
    return {};
  std::vector<std::string> device_relpath_components;
  if (device_relpath != kRootPath) {
    device_relpath_components = base::SplitString(
        device_relpath, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  }
  const MTPFileNode* current_node = root_node_.get();
  for (const std::string& component : device_relpath_components) {
    current_node = current_node->GetChild(component);
    if (!current_node)
      return {};
  }
  return current_node->file_id();
}

void MTPDeviceDelegateImplLinux::EvictCachedPathToId(uint32_t id) {
  FileIdToMTPFileNodeMap::iterator it = file_id_to_node_map_.find(id);
  if (it == file_id_to_node_map_.end())
    return;

  DCHECK(!it->second->HasChildren());
  MTPFileNode* parent = it->second->parent();
  if (parent) {
    bool ret = parent->DeleteChild(id);
    DCHECK(ret);
  }
}

void CreateMTPDeviceAsyncDelegate(
    const std::string& device_location,
    const bool read_only,
    const CreateMTPDeviceAsyncDelegateCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  callback.Run(new MTPDeviceDelegateImplLinux(device_location, read_only));
}
