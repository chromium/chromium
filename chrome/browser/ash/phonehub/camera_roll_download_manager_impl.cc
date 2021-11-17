// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/phonehub/camera_roll_download_manager_impl.h"

#include <utility>

#include "ash/components/phonehub/camera_roll_download_manager.h"
#include "ash/components/phonehub/proto/phonehub_api.pb.h"
#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/safe_base_name.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace phonehub {

namespace {

scoped_refptr<base::SequencedTaskRunner> CreateTaskRunner() {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
}

bool HasEnoughDiskSpace(const base::FilePath& root_path,
                        const proto::CameraRollItemMetadata& item_metadata) {
  DCHECK(item_metadata.file_size_bytes() >= 0);
  return base::SysInfo::AmountOfFreeDiskSpace(root_path) >=
         item_metadata.file_size_bytes();
}

chromeos::secure_channel::mojom::PayloadFilesPtr DoCreatePayloadFiles(
    const base::FilePath& file_path) {
  base::File output_file =
      base::File(file_path, base::File::Flags::FLAG_CREATE_ALWAYS |
                                base::File::Flags::FLAG_WRITE);
  base::File input_file = base::File(
      file_path, base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ);
  return chromeos::secure_channel::mojom::PayloadFiles::New(
      std::move(input_file), std::move(output_file));
}

}  // namespace

CameraRollDownloadManagerImpl::DownloadItem::DownloadItem(
    int64_t payload_id,
    const base::FilePath& file_path,
    const std::string& holding_space_item_id)
    : payload_id(payload_id),
      file_path(file_path),
      holding_space_item_id(holding_space_item_id) {}

CameraRollDownloadManagerImpl::DownloadItem::DownloadItem(
    const CameraRollDownloadManagerImpl::DownloadItem&) = default;

CameraRollDownloadManagerImpl::DownloadItem&
CameraRollDownloadManagerImpl::DownloadItem::operator=(
    const CameraRollDownloadManagerImpl::DownloadItem&) = default;

CameraRollDownloadManagerImpl::DownloadItem::~DownloadItem() = default;

CameraRollDownloadManagerImpl::CameraRollDownloadManagerImpl(
    const base::FilePath& download_path,
    ash::HoldingSpaceKeyedService* holding_space_keyed_service)
    : download_path_(download_path),
      holding_space_keyed_service_(holding_space_keyed_service),
      task_runner_(CreateTaskRunner()) {}

CameraRollDownloadManagerImpl::~CameraRollDownloadManagerImpl() = default;

void CameraRollDownloadManagerImpl::CreatePayloadFiles(
    int64_t payload_id,
    const proto::CameraRollItemMetadata& item_metadata,
    CreatePayloadFilesCallback payload_files_callback) {
  absl::optional<base::SafeBaseName> base_name(
      base::SafeBaseName::Create(item_metadata.file_name()));
  if (!base_name || base_name->path().value() != item_metadata.file_name()) {
    PA_LOG(WARNING) << "Camera roll item file name "
                    << item_metadata.file_name() << " is not a valid base name";
    return std::move(payload_files_callback)
        .Run(CreatePayloadFilesResult::kInvalidFileName, absl::nullopt);
  }

  if (pending_downloads_.contains(payload_id)) {
    PA_LOG(WARNING) << "Payload " << payload_id
                    << " is already being downloaded";
    return std::move(payload_files_callback)
        .Run(CreatePayloadFilesResult::kPayloadAlreadyExists, absl::nullopt);
  }

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&HasEnoughDiskSpace, download_path_, item_metadata),
      base::BindOnce(&CameraRollDownloadManagerImpl::OnDiskSpaceCheckComplete,
                     weak_ptr_factory_.GetWeakPtr(), base_name.value(),
                     payload_id, item_metadata.file_size_bytes(),
                     std::move(payload_files_callback)));
}

