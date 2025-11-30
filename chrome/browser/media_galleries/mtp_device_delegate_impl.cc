// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/mtp_device_delegate_impl.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>

#include <algorithm>
#include <limits>
#include <ostream>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/safe_base_name.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/media_galleries/mtp_device_task_helper_map_service.h"
#include "chrome/browser/media_galleries/snapshot_file_details.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

using enum base::File::Error;

void PostError(MTPDeviceTaskHelper::ErrorCallback callback,
               std::string_view storage_name) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  LOG(ERROR) << "No MTP device task helper for '" << storage_name << "'";
  if (callback) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), FILE_ERROR_FAILED));
  }
}

// File path separator constant.
const char kRootPath[] = "/";

// Helper function to create |MTPDeviceDelegateImpl::storage_name_|.
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
      storage_name, read_only);
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
    MTPDeviceTaskHelper::OpenStorageCallback reply_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name, read_only);
  if (!task_helper) {
    task_helper =
        MTPDeviceTaskHelperMapService::GetInstance()->CreateDeviceTaskHelper(
            storage_name, read_only);
  }
  task_helper->OpenStorage(storage_name, read_only, std::move(reply_callback));
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
    MTPDeviceTaskHelper::CreateDirectorySuccessCallback success_callback,
    MTPDeviceTaskHelper::ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name, read_only);
  if (!task_helper) {
    return PostError(std::move(error_callback), storage_name);
  }
  task_helper->CreateDirectory(parent_id, directory_name,
                               std::move(success_callback),
                               std::move(error_callback));
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
    MTPDeviceTaskHelper::ReadDirectorySuccessCallback success_callback,
    MTPDeviceTaskHelper::ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name, read_only);
  if (!task_helper) {
    return PostError(std::move(error_callback), storage_name);
  }
  task_helper->ReadDirectory(directory_id, success_callback,
                             std::move(error_callback));
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
    MTPDeviceTaskHelper::ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name, read_only);
  if (!task_helper) {
    return PostError(std::move(error_callback), storage_name);
  }
  task_helper->CheckDirectoryEmpty(directory_id, std::move(success_callback),
                                   std::move(error_callback));
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
    MTPDeviceTaskHelper::GetFileInfoSuccessCallback success_callback,
    MTPDeviceTaskHelper::ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name, read_only);
  if (!task_helper) {
    return PostError(std::move(error_callback), storage_name);
  }
  task_helper->GetFileInfo(file_id, std::move(success_callback),
                           std::move(error_callback));
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
    SnapshotRequestInfo request_info,
    const base::File::Info& snapshot_file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name, read_only);
  if (!task_helper) {
    return PostError(std::move(request_info.error_callback), storage_name);
  }
  task_helper->WriteDataIntoSnapshotFile(std::move(request_info),
                                         snapshot_file_info);
}

// Copies the contents of |device_file_path| to |snapshot_file_path|.
//
// Called on the UI thread to dispatch the request to the MTPDeviceTaskHelper.
//
// |storage_name| specifies the name of the storage device.
// |read_only| specifies the mode of the storage device.
// |request| is a struct containing details about the byte read request.
void ReadBytesOnUIThread(const std::string& storage_name,
                         const bool read_only,
                         MTPDeviceAsyncDelegate::ReadBytesRequest request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name, read_only);
  if (!task_helper) {
    return PostError(std::move(request.error_callback), storage_name);
  }
  task_helper->ReadBytes(std::move(request));
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
    MTPDeviceTaskHelper::RenameObjectSuccessCallback success_callback,
    MTPDeviceTaskHelper::ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name, read_only);
  if (!task_helper) {
    return PostError(std::move(error_callback), storage_name);
  }
  task_helper->RenameObject(object_id, new_name, std::move(success_callback),
                            std::move(error_callback));
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
    MTPDeviceTaskHelper::CopyFileFromLocalSuccessCallback success_callback,
    MTPDeviceTaskHelper::ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name, read_only);
  if (!task_helper) {
    return PostError(std::move(error_callback), storage_name);
  }
  task_helper->CopyFileFromLocal(
      storage_name, source_file_descriptor, parent_id, file_name,
      std::move(success_callback), std::move(error_callback));
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
    MTPDeviceTaskHelper::DeleteObjectSuccessCallback success_callback,
    MTPDeviceTaskHelper::ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name, read_only);
  if (!task_helper) {
    return PostError(std::move(error_callback), storage_name);
  }
  task_helper->DeleteObject(object_id, std::move(success_callback),
                            std::move(error_callback));
}

// Closes the device storage specified by the |storage_name| and destroys the
// MTPDeviceTaskHelper object associated with the device storage.
//
// Called on the UI thread to dispatch the request to the MTPDeviceTaskHelper.
void CloseStorageAndDestroyTaskHelperOnUIThread(const std::string& storage_name,
                                                const bool read_only) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MTPDeviceTaskHelper* task_helper =
      GetDeviceTaskHelperForStorage(storage_name, read_only);
  if (!task_helper) {
    return;
  }
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
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (base::DirectoryExists(file_path)) {
    return std::make_pair(-1, base::File::FILE_ERROR_NOT_A_FILE);
  }
  int file_descriptor = open(file_path.value().c_str(), flags);
  if (file_descriptor >= 0) {
    return std::make_pair(file_descriptor, base::File::FILE_OK);
  }
  if (errno == ENOENT) {
    return std::make_pair(file_descriptor, base::File::FILE_ERROR_NOT_FOUND);
  }
  return std::make_pair(file_descriptor, base::File::FILE_ERROR_FAILED);
}

// Closes |file_descriptor| on a background task runner.
void CloseFileDescriptor(const int file_descriptor) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  IGNORE_EINTR(close(file_descriptor));
}

// Deletes a temporary file |file_path|.
void DeleteTemporaryFile(const base::FilePath& file_path) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::GetDeleteFileCallback(file_path));
}

// A fake callback to be passed as CopyFileProgressCallback.
void FakeCopyFileProgressCallback(int64_t size) {}

}  // namespace

MTPDeviceDelegateImpl::PendingTaskInfo::PendingTaskInfo(
    const base::FilePath& path,
    content::BrowserThread::ID thread_id,
    const base::Location& location,
    base::OnceClosure task)
    : path(path),
      thread_id(thread_id),
      location(location),
      task(std::move(task)) {}

MTPDeviceDelegateImpl::PendingTaskInfo::PendingTaskInfo(
    PendingTaskInfo&& other) = default;

MTPDeviceDelegateImpl::PendingTaskInfo::~PendingTaskInfo() = default;

// Represents a file on the MTP device.
// Lives on the IO thread.
class MTPDeviceDelegateImpl::MTPFileNode {
 public:
  MTPFileNode(uint32_t file_id,
              const std::string& file_name,
              MTPFileNode* parent,
              MTPFileNodesById& nodes_by_id);

  MTPFileNode(const MTPFileNode&) = delete;
  MTPFileNode& operator=(const MTPFileNode&) = delete;

  ~MTPFileNode();

  const MTPFileNode* GetChild(const std::string& name) const;

  void EnsureChildExists(const std::string& name, uint32_t id);

  // Clears all the children, except those in |children_to_keep|.
  void ClearNonexistentChildren(const std::set<std::string>& children_to_keep);

  bool DeleteChild(uint32_t file_id);

  bool HasChildren() const;

  uint32_t file_id() const { return file_id_; }
  const std::string& file_name() const { return file_name_; }
  MTPFileNode* parent() { return parent_; }

 private:
  // Container for holding a node's children.
  using ChildNodes =
      std::unordered_map<std::string, std::unique_ptr<MTPFileNode>>;

  friend std::ostream& operator<<(std::ostream& out, const ChildNodes& nodes) {
    if (nodes.empty()) {
      return out << "no children";
    }

    out << nodes.size() << " children: ";
    for (std::string_view sep; const ChildNodes::value_type& v : nodes) {
      out << sep << "'" << v.first << "'";
      sep = ", ";
    }

    return out;
  }

