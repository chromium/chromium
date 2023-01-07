// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/chromeos/mtp_device_task_helper.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
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
                                      OpenStorageCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!storage_name.empty());
  if (!device_handle_.empty()) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
    return;
  }

  const std::string mode =
      read_only ? mtpd::kReadOnlyMode : mtpd::kReadWriteMode;
  GetMediaTransferProtocolManager()->OpenStorage(
      storage_name, mode,
      base::BindOnce(&MTPDeviceTaskHelper::OnDidOpenStorage,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void MTPDeviceTaskHelper::GetFileInfo(
    uint32_t file_id,
    GetFileInfoSuccessCallback success_callback,
    ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (device_handle_.empty())
    return HandleDeviceError(std::move(error_callback),
                             base::File::FILE_ERROR_FAILED);

  const std::vector<uint32_t> file_ids = {file_id};
  GetMediaTransferProtocolManager()->GetFileInfo(
      device_handle_, file_ids,
      base::BindOnce(&MTPDeviceTaskHelper::OnGetFileInfo,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(success_callback), std::move(error_callback)));
}

void MTPDeviceTaskHelper::CreateDirectory(
    const uint32_t parent_id,
    const std::string& directory_name,
    CreateDirectorySuccessCallback success_callback,
    ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (device_handle_.empty())
    return HandleDeviceError(std::move(error_callback),
                             base::File::FILE_ERROR_FAILED);

  GetMediaTransferProtocolManager()->CreateDirectory(
      device_handle_, parent_id, directory_name,
      base::BindOnce(&MTPDeviceTaskHelper::OnCreateDirectory,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(success_callback), std::move(error_callback)));
}

void MTPDeviceTaskHelper::ReadDirectory(
    const uint32_t directory_id,
    ReadDirectorySuccessCallback success_callback,
    ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (device_handle_.empty())
    return HandleDeviceError(std::move(error_callback),
                             base::File::FILE_ERROR_FAILED);

  GetMediaTransferProtocolManager()->ReadDirectoryEntryIds(
      device_handle_, directory_id,
      base::BindOnce(
          &MTPDeviceTaskHelper::OnReadDirectoryEntryIdsToReadDirectory,
          weak_ptr_factory_.GetWeakPtr(), success_callback,
          std::move(error_callback)));
}

void MTPDeviceTaskHelper::CheckDirectoryEmpty(
    uint32_t directory_id,
    CheckDirectoryEmptySuccessCallback success_callback,
    ErrorCallback error_callback) {
  if (device_handle_.empty())
    return HandleDeviceError(std::move(error_callback),
                             base::File::FILE_ERROR_FAILED);

  GetMediaTransferProtocolManager()->ReadDirectoryEntryIds(
      device_handle_, directory_id,
      base::BindOnce(&MTPDeviceTaskHelper::OnCheckedDirectoryEmpty,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(success_callback), std::move(error_callback)));
}

void MTPDeviceTaskHelper::WriteDataIntoSnapshotFile(
    SnapshotRequestInfo request_info,
    const base::File::Info& snapshot_file_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (device_handle_.empty()) {
    return HandleDeviceError(std::move(request_info.error_callback),
                             base::File::FILE_ERROR_FAILED);
  }

  if (!read_file_worker_)
    read_file_worker_ = std::make_unique<MTPReadFileWorker>(device_handle_);
  read_file_worker_->WriteDataIntoSnapshotFile(std::move(request_info),
                                               snapshot_file_info);
}

void MTPDeviceTaskHelper::ReadBytes(
    MTPDeviceAsyncDelegate::ReadBytesRequest request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (device_handle_.empty()) {
    return HandleDeviceError(std::move(request.error_callback),
                             base::File::FILE_ERROR_FAILED);
  }

  const std::vector<uint32_t> file_ids = {request.file_id};
  GetMediaTransferProtocolManager()->GetFileInfo(
      device_handle_, file_ids,
      base::BindOnce(&MTPDeviceTaskHelper::OnGetFileInfoToReadBytes,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request)));
}

void MTPDeviceTaskHelper::RenameObject(
    const uint32_t object_id,
    const std::string& new_name,
    RenameObjectSuccessCallback success_callback,
    ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  GetMediaTransferProtocolManager()->RenameObject(
      device_handle_, object_id, new_name,
      base::BindOnce(&MTPDeviceTaskHelper::OnRenameObject,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(success_callback), std::move(error_callback)));
}

MTPDeviceTaskHelper::MTPEntry::MTPEntry() : file_id(0) {}

// TODO(yawano) storage_name is not used, delete it.
void MTPDeviceTaskHelper::CopyFileFromLocal(
    const std::string& storage_name,
    const int source_file_descriptor,
    const uint32_t parent_id,
    const std::string& file_name,
    CopyFileFromLocalSuccessCallback success_callback,
    ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  GetMediaTransferProtocolManager()->CopyFileFromLocal(
      device_handle_, source_file_descriptor, parent_id, file_name,
      base::BindOnce(&MTPDeviceTaskHelper::OnCopyFileFromLocal,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(success_callback), std::move(error_callback)));
}

void MTPDeviceTaskHelper::DeleteObject(
    const uint32_t object_id,
    DeleteObjectSuccessCallback success_callback,
    ErrorCallback error_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  GetMediaTransferProtocolManager()->DeleteObject(
      device_handle_, object_id,
      base::BindOnce(&MTPDeviceTaskHelper::OnDeleteObject,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(success_callback), std::move(error_callback)));
}

void MTPDeviceTaskHelper::CloseStorage() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (device_handle_.empty())
    return;
  GetMediaTransferProtocolManager()->CloseStorage(device_handle_,
                                                  base::DoNothing());
}

