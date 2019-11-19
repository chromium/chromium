// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/chromeos/mtp_device_task_helper.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "chrome/browser/media_galleries/chromeos/mtp_device_object_enumerator.h"
#include "chrome/browser/media_galleries/chromeos/mtp_read_file_worker.h"
#include "chrome/browser/media_galleries/chromeos/snapshot_file_details.h"
#include "components/storage_monitor/storage_monitor.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "services/device/public/mojom/mtp_manager.mojom.h"
#include "storage/browser/file_system/async_file_util.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using storage_monitor::StorageMonitor;

namespace {

// Splits |file_ids| into |file_ids_to_read_now| and |file_ids_to_read_later|.
// This is used to prevent GetFileInfo() from being called with too many entries
// at once.
void SplitFileIds(const std::vector<uint32_t>& file_ids,
                  std::vector<uint32_t>* file_ids_to_read_now,
                  std::vector<uint32_t>* file_ids_to_read_later) {
  DCHECK(file_ids_to_read_now);
  DCHECK(file_ids_to_read_now->empty());
  DCHECK(file_ids_to_read_later);
  DCHECK(file_ids_to_read_later->empty());

  // When reading directory entries, this is the number of entries for
  // GetFileInfo() to read in one operation. If set too low, efficiency goes
  // down slightly due to the overhead of D-Bus calls. If set too high, then
  // slow devices may trigger a D-Bus timeout.
  // The value below is a good initial estimate.
  static constexpr size_t kFileInfoToFetchChunkSize = 25;

  size_t chunk_size = kFileInfoToFetchChunkSize;
  if (file_ids.size() <= chunk_size) {
    *file_ids_to_read_now = file_ids;
  } else {
    std::copy_n(file_ids.begin(), chunk_size,
                std::back_inserter(*file_ids_to_read_now));
    std::copy(file_ids.begin() + chunk_size, file_ids.end(),
              std::back_inserter(*file_ids_to_read_later));
  }
}

device::mojom::MtpManager* GetMediaTransferProtocolManager() {
  return StorageMonitor::GetInstance()->media_transfer_protocol_manager();
}

base::File::Info FileInfoFromMTPFileEntry(
    device::mojom::MtpFileEntryPtr file_entry) {
  base::File::Info file_entry_info;
  file_entry_info.size = file_entry->file_size;
  file_entry_info.is_directory =
      file_entry->file_type ==
      device::mojom::MtpFileEntry::FileType::FILE_TYPE_FOLDER;
  file_entry_info.is_symbolic_link = false;
  file_entry_info.last_modified =
      base::Time::FromTimeT(file_entry->modification_time);
  file_entry_info.last_accessed = file_entry_info.last_modified;
  file_entry_info.creation_time = base::Time();
  return file_entry_info;
}

}  // namespace

MTPDeviceTaskHelper::MTPDeviceTaskHelper() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

MTPDeviceTaskHelper::~MTPDeviceTaskHelper() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void MTPDeviceTaskHelper::OpenStorage(const std::string& storage_name,
                                      const bool read_only,
                                      const OpenStorageCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!storage_name.empty());
  if (!device_handle_.empty()) {
    base::PostTask(FROM_HERE, {content::BrowserThread::IO},
                   base::BindOnce(callback, true));
    return;
  }

  const std::string mode =
      read_only ? mtpd::kReadOnlyMode : mtpd::kReadWriteMode;
  GetMediaTransferProtocolManager()->OpenStorage(
      storage_name, mode, base::Bind(&MTPDeviceTaskHelper::OnDidOpenStorage,
                                     weak_ptr_factory_.GetWeakPtr(), callback));
}

void MTPDeviceTaskHelper::GetFileInfo(
    uint32_t file_id,
    const GetFileInfoSuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (device_handle_.empty())
    return HandleDeviceError(error_callback, base::File::FILE_ERROR_FAILED);

  const std::vector<uint32_t> file_ids = {file_id};
  GetMediaTransferProtocolManager()->GetFileInfo(
      device_handle_, file_ids,
      base::Bind(&MTPDeviceTaskHelper::OnGetFileInfo,
                 weak_ptr_factory_.GetWeakPtr(), success_callback,
                 error_callback));
}

void MTPDeviceTaskHelper::CreateDirectory(
    const uint32_t parent_id,
    const std::string& directory_name,
    const CreateDirectorySuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (device_handle_.empty())
    return HandleDeviceError(error_callback, base::File::FILE_ERROR_FAILED);

  GetMediaTransferProtocolManager()->CreateDirectory(
      device_handle_, parent_id, directory_name,
      base::Bind(&MTPDeviceTaskHelper::OnCreateDirectory,
                 weak_ptr_factory_.GetWeakPtr(), success_callback,
                 error_callback));
}