  const uint32_t file_id_;
  const std::string file_name_;

  ChildNodes children_;
  const raw_ptr<MTPFileNode> parent_;
  const raw_ref<MTPFileNodesById> nodes_by_id_;
};

MTPDeviceDelegateImpl::MTPFileNode::MTPFileNode(uint32_t file_id,
                                                const std::string& file_name,
                                                MTPFileNode* parent,
                                                MTPFileNodesById& nodes_by_id)
    : file_id_(file_id),
      file_name_(file_name),
      parent_(parent),
      nodes_by_id_(nodes_by_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  const bool ok = nodes_by_id_->try_emplace(file_id_, this).second;
  DCHECK(ok);
}

MTPDeviceDelegateImpl::MTPFileNode::~MTPFileNode() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  const size_t erased = nodes_by_id_->erase(file_id_);
  DCHECK_EQ(1U, erased);
}

const MTPDeviceDelegateImpl::MTPFileNode*
MTPDeviceDelegateImpl::MTPFileNode::GetChild(const std::string& name) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  const ChildNodes::const_iterator it = children_.find(name);
  return it != children_.cend() ? it->second.get() : nullptr;
}

void MTPDeviceDelegateImpl::MTPFileNode::EnsureChildExists(
    const std::string& name,
    uint32_t id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::unique_ptr<MTPFileNode>& child = children_[name];
  if (!child || child->file_id() != id) {
    child = std::make_unique<MTPFileNode>(id, name, this, *nodes_by_id_);
  }
}

void MTPDeviceDelegateImpl::MTPFileNode::ClearNonexistentChildren(
    const std::set<std::string>& children_to_keep) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::erase_if(children_,
                [&children_to_keep](const ChildNodes::value_type& child) {
                  return !children_to_keep.contains(child.first);
                });
}

bool MTPDeviceDelegateImpl::MTPFileNode::DeleteChild(uint32_t file_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  const ChildNodes::const_iterator it = std::ranges::find_if(
      children_, [file_id](const ChildNodes::value_type& child) {
        return child.second->file_id() == file_id;
      });

  if (it == children_.cend()) {
    LOG(ERROR) << "Cannot find MTP node with ID " << file_id;
    return false;
  }

  VLOG_IF(1, !it->second->children_.empty())
      << "Deleting MTP node '" << it->first << "' which has "
      << it->second->children_;
  children_.erase(it);
  return true;
}

bool MTPDeviceDelegateImpl::MTPFileNode::HasChildren() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return !children_.empty();
}

MTPDeviceDelegateImpl::MTPDeviceDelegateImpl(const std::string& device_location,
                                             const bool read_only)
    : device_path_(device_location),
      storage_name_(CreateStorageName(device_location)),
      read_only_(read_only),
      root_node_(std::make_unique<MTPFileNode>(mtpd::kRootFileId,
                                               "",  // Root node has no name.
                                               nullptr,  // And no parent node.
                                               nodes_by_id_)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!device_path_.empty());
  DCHECK(!storage_name_.empty());
}

MTPDeviceDelegateImpl::~MTPDeviceDelegateImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
}

void MTPDeviceDelegateImpl::CreateDirectory(
    const base::FilePath& directory_path,
    const bool exclusive,
    const bool recursive,
    CreateDirectorySuccessCallback success_callback,
    ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!directory_path.empty());

  // If |directory_path| is not the path in this device, fails with error.
  if (!device_path_.IsParent(directory_path)) {
    std::move(error_callback).Run(base::File::FILE_ERROR_FAILED);
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

  base::OnceClosure closure =
      base::BindOnce(&MTPDeviceDelegateImpl::CreateDirectoryInternal,
                     weak_ptr_factory_.GetWeakPtr(), components, exclusive,
                     std::move(success_callback), std::move(error_callback));
  EnsureInitAndRunTask(PendingTaskInfo(directory_path,
                                       content::BrowserThread::IO, FROM_HERE,
                                       std::move(closure)));
}

void MTPDeviceDelegateImpl::GetFileInfo(
    const base::FilePath& file_path,
    GetFileInfoSuccessCallback success_callback,
    ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!file_path.empty());

  // If a ReadDirectory operation is in progress, the file info may already be
  // cached.
  FileInfoCache::const_iterator it = file_info_cache_.find(file_path);
  if (it != file_info_cache_.end()) {
    // TODO(thestig): This code is repeated in several places. Combine them.
    // e.g. c/b/media_galleries/win/mtp_device_operations_util.cc
    const MTPDeviceTaskHelper::MTPEntry& cached_file_entry = it->second;
    std::move(success_callback).Run(cached_file_entry.file_info);
    return;
  }
  base::OnceClosure closure =
      base::BindOnce(&MTPDeviceDelegateImpl::GetFileInfoInternal,
                     weak_ptr_factory_.GetWeakPtr(), file_path,
                     std::move(success_callback), std::move(error_callback));
  EnsureInitAndRunTask(PendingTaskInfo(file_path, content::BrowserThread::IO,
                                       FROM_HERE, std::move(closure)));
}

void MTPDeviceDelegateImpl::ReadDirectory(
    const base::FilePath& root,
    ReadDirectorySuccessCallback success_callback,
    ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!root.empty());
  base::OnceClosure closure =
      base::BindOnce(&MTPDeviceDelegateImpl::ReadDirectoryInternal,
                     weak_ptr_factory_.GetWeakPtr(), root, success_callback,
                     std::move(error_callback));
  EnsureInitAndRunTask(PendingTaskInfo(root, content::BrowserThread::IO,
                                       FROM_HERE, std::move(closure)));
}

void MTPDeviceDelegateImpl::CreateSnapshotFile(
    const base::FilePath& device_file_path,
    const base::FilePath& local_path,
    CreateSnapshotFileSuccessCallback success_callback,
    ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!device_file_path.empty());
  DCHECK(!local_path.empty());
  base::OnceClosure closure = base::BindOnce(
      &MTPDeviceDelegateImpl::CreateSnapshotFileInternal,
      weak_ptr_factory_.GetWeakPtr(), device_file_path, local_path,
      std::move(success_callback), std::move(error_callback));
  EnsureInitAndRunTask(PendingTaskInfo(device_file_path,
                                       content::BrowserThread::IO, FROM_HERE,
                                       std::move(closure)));
}

bool MTPDeviceDelegateImpl::IsStreaming() {
  return true;
}

void MTPDeviceDelegateImpl::ReadBytes(const base::FilePath& device_file_path,
                                      const scoped_refptr<net::IOBuffer>& buf,
                                      int64_t offset,
                                      int buf_len,
                                      ReadBytesSuccessCallback success_callback,
                                      ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!device_file_path.empty());
  base::OnceClosure closure = base::BindOnce(
      &MTPDeviceDelegateImpl::ReadBytesInternal, weak_ptr_factory_.GetWeakPtr(),
      device_file_path, base::RetainedRef(buf), offset, buf_len,
      std::move(success_callback), std::move(error_callback));
  EnsureInitAndRunTask(PendingTaskInfo(device_file_path,
                                       content::BrowserThread::IO, FROM_HERE,
                                       std::move(closure)));
}

bool MTPDeviceDelegateImpl::IsReadOnly() const {
  return read_only_;
}

void MTPDeviceDelegateImpl::CopyFileLocal(
    const base::FilePath& source_file_path,
    const base::FilePath& device_file_path,
    CreateTemporaryFileCallback create_temporary_file_callback,
    CopyFileProgressCallback progress_callback,
    CopyFileLocalSuccessCallback success_callback,
    ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!source_file_path.empty());
  DCHECK(!device_file_path.empty());

  // Create a temporary file for creating a copy of source file on local.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      std::move(create_temporary_file_callback),
      base::BindOnce(
          &MTPDeviceDelegateImpl::OnDidCreateTemporaryFileToCopyFileLocal,
          weak_ptr_factory_.GetWeakPtr(), source_file_path, device_file_path,
          progress_callback, std::move(success_callback),
          std::move(error_callback)));
}