void MTPDeviceTaskHelper::OnDidOpenStorage(
    OpenStorageCallback completion_callback,
    const std::string& device_handle,
    bool error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  device_handle_ = device_handle;
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(completion_callback), !error));
}

void MTPDeviceTaskHelper::OnGetFileInfo(
    GetFileInfoSuccessCallback success_callback,
    ErrorCallback error_callback,
    std::vector<device::mojom::MtpFileEntryPtr> entries,
    bool error) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (error || entries.size() != 1) {
    return HandleDeviceError(std::move(error_callback),
                             base::File::FILE_ERROR_NOT_FOUND);
  }

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(success_callback),
                     FileInfoFromMTPFileEntry(std::move(entries[0]))));
}

void MTPDeviceTaskHelper::OnCreateDirectory(
    CreateDirectorySuccessCallback success_callback,
    ErrorCallback error_callback,
    const bool error) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (error) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(error_callback),
                                  base::File::FILE_ERROR_FAILED));
    return;
  }

  content::GetIOThreadTaskRunner({})->PostTask(FROM_HERE,
                                               std::move(success_callback));
}

void MTPDeviceTaskHelper::OnReadDirectoryEntryIdsToReadDirectory(
    ReadDirectorySuccessCallback success_callback,
    ErrorCallback error_callback,
    const std::vector<uint32_t>& file_ids,
    bool error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (error)
    return HandleDeviceError(std::move(error_callback),
                             base::File::FILE_ERROR_FAILED);

  if (file_ids.empty()) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(success_callback, MTPEntries(), /*has_more=*/false));
    return;
  }

  std::vector<uint32_t> file_ids_to_read_now;
  std::vector<uint32_t> file_ids_to_read_later;
  SplitFileIds(file_ids, &file_ids_to_read_now, &file_ids_to_read_later);

  GetMediaTransferProtocolManager()->GetFileInfo(
      device_handle_, file_ids_to_read_now,
      base::BindOnce(&MTPDeviceTaskHelper::OnGotDirectoryEntries,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(success_callback), std::move(error_callback),
                     file_ids_to_read_now, file_ids_to_read_later));
}

void MTPDeviceTaskHelper::OnGotDirectoryEntries(
    ReadDirectorySuccessCallback success_callback,
    ErrorCallback error_callback,
    const std::vector<uint32_t>& expected_file_ids,
    const std::vector<uint32_t>& file_ids_to_read,
    std::vector<device::mojom::MtpFileEntryPtr> file_entries,
    bool error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (error)
    return HandleDeviceError(std::move(error_callback),
                             base::File::FILE_ERROR_FAILED);

  // Use |expected_file_ids| to verify the results are the requested ids.
  std::vector<uint32_t> sorted_expected_file_ids = expected_file_ids;
  std::sort(sorted_expected_file_ids.begin(), sorted_expected_file_ids.end());
  for (const auto& entry : file_entries) {
    std::vector<uint32_t>::const_iterator it =
        std::lower_bound(sorted_expected_file_ids.begin(),
                         sorted_expected_file_ids.end(), entry->item_id);
    if (it == sorted_expected_file_ids.end()) {
      return HandleDeviceError(std::move(error_callback),
                               base::File::FILE_ERROR_FAILED);
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

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(success_callback, entries, has_more));

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
                     std::move(error_callback), file_ids_to_read_now,
                     file_ids_to_read_later));
}