void MTPDeviceTaskHelper::ReadDirectory(
    const uint32_t directory_id,
    const ReadDirectorySuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (device_handle_.empty())
    return HandleDeviceError(error_callback, base::File::FILE_ERROR_FAILED);

  GetMediaTransferProtocolManager()->ReadDirectoryEntryIds(
      device_handle_, directory_id,
      base::BindOnce(
          &MTPDeviceTaskHelper::OnReadDirectoryEntryIdsToReadDirectory,
          weak_ptr_factory_.GetWeakPtr(), success_callback, error_callback));
}

void MTPDeviceTaskHelper::CheckDirectoryEmpty(
    uint32_t directory_id,
    CheckDirectoryEmptySuccessCallback success_callback,
    const ErrorCallback& error_callback) {
  if (device_handle_.empty())
    return HandleDeviceError(error_callback, base::File::FILE_ERROR_FAILED);

  GetMediaTransferProtocolManager()->ReadDirectoryEntryIds(
      device_handle_, directory_id,
      base::BindOnce(&MTPDeviceTaskHelper::OnCheckedDirectoryEmpty,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(success_callback), error_callback));
}

void MTPDeviceTaskHelper::WriteDataIntoSnapshotFile(
    const SnapshotRequestInfo& request_info,
    const base::File::Info& snapshot_file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (device_handle_.empty()) {
    return HandleDeviceError(request_info.error_callback,
                             base::File::FILE_ERROR_FAILED);
  }

  if (!read_file_worker_)
    read_file_worker_.reset(new MTPReadFileWorker(device_handle_));
  read_file_worker_->WriteDataIntoSnapshotFile(request_info,
                                               snapshot_file_info);
}

void MTPDeviceTaskHelper::ReadBytes(
    const MTPDeviceAsyncDelegate::ReadBytesRequest& request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (device_handle_.empty()) {
    return HandleDeviceError(request.error_callback,
                             base::File::FILE_ERROR_FAILED);
  }

  const std::vector<uint32_t> file_ids = {request.file_id};
  GetMediaTransferProtocolManager()->GetFileInfo(
      device_handle_, file_ids,
      base::BindOnce(&MTPDeviceTaskHelper::OnGetFileInfoToReadBytes,
                     weak_ptr_factory_.GetWeakPtr(), request));
}

void MTPDeviceTaskHelper::RenameObject(
    const uint32_t object_id,
    const std::string& new_name,
    const RenameObjectSuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  GetMediaTransferProtocolManager()->RenameObject(
      device_handle_, object_id, new_name,
      base::Bind(&MTPDeviceTaskHelper::OnRenameObject,
                 weak_ptr_factory_.GetWeakPtr(), success_callback,
                 error_callback));
}

MTPDeviceTaskHelper::MTPEntry::MTPEntry() : file_id(0) {}

// TODO(yawano) storage_name is not used, delete it.
void MTPDeviceTaskHelper::CopyFileFromLocal(
    const std::string& storage_name,
    const int source_file_descriptor,
    const uint32_t parent_id,
    const std::string& file_name,
    const CopyFileFromLocalSuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  GetMediaTransferProtocolManager()->CopyFileFromLocal(
      device_handle_, source_file_descriptor, parent_id, file_name,
      base::Bind(&MTPDeviceTaskHelper::OnCopyFileFromLocal,
                 weak_ptr_factory_.GetWeakPtr(), success_callback,
                 error_callback));
}

void MTPDeviceTaskHelper::DeleteObject(
    const uint32_t object_id,
    const DeleteObjectSuccessCallback& success_callback,
    const ErrorCallback& error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  GetMediaTransferProtocolManager()->DeleteObject(
      device_handle_, object_id,
      base::Bind(&MTPDeviceTaskHelper::OnDeleteObject,
                 weak_ptr_factory_.GetWeakPtr(), success_callback,
                 error_callback));
}

void MTPDeviceTaskHelper::CloseStorage() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (device_handle_.empty())
    return;
  GetMediaTransferProtocolManager()->CloseStorage(device_handle_,
                                                  base::DoNothing());
}