void MTPDeviceDelegateImpl::MoveFileLocal(
    const base::FilePath& source_file_path,
    const base::FilePath& device_file_path,
    CreateTemporaryFileCallback create_temporary_file_callback,
    MoveFileLocalSuccessCallback success_callback,
    ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!source_file_path.empty());
  DCHECK(!device_file_path.empty());

  // In case of error, only one callback will be called.
  auto split_error_callback =
      base::SplitOnceCallback(std::move(error_callback));

  // Get file info to move file on local.
  GetFileInfoSuccessCallback success_callback_wrapper = base::BindOnce(
      &MTPDeviceDelegateImpl::MoveFileLocalInternal,
      weak_ptr_factory_.GetWeakPtr(), source_file_path, device_file_path,
      std::move(create_temporary_file_callback), std::move(success_callback),
      std::move(split_error_callback.first));
  base::OnceClosure closure =
      base::BindOnce(&MTPDeviceDelegateImpl::GetFileInfoInternal,
                     weak_ptr_factory_.GetWeakPtr(), source_file_path,
                     std::move(success_callback_wrapper),
                     std::move(split_error_callback.second));
  EnsureInitAndRunTask(PendingTaskInfo(source_file_path,
                                       content::BrowserThread::IO, FROM_HERE,
                                       std::move(closure)));
}

void MTPDeviceDelegateImpl::CopyFileFromLocal(
    const base::FilePath& source_file_path,
    const base::FilePath& device_file_path,
    CopyFileFromLocalSuccessCallback success_callback,
    ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!source_file_path.empty());
  DCHECK(!device_file_path.empty());

  // In case of error, only one callback will be called.
  auto split_error_callback =
      base::SplitOnceCallback(std::move(error_callback));

  // Get file info of destination file path.
  GetFileInfoSuccessCallback success_callback_wrapper = base::BindOnce(
      &MTPDeviceDelegateImpl::OnDidGetDestFileInfoToCopyFileFromLocal,
      weak_ptr_factory_.GetWeakPtr(), std::move(split_error_callback.first));
  ErrorCallback error_callback_wrapper = base::BindOnce(
      &MTPDeviceDelegateImpl::OnGetDestFileInfoErrorToCopyFileFromLocal,
      weak_ptr_factory_.GetWeakPtr(), source_file_path, device_file_path,
      std::move(success_callback), std::move(split_error_callback.second));
  base::OnceClosure closure = base::BindOnce(
      &MTPDeviceDelegateImpl::GetFileInfoInternal,
      weak_ptr_factory_.GetWeakPtr(), device_file_path,
      std::move(success_callback_wrapper), std::move(error_callback_wrapper));
  EnsureInitAndRunTask(PendingTaskInfo(device_file_path,
                                       content::BrowserThread::IO, FROM_HERE,
                                       std::move(closure)));
}

void MTPDeviceDelegateImpl::DeleteFile(
    const base::FilePath& file_path,
    DeleteFileSuccessCallback success_callback,
    ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!file_path.empty());

  // In case of error, only one callback will be called.
  auto split_error_callback =
      base::SplitOnceCallback(std::move(error_callback));

  GetFileInfoSuccessCallback success_callback_wrapper = base::BindOnce(
      &MTPDeviceDelegateImpl::DeleteFileInternal,
      weak_ptr_factory_.GetWeakPtr(), file_path, std::move(success_callback),
      std::move(split_error_callback.first));

  base::OnceClosure closure =
      base::BindOnce(&MTPDeviceDelegateImpl::GetFileInfoInternal,
                     weak_ptr_factory_.GetWeakPtr(), file_path,
                     std::move(success_callback_wrapper),
                     std::move(split_error_callback.second));
  EnsureInitAndRunTask(PendingTaskInfo(file_path, content::BrowserThread::IO,
                                       FROM_HERE, std::move(closure)));
}

void MTPDeviceDelegateImpl::DeleteDirectory(
    const base::FilePath& file_path,
    DeleteDirectorySuccessCallback success_callback,
    ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!file_path.empty());

  // In case of error, only one callback will be called.
  auto split_error_callback =
      base::SplitOnceCallback(std::move(error_callback));

  GetFileInfoSuccessCallback success_callback_wrapper = base::BindOnce(
      &MTPDeviceDelegateImpl::DeleteDirectoryInternal,
      weak_ptr_factory_.GetWeakPtr(), file_path, std::move(success_callback),
      std::move(split_error_callback.first));

  base::OnceClosure closure =
      base::BindOnce(&MTPDeviceDelegateImpl::GetFileInfoInternal,
                     weak_ptr_factory_.GetWeakPtr(), file_path,
                     std::move(success_callback_wrapper),
                     std::move(split_error_callback.second));
  EnsureInitAndRunTask(PendingTaskInfo(file_path, content::BrowserThread::IO,
                                       FROM_HERE, std::move(closure)));
}