void CameraRollDownloadManagerImpl::OnDiskSpaceCheckComplete(
    const base::SafeBaseName& base_name,
    int64_t payload_id,
    int64_t file_size_bytes,
    CreatePayloadFilesCallback payload_files_callback,
    bool has_enough_disk_space) {
  if (!has_enough_disk_space) {
    std::move(payload_files_callback)
        .Run(CreatePayloadFilesResult::kInsufficientDiskSpace, absl::nullopt);
    return;
  }

  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&base::GetUniquePath,
                     download_path_.Append(base_name.path())),
      base::BindOnce(&CameraRollDownloadManagerImpl::OnUniquePathFetched,
                     weak_ptr_factory_.GetWeakPtr(), payload_id,
                     file_size_bytes, std::move(payload_files_callback)));
}

void CameraRollDownloadManagerImpl::OnUniquePathFetched(
    int64_t payload_id,
    int64_t file_size_bytes,
    CreatePayloadFilesCallback payload_files_callback,
    const base::FilePath& unique_path) {
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&DoCreatePayloadFiles, unique_path),
      base::BindOnce(&CameraRollDownloadManagerImpl::OnPayloadFilesCreated,
                     weak_ptr_factory_.GetWeakPtr(), payload_id, unique_path,
                     file_size_bytes, std::move(payload_files_callback)));
}

void CameraRollDownloadManagerImpl::OnPayloadFilesCreated(
    int64_t payload_id,
    const base::FilePath& file_path,
    int64_t file_size_bytes,
    CreatePayloadFilesCallback payload_files_callback,
    chromeos::secure_channel::mojom::PayloadFilesPtr payload_files) {
  const std::string& holding_space_item_id =
      holding_space_keyed_service_->AddPhoneHubCameraRollItem(
          file_path,
          ash::HoldingSpaceProgress(/*current_bytes=*/0,
                                    /*total_bytes=*/file_size_bytes));
  pending_downloads_.emplace(
      payload_id, DownloadItem(payload_id, file_path, holding_space_item_id));

  std::move(payload_files_callback)
      .Run(CreatePayloadFilesResult::kSuccess, std::move(payload_files));
}

void CameraRollDownloadManagerImpl::UpdateDownloadProgress(
    chromeos::secure_channel::mojom::FileTransferUpdatePtr update) {
  auto it = pending_downloads_.find(update->payload_id);
  if (it == pending_downloads_.end()) {
    PA_LOG(ERROR) << "Received unexpected FileTransferUpdate for payload "
                  << update->payload_id;
    return;
  }

  holding_space_keyed_service_->UpdateItem(it->second.holding_space_item_id)
      ->SetProgress(ash::HoldingSpaceProgress(update->bytes_transferred,
                                              update->total_bytes))
      .SetInvalidateImage(
          update->status ==
          chromeos::secure_channel::mojom::FileTransferStatus::kSuccess);

  switch (update->status) {
    case chromeos::secure_channel::mojom::FileTransferStatus::kInProgress:
      return;
    case chromeos::secure_channel::mojom::FileTransferStatus::kFailure:
    case chromeos::secure_channel::mojom::FileTransferStatus::kCanceled:
      // Delete files created for failed and canceled items, in addition to
      // removing the DownloadItem objects.
      DeleteFile(update->payload_id);
      ABSL_FALLTHROUGH_INTENDED;
    case chromeos::secure_channel::mojom::FileTransferStatus::kSuccess:
      pending_downloads_.erase(it);
  }
}

void CameraRollDownloadManagerImpl::DeleteFile(int64_t payload_id) {
  if (pending_downloads_.contains(payload_id)) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(
                       [](const DownloadItem& download_item) {
                         if (!base::DeleteFile(download_item.file_path)) {
                           PA_LOG(WARNING)
                               << "Failed to delete file for payload "
                               << download_item.payload_id;
                         }
                       },
                       pending_downloads_.at(payload_id)));
  }
}

}  // namespace phonehub
}  // namespace ash