void MTPDeviceTaskHelper::OnDidOpenStorage(
    const OpenStorageCallback& completion_callback,
    const std::string& device_handle,
    bool error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  device_handle_ = device_handle;
  base::PostTask(FROM_HERE, {content::BrowserThread::IO},
                 base::BindOnce(completion_callback, !error));
}

void MTPDeviceTaskHelper::OnGetFileInfo(
    const GetFileInfoSuccessCallback& success_callback,
    const ErrorCallback& error_callback,
    std::vector<device::mojom::MtpFileEntryPtr> entries,
    bool error) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (error || entries.size() != 1) {
    return HandleDeviceError(error_callback,
                             base::File::FILE_ERROR_NOT_FOUND);
  }

  base::PostTask(FROM_HERE, {content::BrowserThread::IO},
                 base::BindOnce(success_callback, FileInfoFromMTPFileEntry(
                                                      std::move(entries[0]))));
}

void MTPDeviceTaskHelper::OnCreateDirectory(
    const CreateDirectorySuccessCallback& success_callback,
    const ErrorCallback& error_callback,
    const bool error) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (error) {
    base::PostTask(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(error_callback, base::File::FILE_ERROR_FAILED));
    return;
  }

  base::PostTask(FROM_HERE, {content::BrowserThread::IO}, success_callback);
}

void MTPDeviceTaskHelper::OnReadDirectoryEntryIdsToReadDirectory(
    const ReadDirectorySuccessCallback& success_callback,
    const ErrorCallback& error_callback,
    const std::vector<uint32_t>& file_ids,
    bool error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (error)
    return HandleDeviceError(error_callback, base::File::FILE_ERROR_FAILED);

  if (file_ids.empty()) {
    base::PostTask(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(success_callback, MTPEntries(), /*has_more=*/false));
    return;
  }

  std::vector<uint32_t> file_ids_to_read_now;
  std::vector<uint32_t> file_ids_to_read_later;
  SplitFileIds(file_ids, &file_ids_to_read_now, &file_ids_to_read_later);

  GetMediaTransferProtocolManager()->GetFileInfo(
      device_handle_, file_ids_to_read_now,
      base::BindOnce(&MTPDeviceTaskHelper::OnGotDirectoryEntries,
                     weak_ptr_factory_.GetWeakPtr(), success_callback,
                     error_callback, file_ids_to_read_now,
                     file_ids_to_read_later));
}

void MTPDeviceTaskHelper::OnGotDirectoryEntries(
    const ReadDirectorySuccessCallback& success_callback,
    const ErrorCallback& error_callback,
    const std::vector<uint32_t>& expected_file_ids,
    const std::vector<uint32_t>& file_ids_to_read,
    std::vector<device::mojom::MtpFileEntryPtr> file_entries,
    bool error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (error)
    return HandleDeviceError(error_callback, base::File::FILE_ERROR_FAILED);

  // Use |expected_file_ids| to verify the results are the requested ids.
  std::vector<uint32_t> sorted_expected_file_ids = expected_file_ids;
  std::sort(sorted_expected_file_ids.begin(), sorted_expected_file_ids.end());
  for (const auto& entry : file_entries) {
    std::vector<uint32_t>::const_iterator it =
        std::lower_bound(sorted_expected_file_ids.begin(),
                         sorted_expected_file_ids.end(), entry->item_id);
    if (it == sorted_expected_file_ids.end()) {
      return HandleDeviceError(error_callback, base::File::FILE_ERROR_FAILED);
    }
  }

  MTPEntries entries;
  base::FilePath current;
  MTPDeviceObjectEnumerator file_enum(std::move(file_entries));
  while (!(current = file_enum.Next()).empty()) {
    MTPEntry entry;
    entry.name = storage::VirtualPath::BaseName(current).value();
    bool ret = file_enum.GetEntryId(&entry.file_id);
    DCHECK(ret);
    entry.file_info.is_directory = file_enum.IsDirectory();
    entry.file_info.size = file_enum.Size();
    entry.file_info.last_modified = file_enum.LastModifiedTime();
    entries.push_back(entry);
  }

  bool has_more = !file_ids_to_read.empty();

  base::PostTask(FROM_HERE, {content::BrowserThread::IO},
                 base::BindOnce(success_callback, entries, has_more));

  if (!has_more)
    return;

  std::vector<uint32_t> file_ids_to_read_now;
  std::vector<uint32_t> file_ids_to_read_later;
  SplitFileIds(file_ids_to_read, &file_ids_to_read_now,
               &file_ids_to_read_later);

  GetMediaTransferProtocolManager()->GetFileInfo(
      device_handle_, file_ids_to_read_now,
      base::BindOnce(&MTPDeviceTaskHelper::OnGotDirectoryEntries,
                     weak_ptr_factory_.GetWeakPtr(), success_callback,
                     error_callback, file_ids_to_read_now,
                     file_ids_to_read_later));
}