void MTPDeviceDelegateImpl::AddWatcher(
    const GURL& origin,
    const base::FilePath& file_path,
    const bool recursive,
    storage::WatcherManager::StatusCallback callback,
    storage::WatcherManager::NotificationCallback notification_callback) {
  if (recursive) {
    std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  const auto it = subscribers_.find(file_path);
  if (it != subscribers_.end()) {
    // Adds to existing origin callback map.
    if (base::Contains(it->second, origin)) {
      std::move(callback).Run(base::File::FILE_ERROR_EXISTS);
      return;
    }

    it->second.insert(std::make_pair(origin, std::move(notification_callback)));
  } else {
    // Creates new origin callback map.
    OriginNotificationCallbackMap callback_map;
    callback_map.insert(
        std::make_pair(origin, std::move(notification_callback)));
    subscribers_.insert(std::make_pair(file_path, callback_map));
  }

  std::move(callback).Run(base::File::FILE_OK);
}

void MTPDeviceDelegateImpl::RemoveWatcher(
    const GURL& origin,
    const base::FilePath& file_path,
    const bool recursive,
    storage::WatcherManager::StatusCallback callback) {
  if (recursive) {
    std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  const auto it = subscribers_.find(file_path);
  if (it == subscribers_.end()) {
    std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  if (it->second.erase(origin) == 0) {
    std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  if (it->second.empty()) {
    subscribers_.erase(it);
  }

  std::move(callback).Run(base::File::FILE_OK);
}

void MTPDeviceDelegateImpl::NotifyFileChange(
    const base::FilePath& file_path,
    const storage::WatcherManager::ChangeType change_type) {
  const auto it = subscribers_.find(file_path);
  if (it != subscribers_.end()) {
    for (const auto& origin_callback : it->second) {
      origin_callback.second.Run(change_type);
    }
  }
}

void MTPDeviceDelegateImpl::CancelPendingTasksAndDeleteDelegate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // To cancel all the pending tasks, destroy the MTPDeviceTaskHelper object.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&CloseStorageAndDestroyTaskHelperOnUIThread,
                                storage_name_, read_only_));
  delete this;
}

void MTPDeviceDelegateImpl::GetFileInfoInternal(
    const base::FilePath& file_path,
    GetFileInfoSuccessCallback success_callback,
    ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  std::optional<uint32_t> file_id = CachedPathToId(file_path);
  if (file_id) {
    GetFileInfoSuccessCallback success_callback_wrapper = base::BindOnce(
        &MTPDeviceDelegateImpl::OnDidGetFileInfo,
        weak_ptr_factory_.GetWeakPtr(), std::move(success_callback));
    ErrorCallback error_callback_wrapper = base::BindOnce(
        &MTPDeviceDelegateImpl::HandleDeviceFileError,
        weak_ptr_factory_.GetWeakPtr(), std::move(error_callback), *file_id);

    base::OnceClosure closure = base::BindOnce(
        &GetFileInfoOnUIThread, storage_name_, read_only_, *file_id,
        std::move(success_callback_wrapper), std::move(error_callback_wrapper));
    EnsureInitAndRunTask(PendingTaskInfo(base::FilePath(),
                                         content::BrowserThread::UI, FROM_HERE,
                                         std::move(closure)));
  } else {
    std::move(error_callback).Run(base::File::FILE_ERROR_NOT_FOUND);
  }
  PendingRequestDone();
}

void MTPDeviceDelegateImpl::CreateDirectoryInternal(
    const std::vector<base::FilePath>& components,
    const bool exclusive,
    CreateDirectorySuccessCallback success_callback,
    ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  const base::FilePath current_component = components.back();
  std::vector<base::FilePath> other_components = components;
  other_components.pop_back();

  if (other_components.empty()) {
    // Either we reached the last component in the recursive case, or this is
    // the non-recursive case.
    std::optional<uint32_t> parent_id =
        CachedPathToId(current_component.DirName());
    if (parent_id) {
      base::OnceClosure closure = base::BindOnce(
          &MTPDeviceDelegateImpl::CreateSingleDirectory,
          weak_ptr_factory_.GetWeakPtr(), current_component, exclusive,
          std::move(success_callback), std::move(error_callback));
      EnsureInitAndRunTask(PendingTaskInfo(base::FilePath(),
                                           content::BrowserThread::IO,
                                           FROM_HERE, std::move(closure)));
    } else {
      std::move(error_callback).Run(base::File::FILE_ERROR_NOT_FOUND);
    }
  } else {
    // Ensures that parent directories are created for recursive case.
    std::optional<uint32_t> directory_id = CachedPathToId(current_component);
    if (directory_id) {
      // Parent directory |current_component| already exists, continue creating
      // directories.
      base::OnceClosure closure = base::BindOnce(
          &MTPDeviceDelegateImpl::CreateDirectoryInternal,
          weak_ptr_factory_.GetWeakPtr(), other_components, exclusive,
          std::move(success_callback), std::move(error_callback));
      EnsureInitAndRunTask(PendingTaskInfo(base::FilePath(),
                                           content::BrowserThread::IO,
                                           FROM_HERE, std::move(closure)));
    } else {
      // In case of error, only one callback will be called.
      auto split_error_callback =
          base::SplitOnceCallback(std::move(error_callback));

      // If parent directory |current_component| does not exist, create it.
      CreateDirectorySuccessCallback success_callback_wrapper = base::BindOnce(
          &MTPDeviceDelegateImpl::OnDidCreateParentDirectoryToCreateDirectory,
          weak_ptr_factory_.GetWeakPtr(), current_component, other_components,
          exclusive, std::move(success_callback),
          std::move(split_error_callback.first));
      // Wraps error callback to return all errors of creating parent
      // directories as FILE_ERROR_FAILED.
      ErrorCallback error_callback_wrapper = base::BindOnce(
          &MTPDeviceDelegateImpl::OnCreateParentDirectoryErrorToCreateDirectory,
          weak_ptr_factory_.GetWeakPtr(),
          std::move(split_error_callback.second));
      base::OnceClosure closure = base::BindOnce(
          &MTPDeviceDelegateImpl::CreateSingleDirectory,
          weak_ptr_factory_.GetWeakPtr(), current_component,
          false /* not exclusive */, std::move(success_callback_wrapper),
          std::move(error_callback_wrapper));
      EnsureInitAndRunTask(PendingTaskInfo(base::FilePath(),
                                           content::BrowserThread::IO,
                                           FROM_HERE, std::move(closure)));
    }
  }

  PendingRequestDone();
}

void MTPDeviceDelegateImpl::ReadDirectoryInternal(
    const base::FilePath& root,
    ReadDirectorySuccessCallback success_callback,
    ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(task_in_progress_);

  std::optional<uint32_t> dir_id = CachedPathToId(root);
  if (!dir_id) {
    std::move(error_callback).Run(base::File::FILE_ERROR_NOT_FOUND);
    PendingRequestDone();
    return;
  }

  // In case of error, only one callback will be called.
  auto split_error_callback =
      base::SplitOnceCallback(std::move(error_callback));

  GetFileInfoSuccessCallback success_callback_wrapper =
      base::BindOnce(&MTPDeviceDelegateImpl::OnDidGetFileInfoToReadDirectory,
                     weak_ptr_factory_.GetWeakPtr(), *dir_id, success_callback,
                     std::move(split_error_callback.first));
  ErrorCallback error_callback_wrapper =
      base::BindOnce(&MTPDeviceDelegateImpl::HandleDeviceFileError,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(split_error_callback.second), *dir_id);
  base::OnceClosure closure = base::BindOnce(
      &GetFileInfoOnUIThread, storage_name_, read_only_, *dir_id,
      std::move(success_callback_wrapper), std::move(error_callback_wrapper));

  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(closure));
}

void MTPDeviceDelegateImpl::CreateSnapshotFileInternal(
    const base::FilePath& device_file_path,
    const base::FilePath& local_path,
    CreateSnapshotFileSuccessCallback success_callback,
    ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  std::optional<uint32_t> file_id = CachedPathToId(device_file_path);
  if (file_id) {
    // In case of error, only one callback will be called.
    auto split_error_callback =
        base::SplitOnceCallback(std::move(error_callback));

    auto request_info = std::make_unique<SnapshotRequestInfo>(
        *file_id, local_path, std::move(success_callback),
        std::move(split_error_callback.first));
    GetFileInfoSuccessCallback success_callback_wrapper = base::BindOnce(
        &MTPDeviceDelegateImpl::OnDidGetFileInfoToCreateSnapshotFile,
        weak_ptr_factory_.GetWeakPtr(), std::move(request_info));
    ErrorCallback error_callback_wrapper =
        base::BindOnce(&MTPDeviceDelegateImpl::HandleDeviceFileError,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(split_error_callback.second), *file_id);
    base::OnceClosure closure = base::BindOnce(
        &GetFileInfoOnUIThread, storage_name_, read_only_, *file_id,
        std::move(success_callback_wrapper), std::move(error_callback_wrapper));
    EnsureInitAndRunTask(PendingTaskInfo(base::FilePath(),
                                         content::BrowserThread::UI, FROM_HERE,
                                         std::move(closure)));
  } else {
    std::move(error_callback).Run(base::File::FILE_ERROR_NOT_FOUND);
  }
  PendingRequestDone();
}

void MTPDeviceDelegateImpl::ReadBytesInternal(
    const base::FilePath& device_file_path,
    net::IOBuffer* buf,
    int64_t offset,
    int buf_len,
    ReadBytesSuccessCallback success_callback,
    ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  std::optional<uint32_t> file_id = CachedPathToId(device_file_path);
  if (file_id) {
    ReadBytesRequest request(
        *file_id, buf, offset, buf_len,
        base::BindOnce(&MTPDeviceDelegateImpl::OnDidReadBytes,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(success_callback)),
        base::BindOnce(&MTPDeviceDelegateImpl::HandleDeviceFileError,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(error_callback), *file_id));

    base::OnceClosure closure = base::BindOnce(
        &ReadBytesOnUIThread, storage_name_, read_only_, std::move(request));
    EnsureInitAndRunTask(PendingTaskInfo(base::FilePath(),
                                         content::BrowserThread::UI, FROM_HERE,
                                         std::move(closure)));
  } else {
    std::move(error_callback).Run(base::File::FILE_ERROR_NOT_FOUND);
  }
  PendingRequestDone();
}