void MTPDeviceTaskHelper::OnCheckedDirectoryEmpty(
    CheckDirectoryEmptySuccessCallback success_callback,
    ErrorCallback error_callback,
    const std::vector<uint32_t>& file_ids,
    bool error) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (error)
    return HandleDeviceError(std::move(error_callback),
                             base::File::FILE_ERROR_FAILED);

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(success_callback), file_ids.empty()));
}

void MTPDeviceTaskHelper::OnGetFileInfoToReadBytes(
    MTPDeviceAsyncDelegate::ReadBytesRequest request,
    std::vector<device::mojom::MtpFileEntryPtr> entries,
    bool error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(request.buf.get());
  DCHECK_GE(request.buf_len, 0);
  DCHECK_GE(request.offset, 0);
  if (error || entries.size() != 1) {
    return HandleDeviceError(std::move(request.error_callback),
                             base::File::FILE_ERROR_FAILED);
  }

  base::File::Info file_info = FileInfoFromMTPFileEntry(std::move(entries[0]));
  if (file_info.is_directory) {
    return HandleDeviceError(std::move(request.error_callback),
                             base::File::FILE_ERROR_NOT_A_FILE);
  }
  if (file_info.size < 0 ||
      file_info.size > std::numeric_limits<uint32_t>::max() ||
      request.offset > file_info.size) {
    return HandleDeviceError(std::move(request.error_callback),
                             base::File::FILE_ERROR_FAILED);
  }
  if (request.offset == file_info.size) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(request.success_callback), file_info, 0u));
    return;
  }

  uint32_t bytes_to_read =
      std::min(base::checked_cast<uint32_t>(request.buf_len),
               base::saturated_cast<uint32_t>(file_info.size - request.offset));
  auto file_id = request.file_id;
  auto offset = base::checked_cast<uint32_t>(request.offset);
  GetMediaTransferProtocolManager()->ReadFileChunk(
      device_handle_, file_id, offset, bytes_to_read,
      base::BindOnce(&MTPDeviceTaskHelper::OnDidReadBytes,
                     weak_ptr_factory_.GetWeakPtr(), std::move(request),
                     file_info));
}

void MTPDeviceTaskHelper::OnDidReadBytes(
    MTPDeviceAsyncDelegate::ReadBytesRequest request,
    const base::File::Info& file_info,
    const std::string& data,
    bool error) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (error) {
    return HandleDeviceError(std::move(request.error_callback),
                             base::File::FILE_ERROR_FAILED);
  }

  CHECK_LE(base::checked_cast<int>(data.length()), request.buf_len);
  base::ranges::copy(data, request.buf->data());

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(request.success_callback), file_info,
                                data.length()));
}

void MTPDeviceTaskHelper::OnRenameObject(
    RenameObjectSuccessCallback success_callback,
    ErrorCallback error_callback,
    const bool error) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (error) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(error_callback),
                                  base::File::FILE_ERROR_FAILED));
    return;
  }

  content::GetIOThreadTaskRunner({})->PostTask(FROM_HERE,
                                               std::move(success_callback));
}

void MTPDeviceTaskHelper::OnCopyFileFromLocal(
    CopyFileFromLocalSuccessCallback success_callback,
    ErrorCallback error_callback,
    const bool error) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (error) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(error_callback),
                                  base::File::FILE_ERROR_FAILED));
    return;
  }

  content::GetIOThreadTaskRunner({})->PostTask(FROM_HERE,
                                               std::move(success_callback));
}

void MTPDeviceTaskHelper::OnDeleteObject(
    DeleteObjectSuccessCallback success_callback,
    ErrorCallback error_callback,
    const bool error) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (error) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(error_callback),
                                  base::File::FILE_ERROR_FAILED));
    return;
  }

  content::GetIOThreadTaskRunner({})->PostTask(FROM_HERE,
                                               std::move(success_callback));
}

void MTPDeviceTaskHelper::HandleDeviceError(ErrorCallback error_callback,
                                            base::File::Error error) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(error_callback), error));
}