void MTPDeviceTaskHelper::OnCheckedDirectoryEmpty(
    CheckDirectoryEmptySuccessCallback success_callback,
    const ErrorCallback& error_callback,
    const std::vector<uint32_t>& file_ids,
    bool error) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (error)
    return HandleDeviceError(error_callback, base::File::FILE_ERROR_FAILED);

  base::PostTask(FROM_HERE, {content::BrowserThread::IO},
                 base::BindOnce(std::move(success_callback), file_ids.empty()));
}

void MTPDeviceTaskHelper::OnGetFileInfoToReadBytes(
    const MTPDeviceAsyncDelegate::ReadBytesRequest& request,
    std::vector<device::mojom::MtpFileEntryPtr> entries,
    bool error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(request.buf.get());
  DCHECK_GE(request.buf_len, 0);
  DCHECK_GE(request.offset, 0);
  if (error || entries.size() != 1) {
    return HandleDeviceError(request.error_callback,
                             base::File::FILE_ERROR_FAILED);
  }

  base::File::Info file_info = FileInfoFromMTPFileEntry(std::move(entries[0]));
  if (file_info.is_directory) {
    return HandleDeviceError(request.error_callback,
                             base::File::FILE_ERROR_NOT_A_FILE);
  }
  if (file_info.size < 0 ||
      file_info.size > std::numeric_limits<uint32_t>::max() ||
      request.offset > file_info.size) {
    return HandleDeviceError(request.error_callback,
                             base::File::FILE_ERROR_FAILED);
  }
  if (request.offset == file_info.size) {
    base::PostTask(FROM_HERE, {content::BrowserThread::IO},
                   base::BindOnce(request.success_callback, file_info, 0u));
    return;
  }

  uint32_t bytes_to_read =
      std::min(base::checked_cast<uint32_t>(request.buf_len),
               base::saturated_cast<uint32_t>(file_info.size - request.offset));

  GetMediaTransferProtocolManager()->ReadFileChunk(
      device_handle_, request.file_id,
      base::checked_cast<uint32_t>(request.offset), bytes_to_read,
      base::Bind(&MTPDeviceTaskHelper::OnDidReadBytes,
                 weak_ptr_factory_.GetWeakPtr(), request, file_info));
}

void MTPDeviceTaskHelper::OnDidReadBytes(
    const MTPDeviceAsyncDelegate::ReadBytesRequest& request,
    const base::File::Info& file_info,
    const std::string& data,
    bool error) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (error) {
    return HandleDeviceError(request.error_callback,
                             base::File::FILE_ERROR_FAILED);
  }

  CHECK_LE(base::checked_cast<int>(data.length()), request.buf_len);
  std::copy(data.begin(), data.end(), request.buf->data());

  base::PostTask(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(request.success_callback, file_info, data.length()));
}

void MTPDeviceTaskHelper::OnRenameObject(
    const RenameObjectSuccessCallback& success_callback,
    const ErrorCallback& error_callback,
    const bool error) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (error) {
    base::PostTask(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(error_callback, base::File::FILE_ERROR_FAILED));
    return;
  }

  base::PostTask(FROM_HERE, {content::BrowserThread::IO}, success_callback);
}

void MTPDeviceTaskHelper::OnCopyFileFromLocal(
    const CopyFileFromLocalSuccessCallback& success_callback,
    const ErrorCallback& error_callback,
    const bool error) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (error) {
    base::PostTask(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(error_callback, base::File::FILE_ERROR_FAILED));
    return;
  }

  base::PostTask(FROM_HERE, {content::BrowserThread::IO}, success_callback);
}

void MTPDeviceTaskHelper::OnDeleteObject(
    const DeleteObjectSuccessCallback& success_callback,
    const ErrorCallback& error_callback,
    const bool error) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (error) {
    base::PostTask(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(error_callback, base::File::FILE_ERROR_FAILED));
    return;
  }

  base::PostTask(FROM_HERE, {content::BrowserThread::IO}, success_callback);
}

void MTPDeviceTaskHelper::HandleDeviceError(
    const ErrorCallback& error_callback,
    base::File::Error error) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::PostTask(FROM_HERE, {content::BrowserThread::IO},
                 base::BindOnce(error_callback, error));
}