void MTPDeviceDelegateImpl::MoveFileLocalInternal(
    const base::FilePath& source_file_path,
    const base::FilePath& device_file_path,
    CreateTemporaryFileCallback create_temporary_file_callback,
    MoveFileLocalSuccessCallback success_callback,
    ErrorCallback error_callback,
    const base::File::Info& source_file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (source_file_path.DirName() == device_file_path.DirName()) {
    // If a file or directory is moved in a same directory, rename it.
    std::optional<uint32_t> file_id = CachedPathToId(source_file_path);
    if (file_id) {
      MTPDeviceTaskHelper::RenameObjectSuccessCallback
          success_callback_wrapper = base::BindOnce(
              &MTPDeviceDelegateImpl::OnDidMoveFileLocalWithRename,
              weak_ptr_factory_.GetWeakPtr(), std::move(success_callback),
              source_file_path, *file_id);
      MTPDeviceTaskHelper::ErrorCallback error_callback_wrapper =
          base::BindOnce(&MTPDeviceDelegateImpl::HandleDeviceFileError,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(error_callback), *file_id);
      base::OnceClosure closure =
          base::BindOnce(&RenameObjectOnUIThread, storage_name_, read_only_,
                         *file_id, device_file_path.BaseName().value(),
                         std::move(success_callback_wrapper),
                         std::move(error_callback_wrapper));
      EnsureInitAndRunTask(PendingTaskInfo(base::FilePath(),
                                           content::BrowserThread::UI,
                                           FROM_HERE, std::move(closure)));
    } else {
      std::move(error_callback)
          .Run(source_file_info.is_directory
                   ? base::File::FILE_ERROR_NOT_A_FILE
                   : base::File::FILE_ERROR_NOT_FOUND);
    }
    return;
  }

  if (source_file_info.is_directory) {
    std::move(error_callback).Run(base::File::FILE_ERROR_NOT_A_FILE);
    return;
  }

  // In case of error, only one callback will be called.
  auto split_error_callback =
      base::SplitOnceCallback(std::move(error_callback));

  // If a file is moved to a different directory, create a copy to the
  // destination path, and remove source file.
  CopyFileLocalSuccessCallback success_callback_wrapper =
      base::BindOnce(&MTPDeviceDelegateImpl::DeleteFileInternal,
                     weak_ptr_factory_.GetWeakPtr(), source_file_path,
                     std::move(success_callback),
                     std::move(split_error_callback.first), source_file_info);

  // TODO(yawano): Avoid to call external method from internal code.
  CopyFileLocal(source_file_path, device_file_path,
                std::move(create_temporary_file_callback),
                base::BindRepeating(&FakeCopyFileProgressCallback),
                std::move(success_callback_wrapper),
                std::move(split_error_callback.second));
}

void MTPDeviceDelegateImpl::OnDidOpenFDToCopyFileFromLocal(
    const base::FilePath& device_file_path,
    CopyFileFromLocalSuccessCallback success_callback,
    ErrorCallback error_callback,
    const std::pair<int, base::File::Error>& open_fd_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (open_fd_result.second != base::File::FILE_OK) {
    std::move(error_callback).Run(open_fd_result.second);
    return;
  }

  const int source_file_descriptor = open_fd_result.first;
  std::optional<uint32_t> parent_id =
      CachedPathToId(device_file_path.DirName());
  if (!parent_id) {
    HandleCopyFileFromLocalError(std::move(error_callback),
                                 source_file_descriptor,
                                 base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  MTPDeviceTaskHelper::CopyFileFromLocalSuccessCallback
      success_callback_wrapper = base::BindOnce(
          &MTPDeviceDelegateImpl::OnDidCopyFileFromLocal,
          weak_ptr_factory_.GetWeakPtr(), std::move(success_callback),
          device_file_path, source_file_descriptor);

  ErrorCallback error_callback_wrapper =
      base::BindOnce(&MTPDeviceDelegateImpl::HandleCopyFileFromLocalError,
                     weak_ptr_factory_.GetWeakPtr(), std::move(error_callback),
                     source_file_descriptor);

  base::OnceClosure closure = base::BindOnce(
      &CopyFileFromLocalOnUIThread, storage_name_, read_only_,
      source_file_descriptor, *parent_id, device_file_path.BaseName().value(),
      std::move(success_callback_wrapper), std::move(error_callback_wrapper));

  EnsureInitAndRunTask(PendingTaskInfo(base::FilePath(),
                                       content::BrowserThread::UI, FROM_HERE,
                                       std::move(closure)));
}

void MTPDeviceDelegateImpl::DeleteFileInternal(
    const base::FilePath& file_path,
    DeleteFileSuccessCallback success_callback,
    ErrorCallback error_callback,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (file_info.is_directory) {
    std::move(error_callback).Run(base::File::FILE_ERROR_NOT_A_FILE);
    return;
  }

  std::optional<uint32_t> file_id = CachedPathToId(file_path);
  if (!file_id) {
    std::move(error_callback).Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  RunDeleteObjectOnUIThread(file_path, *file_id, std::move(success_callback),
                            std::move(error_callback));
}

void MTPDeviceDelegateImpl::DeleteDirectoryInternal(
    const base::FilePath& file_path,
    DeleteDirectorySuccessCallback success_callback,
    ErrorCallback error_callback,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!file_info.is_directory) {
    std::move(error_callback).Run(base::File::FILE_ERROR_NOT_A_DIRECTORY);
    return;
  }

  std::optional<uint32_t> directory_id = CachedPathToId(file_path);
  if (!directory_id) {
    std::move(error_callback).Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  // Checks the cache first. If it has children in cache, the directory cannot
  // be empty.
  const MTPFileNodesById::const_iterator it = nodes_by_id_.find(*directory_id);
  if (it != nodes_by_id_.end() && it->second->HasChildren()) {
    std::move(error_callback).Run(base::File::FILE_ERROR_NOT_EMPTY);
    return;
  }

  // In case of error, only one callback will be called.
  auto split_error_callback =
      base::SplitOnceCallback(std::move(error_callback));

  // Since the directory can contain a file even if the cache returns it as
  // empty, explicitly check the directory and confirm it is actually empty.
  MTPDeviceTaskHelper::CheckDirectoryEmptySuccessCallback
      success_callback_wrapper = base::BindOnce(
          &MTPDeviceDelegateImpl::OnDidCheckDirectoryEmptyToDeleteDirectory,
          weak_ptr_factory_.GetWeakPtr(), file_path, *directory_id,
          std::move(success_callback), std::move(split_error_callback.first));
  MTPDeviceTaskHelper::ErrorCallback error_callback_wrapper =
      base::BindOnce(&MTPDeviceDelegateImpl::HandleDeviceFileError,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(split_error_callback.second), *directory_id);
  base::OnceClosure closure = base::BindOnce(
      &CheckDirectoryEmptyOnUIThread, storage_name_, read_only_, *directory_id,
      std::move(success_callback_wrapper), std::move(error_callback_wrapper));
  EnsureInitAndRunTask(PendingTaskInfo(base::FilePath(),
                                       content::BrowserThread::UI, FROM_HERE,
                                       std::move(closure)));
}

void MTPDeviceDelegateImpl::CreateSingleDirectory(
    const base::FilePath& directory_path,
    const bool exclusive,
    CreateDirectorySuccessCallback success_callback,
    ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // In case of error, only one callback will be called.
  auto split_error_callback =
      base::SplitOnceCallback(std::move(error_callback));

  // Only one of the callbacks will be called in either path below.
  auto split_success_callback =
      base::SplitOnceCallback(std::move(success_callback));

  GetFileInfoSuccessCallback success_callback_wrapper = base::BindOnce(
      &MTPDeviceDelegateImpl::OnPathAlreadyExistsForCreateSingleDirectory,
      weak_ptr_factory_.GetWeakPtr(), exclusive,
      std::move(split_success_callback.first),
      std::move(split_error_callback.first));
  ErrorCallback error_callback_wrapper = base::BindOnce(
      &MTPDeviceDelegateImpl::OnPathDoesNotExistForCreateSingleDirectory,
      weak_ptr_factory_.GetWeakPtr(), directory_path,
      std::move(split_success_callback.second),
      std::move(split_error_callback.second));
  base::OnceClosure closure = base::BindOnce(
      &MTPDeviceDelegateImpl::GetFileInfoInternal,
      weak_ptr_factory_.GetWeakPtr(), directory_path,
      std::move(success_callback_wrapper), std::move(error_callback_wrapper));
  EnsureInitAndRunTask(PendingTaskInfo(base::FilePath(),
                                       content::BrowserThread::IO, FROM_HERE,
                                       std::move(closure)));
  PendingRequestDone();
}

void MTPDeviceDelegateImpl::OnDidReadDirectoryToCreateDirectory(
    const std::vector<base::FilePath>& components,
    const bool exclusive,
    CreateDirectorySuccessCallback success_callback,
    ErrorCallback error_callback,
    storage::AsyncFileUtil::EntryList /* entries */,
    const bool has_more) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (has_more) {
    return;  // Wait until all entries have been read.
  }

  base::OnceClosure closure =
      base::BindOnce(&MTPDeviceDelegateImpl::CreateDirectoryInternal,
                     weak_ptr_factory_.GetWeakPtr(), components, exclusive,
                     std::move(success_callback), std::move(error_callback));
  EnsureInitAndRunTask(PendingTaskInfo(base::FilePath(),
                                       content::BrowserThread::IO, FROM_HERE,
                                       std::move(closure)));
}

void MTPDeviceDelegateImpl::OnDidCheckDirectoryEmptyToDeleteDirectory(
    const base::FilePath& directory_path,
    uint32_t directory_id,
    DeleteDirectorySuccessCallback success_callback,
    ErrorCallback error_callback,
    bool is_empty) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (is_empty) {
    RunDeleteObjectOnUIThread(directory_path, directory_id,
                              std::move(success_callback),
                              std::move(error_callback));
  } else {
    std::move(error_callback).Run(base::File::FILE_ERROR_NOT_EMPTY);
  }

  PendingRequestDone();
}

void MTPDeviceDelegateImpl::RunDeleteObjectOnUIThread(
    const base::FilePath& object_path,
    const uint32_t object_id,
    DeleteObjectSuccessCallback success_callback,
    ErrorCallback error_callback) {
  MTPDeviceTaskHelper::DeleteObjectSuccessCallback success_callback_wrapper =
      base::BindOnce(&MTPDeviceDelegateImpl::OnDidDeleteObject,
                     weak_ptr_factory_.GetWeakPtr(), object_path, object_id,
                     std::move(success_callback));

  MTPDeviceTaskHelper::ErrorCallback error_callback_wrapper =
      base::BindOnce(&MTPDeviceDelegateImpl::HandleDeleteFileOrDirectoryError,
                     weak_ptr_factory_.GetWeakPtr(), std::move(error_callback));

  base::OnceClosure closure = base::BindOnce(
      &DeleteObjectOnUIThread, storage_name_, read_only_, object_id,
      std::move(success_callback_wrapper), std::move(error_callback_wrapper));
  EnsureInitAndRunTask(PendingTaskInfo(base::FilePath(),
                                       content::BrowserThread::UI, FROM_HERE,
                                       std::move(closure)));
}

void MTPDeviceDelegateImpl::EnsureInitAndRunTask(PendingTaskInfo task_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if ((init_state_ == kInitialized) && !task_in_progress_) {
    RunTask(std::move(task_info));
    return;
  }

  // Only *Internal functions have empty paths. Since they are the continuation
  // of the current running task, they get to cut in line.
  if (task_info.path.empty()) {
    pending_tasks_.push_front(std::move(task_info));
  } else {
    pending_tasks_.push_back(std::move(task_info));
  }

  if (init_state_ == kUninitialized) {
    init_state_ = kPendingInit;
    task_in_progress_ = true;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&OpenStorageOnUIThread, storage_name_, read_only_,
                       base::BindOnce(&MTPDeviceDelegateImpl::OnInitCompleted,
                                      weak_ptr_factory_.GetWeakPtr())));
  }
}

void MTPDeviceDelegateImpl::RunTask(PendingTaskInfo task_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK_EQ(kInitialized, init_state_);
  DCHECK(!task_in_progress_);
  task_in_progress_ = true;

  bool need_to_check_cache = !task_info.path.empty();
  if (need_to_check_cache) {
    base::FilePath uncached_path =
        NextUncachedPathComponent(task_info.path, task_info.cached_path);
    if (!uncached_path.empty()) {
      // Save the current task and do a cache lookup first.
      pending_tasks_.push_front(std::move(task_info));
      FillFileCache(uncached_path);
      return;
    }
  }

  switch (task_info.thread_id) {
    case content::BrowserThread::UI:
      content::GetUIThreadTaskRunner({})->PostTask(task_info.location,
                                                   std::move(task_info.task));
      break;
    case content::BrowserThread::IO:
      content::GetIOThreadTaskRunner({})->PostTask(task_info.location,
                                                   std::move(task_info.task));
      break;
    case content::BrowserThread::ID_COUNT:
      NOTREACHED();
  }
}

void MTPDeviceDelegateImpl::WriteDataIntoSnapshotFile(
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(current_snapshot_request_info_.get());
  DCHECK_GT(file_info.size, 0);
  DCHECK(task_in_progress_);
  SnapshotRequestInfo request_info(
      current_snapshot_request_info_->file_id,
      current_snapshot_request_info_->snapshot_file_path,
      base::BindOnce(&MTPDeviceDelegateImpl::OnDidWriteDataIntoSnapshotFile,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&MTPDeviceDelegateImpl::OnWriteDataIntoSnapshotFileError,
                     weak_ptr_factory_.GetWeakPtr()));

  base::OnceClosure task_closure =
      base::BindOnce(&WriteDataIntoSnapshotFileOnUIThread, storage_name_,
                     read_only_, std::move(request_info), file_info);
  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                               std::move(task_closure));
}

void MTPDeviceDelegateImpl::PendingRequestDone() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(task_in_progress_);
  task_in_progress_ = false;
  ProcessNextPendingRequest();
}

void MTPDeviceDelegateImpl::ProcessNextPendingRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!task_in_progress_);
  if (pending_tasks_.empty()) {
    return;
  }

  PendingTaskInfo task_info = std::move(pending_tasks_.front());
  pending_tasks_.pop_front();
  RunTask(std::move(task_info));
}

void MTPDeviceDelegateImpl::OnInitCompleted(bool succeeded) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  init_state_ = succeeded ? kInitialized : kUninitialized;
  PendingRequestDone();
}

void MTPDeviceDelegateImpl::OnDidGetFileInfo(
    GetFileInfoSuccessCallback success_callback,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::move(success_callback).Run(file_info);
  PendingRequestDone();
}

void MTPDeviceDelegateImpl::OnPathAlreadyExistsForCreateSingleDirectory(
    const bool exclusive,
    CreateDirectorySuccessCallback success_callback,
    ErrorCallback error_callback,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!file_info.is_directory || exclusive) {
    std::move(error_callback).Run(base::File::FILE_ERROR_EXISTS);
  } else {
    std::move(success_callback).Run();
  }
}

void MTPDeviceDelegateImpl::OnPathDoesNotExistForCreateSingleDirectory(
    const base::FilePath& directory_path,
    CreateDirectorySuccessCallback success_callback,
    ErrorCallback error_callback,
    const base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (error != base::File::FILE_ERROR_NOT_FOUND) {
    std::move(error_callback).Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  std::optional<uint32_t> parent_id = CachedPathToId(directory_path.DirName());
  if (!parent_id) {
    std::move(error_callback).Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  MTPDeviceTaskHelper::CreateDirectorySuccessCallback success_callback_wrapper =
      base::BindOnce(&MTPDeviceDelegateImpl::OnDidCreateSingleDirectory,
                     weak_ptr_factory_.GetWeakPtr(), directory_path,
                     std::move(success_callback));
  MTPDeviceTaskHelper::ErrorCallback error_callback_wrapper = base::BindOnce(
      &MTPDeviceDelegateImpl::HandleDeviceFileError,
      weak_ptr_factory_.GetWeakPtr(), std::move(error_callback), *parent_id);
  base::OnceClosure closure = base::BindOnce(
      &CreateDirectoryOnUIThread, storage_name_, read_only_, *parent_id,
      directory_path.BaseName().value(), std::move(success_callback_wrapper),
      std::move(error_callback_wrapper));
  EnsureInitAndRunTask(PendingTaskInfo(base::FilePath(),
                                       content::BrowserThread::UI, FROM_HERE,
                                       std::move(closure)));
}

void MTPDeviceDelegateImpl::OnDidGetFileInfoToReadDirectory(
    uint32_t dir_id,
    ReadDirectorySuccessCallback success_callback,
    ErrorCallback error_callback,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(task_in_progress_);
  if (!file_info.is_directory) {
    return HandleDeviceFileError(std::move(error_callback), dir_id,
                                 base::File::FILE_ERROR_NOT_A_DIRECTORY);
  }

  base::OnceClosure task_closure = base::BindOnce(
      &ReadDirectoryOnUIThread, storage_name_, read_only_, dir_id,
      base::BindRepeating(&MTPDeviceDelegateImpl::OnDidReadDirectory,
                          weak_ptr_factory_.GetWeakPtr(), dir_id,
                          success_callback),
      base::BindOnce(&MTPDeviceDelegateImpl::HandleDeviceFileError,
                     weak_ptr_factory_.GetWeakPtr(), std::move(error_callback),
                     dir_id));
  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                               std::move(task_closure));
}

void MTPDeviceDelegateImpl::OnDidGetFileInfoToCreateSnapshotFile(
    std::unique_ptr<SnapshotRequestInfo> snapshot_request_info,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!current_snapshot_request_info_.get());
  DCHECK(snapshot_request_info.get());
  DCHECK(task_in_progress_);
  base::File::Error error = base::File::FILE_OK;
  if (file_info.is_directory) {
    error = base::File::FILE_ERROR_NOT_A_FILE;
  } else if (file_info.size < 0 ||
             file_info.size > std::numeric_limits<uint32_t>::max()) {
    error = base::File::FILE_ERROR_FAILED;
  }

  if (error != base::File::FILE_OK) {
    return HandleDeviceFileError(
        std::move(snapshot_request_info->error_callback),
        snapshot_request_info->file_id, error);
  }

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

void MTPDeviceDelegateImpl::OnDidGetDestFileInfoToCopyFileFromLocal(
    ErrorCallback error_callback,
    const base::File::Info& file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (file_info.is_directory) {
    std::move(error_callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
  } else {
    std::move(error_callback).Run(base::File::FILE_ERROR_FAILED);
  }
}

void MTPDeviceDelegateImpl::OnGetDestFileInfoErrorToCopyFileFromLocal(
    const base::FilePath& source_file_path,
    const base::FilePath& device_file_path,
    CopyFileFromLocalSuccessCallback success_callback,
    ErrorCallback error_callback,
    const base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (error != base::File::FILE_ERROR_NOT_FOUND) {
    std::move(error_callback).Run(error);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&OpenFileDescriptor, source_file_path, O_RDONLY),
      base::BindOnce(&MTPDeviceDelegateImpl::OnDidOpenFDToCopyFileFromLocal,
                     weak_ptr_factory_.GetWeakPtr(), device_file_path,
                     std::move(success_callback), std::move(error_callback)));
}

void MTPDeviceDelegateImpl::OnDidCreateSingleDirectory(
    const base::FilePath& directory_path,
    CreateDirectorySuccessCallback success_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  std::move(success_callback).Run();
  NotifyFileChange(directory_path.DirName(),
                   storage::WatcherManager::ChangeType::CHANGED);
  PendingRequestDone();
}

void MTPDeviceDelegateImpl::OnDidCreateParentDirectoryToCreateDirectory(
    const base::FilePath& created_directory,
    const std::vector<base::FilePath>& components,
    const bool exclusive,
    CreateDirectorySuccessCallback success_callback,
    ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // In case of error, only one callback will be called.
  auto split_error_callback =
      base::SplitOnceCallback(std::move(error_callback));

  // Calls ReadDirectoryInternal to fill the cache for created directory.
  // Calls ReadDirectoryInternal in this method to call it via
  // EnsureInitAndRunTask.
  ReadDirectorySuccessCallback success_callback_wrapper = base::BindRepeating(
      &MTPDeviceDelegateImpl::OnDidReadDirectoryToCreateDirectory,
      weak_ptr_factory_.GetWeakPtr(), components, exclusive,
      base::Passed(&success_callback),
      base::Passed(&split_error_callback.first));
  base::OnceClosure closure = base::BindOnce(
      &MTPDeviceDelegateImpl::ReadDirectoryInternal,
      weak_ptr_factory_.GetWeakPtr(), created_directory.DirName(),
      success_callback_wrapper, std::move(split_error_callback.second));
  EnsureInitAndRunTask(PendingTaskInfo(base::FilePath(),
                                       content::BrowserThread::IO, FROM_HERE,
                                       std::move(closure)));
}

void MTPDeviceDelegateImpl::OnCreateParentDirectoryErrorToCreateDirectory(
    ErrorCallback callback,
    const base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  std::move(callback).Run(base::File::FILE_ERROR_FAILED);
}

void MTPDeviceDelegateImpl::OnDidReadDirectory(
    uint32_t dir_id,
    ReadDirectorySuccessCallback success_callback,
    const MTPDeviceTaskHelper::MTPEntries& mtp_entries,
    bool has_more) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  storage::AsyncFileUtil::EntryList file_list;

  MTPFileNode* dir_node = nullptr;
  if (const MTPFileNodesById::const_iterator it = nodes_by_id_.find(dir_id);
      it != nodes_by_id_.cend()) {
    dir_node = it->second;

    // Traverse the MTPFileNode tree to reconstruct the full path for |dir_id|.
    base::circular_deque<std::string> dir_path_parts;
    MTPFileNode* parent_node = dir_node;
    while (parent_node->parent()) {
      dir_path_parts.push_front(parent_node->file_name());
      parent_node = parent_node->parent();
    }
    base::FilePath dir_path = device_path_;
    for (const auto& dir_path_part : dir_path_parts) {
      dir_path = dir_path.Append(dir_path_part);
    }

    for (const auto& mtp_entry : mtp_entries) {
      filesystem::mojom::DirectoryEntry entry;
      auto name = base::SafeBaseName::Create(mtp_entry.name);
      if (!name) {
        LOG(ERROR) << "Skipped unsafe MTP entry '" << mtp_entry.name << "'";
        continue;
      }
      entry.name = *name;
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
  } else {
    LOG(ERROR) << "Cannot find MTP node with ID " << dir_id;
  }

  success_callback.Run(file_list, has_more);
  if (has_more) {
    return;  // Wait to be called again.
  }

  // Last call, finish book keeping and continue with the next request.
  if (dir_node) {
    dir_node->ClearNonexistentChildren(child_nodes_seen_);
  }

  child_nodes_seen_.clear();
  file_info_cache_.clear();

  PendingRequestDone();
}

void MTPDeviceDelegateImpl::OnDidWriteDataIntoSnapshotFile(
    const base::File::Info& file_info,
    const base::FilePath& snapshot_file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(current_snapshot_request_info_.get());
  std::move(current_snapshot_request_info_->success_callback)
      .Run(file_info, snapshot_file_path);
  current_snapshot_request_info_.reset();
  PendingRequestDone();
}

void MTPDeviceDelegateImpl::OnWriteDataIntoSnapshotFileError(
    base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(current_snapshot_request_info_.get());
  std::move(current_snapshot_request_info_->error_callback).Run(error);
  current_snapshot_request_info_.reset();
  PendingRequestDone();
}

void MTPDeviceDelegateImpl::OnDidReadBytes(
    ReadBytesSuccessCallback success_callback,
    const base::File::Info& file_info,
    int bytes_read) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::move(success_callback).Run(file_info, bytes_read);
  PendingRequestDone();
}

void MTPDeviceDelegateImpl::OnDidFillFileCache(
    const base::FilePath& path,
    storage::AsyncFileUtil::EntryList /* entries */,
    bool has_more) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (pending_tasks_.empty()) {
    return;
  }

  DCHECK(path.IsParent(pending_tasks_.front().path));
  if (has_more) {
    return;  // Wait until all entries have been read.
  }
  pending_tasks_.front().cached_path = path;
}

void MTPDeviceDelegateImpl::OnFillFileCacheFailed(
    base::File::Error /* error */) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (pending_tasks_.empty()) {
    return;
  }

  // When filling the cache fails for the task at the front of the queue, clear
  // the path of the task so it will not try to do any more caching. Instead,
  // the task will just run and fail the CachedPathToId() lookup.
  pending_tasks_.front().path.clear();
}

void MTPDeviceDelegateImpl::OnDidCreateTemporaryFileToCopyFileLocal(
    const base::FilePath& source_file_path,
    const base::FilePath& device_file_path,
    CopyFileProgressCallback progress_callback,
    CopyFileLocalSuccessCallback success_callback,
    ErrorCallback error_callback,
    const base::FilePath& temporary_file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (temporary_file_path.empty()) {
    std::move(error_callback).Run(base::File::FILE_ERROR_FAILED);
    return;
  }

  // In case of error, only one callback will be called.
  auto split_error_callback =
      base::SplitOnceCallback(std::move(error_callback));

  CreateSnapshotFile(
      source_file_path, temporary_file_path,
      base::BindOnce(
          &MTPDeviceDelegateImpl::OnDidCreateSnapshotFileOfCopyFileLocal,
          weak_ptr_factory_.GetWeakPtr(), device_file_path, progress_callback,
          std::move(success_callback), std::move(split_error_callback.first)),
      base::BindOnce(&MTPDeviceDelegateImpl::HandleCopyFileLocalError,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(split_error_callback.second),
                     temporary_file_path));
}

void MTPDeviceDelegateImpl::OnDidCreateSnapshotFileOfCopyFileLocal(
    const base::FilePath& device_file_path,
    CopyFileProgressCallback progress_callback,
    CopyFileLocalSuccessCallback success_callback,
    ErrorCallback error_callback,
    const base::File::Info& file_info,
    const base::FilePath& temporary_file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // Consider that half of copy is completed by creating a temporary file.
  progress_callback.Run(file_info.size / 2);

  // TODO(yawano): Avoid to call external method from internal code.
  CopyFileFromLocal(
      temporary_file_path, device_file_path,
      base::BindOnce(
          &MTPDeviceDelegateImpl::OnDidCopyFileFromLocalOfCopyFileLocal,
          weak_ptr_factory_.GetWeakPtr(), std::move(success_callback),
          temporary_file_path),
      base::BindOnce(&MTPDeviceDelegateImpl::HandleCopyFileLocalError,
                     weak_ptr_factory_.GetWeakPtr(), std::move(error_callback),
                     temporary_file_path));
}

void MTPDeviceDelegateImpl::OnDidCopyFileFromLocalOfCopyFileLocal(
    CopyFileFromLocalSuccessCallback success_callback,
    const base::FilePath& temporary_file_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  DeleteTemporaryFile(temporary_file_path);
  std::move(success_callback).Run();
}

void MTPDeviceDelegateImpl::OnDidMoveFileLocalWithRename(
    MoveFileLocalSuccessCallback success_callback,
    const base::FilePath& source_file_path,
    const uint32_t file_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  EvictCachedPathToId(file_id);
  std::move(success_callback).Run();
  NotifyFileChange(source_file_path,
                   storage::WatcherManager::ChangeType::DELETED);
  NotifyFileChange(source_file_path.DirName(),
                   storage::WatcherManager::ChangeType::CHANGED);
  PendingRequestDone();
}

void MTPDeviceDelegateImpl::OnDidCopyFileFromLocal(
    CopyFileFromLocalSuccessCallback success_callback,
    const base::FilePath& file_path,
    const int source_file_descriptor) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&CloseFileDescriptor, source_file_descriptor));

  std::move(success_callback).Run();
  NotifyFileChange(file_path.DirName(),
                   storage::WatcherManager::ChangeType::CHANGED);
  PendingRequestDone();
}

void MTPDeviceDelegateImpl::HandleCopyFileLocalError(
    ErrorCallback error_callback,
    const base::FilePath& temporary_file_path,
    const base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  DeleteTemporaryFile(temporary_file_path);
  std::move(error_callback).Run(error);
}

void MTPDeviceDelegateImpl::HandleCopyFileFromLocalError(
    ErrorCallback error_callback,
    const int source_file_descriptor,
    base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&CloseFileDescriptor, source_file_descriptor));

  std::move(error_callback).Run(error);
  PendingRequestDone();
}

void MTPDeviceDelegateImpl::OnDidDeleteObject(
    const base::FilePath& object_path,
    const uint32_t object_id,
    DeleteObjectSuccessCallback success_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  EvictCachedPathToId(object_id);
  std::move(success_callback).Run();
  NotifyFileChange(object_path, storage::WatcherManager::ChangeType::DELETED);
  NotifyFileChange(object_path.DirName(),
                   storage::WatcherManager::ChangeType::CHANGED);
  PendingRequestDone();
}

void MTPDeviceDelegateImpl::HandleDeleteFileOrDirectoryError(
    ErrorCallback error_callback,
    base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  std::move(error_callback).Run(error);
  PendingRequestDone();
}

void MTPDeviceDelegateImpl::HandleDeviceFileError(ErrorCallback error_callback,
                                                  uint32_t file_id,
                                                  base::File::Error error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  EvictCachedPathToId(file_id);
  std::move(error_callback).Run(error);
  PendingRequestDone();
}

base::FilePath MTPDeviceDelegateImpl::NextUncachedPathComponent(
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
    if (all_components_cached) {
      uncached_path.clear();
    }
  }
  return uncached_path;
}

void MTPDeviceDelegateImpl::FillFileCache(const base::FilePath& uncached_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(task_in_progress_);

  ReadDirectorySuccessCallback success_callback =
      base::BindRepeating(&MTPDeviceDelegateImpl::OnDidFillFileCache,
                          weak_ptr_factory_.GetWeakPtr(), uncached_path);
  ErrorCallback error_callback =
      base::BindOnce(&MTPDeviceDelegateImpl::OnFillFileCacheFailed,
                     weak_ptr_factory_.GetWeakPtr());
  ReadDirectoryInternal(uncached_path, success_callback,
                        std::move(error_callback));
}

std::optional<uint32_t> MTPDeviceDelegateImpl::CachedPathToId(
    const base::FilePath& path) const {
  std::string device_relpath = GetDeviceRelativePath(device_path_, path);
  if (device_relpath.empty()) {
    return {};
  }
  std::vector<std::string> device_relpath_components;
  if (device_relpath != kRootPath) {
    device_relpath_components = base::SplitString(
        device_relpath, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  }
  const MTPFileNode* current_node = root_node_.get();
  for (const std::string& component : device_relpath_components) {
    current_node = current_node->GetChild(component);
    if (!current_node) {
      return {};
    }
  }
  return current_node->file_id();
}

void MTPDeviceDelegateImpl::EvictCachedPathToId(uint32_t id) {
  const MTPFileNodesById::const_iterator it = nodes_by_id_.find(id);
  if (it == nodes_by_id_.cend()) {
    LOG(ERROR) << "Cannot find MTP node with ID " << id;
    return;
  }

  if (MTPFileNode* const parent = it->second->parent()) {
    const bool ok = parent->DeleteChild(id);
    DCHECK(ok);
  }
}

void CreateMTPDeviceAsyncDelegate(
    const std::string& device_location,
    const bool read_only,
    CreateMTPDeviceAsyncDelegateCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  std::move(callback).Run(
      new MTPDeviceDelegateImpl(device_location, read_only));
}
