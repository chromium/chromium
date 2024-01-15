// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/phonehub/camera_roll_download_manager_impl.h"

#include <optional>
#include <utility>

#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/safe_base_name.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/clamped_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/camera_roll_download_manager.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"

namespace ash {
namespace phonehub {

namespace {

const size_t kBytesPerKilobyte = 1024;

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

std::optional<secure_channel::mojom::PayloadFilesPtr> DoCreatePayloadFiles(
    const base::FilePath& file_path) {
  if (base::PathExists(file_path)) {
    // Perhaps a file was created at the same path for a different payload after
    // we checkedfor its existence.
    return std::nullopt;
  }

  base::File output_file =
      base::File(file_path, base::File::Flags::FLAG_CREATE_ALWAYS |
                                base::File::Flags::FLAG_WRITE);
  base::File input_file = base::File(
      file_path, base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ);
  return secure_channel::mojom::PayloadFiles::New(std::move(input_file),
                                                  std::move(output_file));
}

}  // namespace

CameraRollDownloadManagerImpl::DownloadItem::DownloadItem(
    int64_t payload_id,
    const base::FilePath& file_path,
    int64_t file_size_bytes,
    const std::string& holding_space_item_id)
    : payload_id(payload_id),
      file_path(file_path),
      file_size_bytes(file_size_bytes),
      holding_space_item_id(holding_space_item_id) {}

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
  std::optional<base::SafeBaseName> base_name(
      base::SafeBaseName::Create(item_metadata.file_name()));
  if (!base_name || base_name->path().value() != item_metadata.file_name()) {
    PA_LOG(WARNING) << "Camera roll item file name "
                    << item_metadata.file_name() << " is not a valid base name";
    return std::move(payload_files_callback)
        .Run(CreatePayloadFilesResult::kInvalidFileName, std::nullopt);
  }

  if (pending_downloads_.contains(payload_id)) {
    PA_LOG(WARNING) << "Payload " << payload_id
                    << " is already being downloaded";
    return std::move(payload_files_callback)
        .Run(CreatePayloadFilesResult::kPayloadAlreadyExists, std::nullopt);
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
        .Run(CreatePayloadFilesResult::kInsufficientDiskSpace, std::nullopt);
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
    std::optional<secure_channel::mojom::PayloadFilesPtr> payload_files) {
  if (!payload_files) {
    PA_LOG(WARNING) << "Failed to create files for payload " << payload_id
                    << ": the requested file path already exists.";
    return std::move(payload_files_callback)
        .Run(CreatePayloadFilesResult::kNotUniqueFilePath, std::nullopt);
  }

  const std::string& holding_space_item_id =
      holding_space_keyed_service_->AddItemOfType(
          HoldingSpaceItem::Type::kPhoneHubCameraRoll, file_path,
          ash::HoldingSpaceProgress(/*current_bytes=*/0,
                                    /*total_bytes=*/file_size_bytes));
  if (holding_space_item_id.empty()) {
    // This can happen if a file was created at the same path for a previous
    // payload but then got deleted before that payload is fully downloaded.
    PA_LOG(WARNING) << "Failed to add payload " << payload_id
                    << " to holding space. It's likely that an item with the "
                       "same file path already exists.";
    return std::move(payload_files_callback)
        .Run(CreatePayloadFilesResult::kNotUniqueFilePath, std::nullopt);
  }

  pending_downloads_.emplace(
      payload_id, DownloadItem(payload_id, file_path, file_size_bytes,
                               holding_space_item_id));

  std::move(payload_files_callback)
      .Run(CreatePayloadFilesResult::kSuccess, std::move(payload_files));
}

void CameraRollDownloadManagerImpl::UpdateDownloadProgress(
    secure_channel::mojom::FileTransferUpdatePtr update) {
  auto it = pending_downloads_.find(update->payload_id);
  if (it == pending_downloads_.end()) {
    PA_LOG(ERROR) << "Received unexpected FileTransferUpdate for payload "
                  << update->payload_id;
    return;
  }

  const DownloadItem& download_item = it->second;
  const std::string holding_space_item_id = download_item.holding_space_item_id;

  switch (update->status) {
    case secure_channel::mojom::FileTransferStatus::kInProgress:
      holding_space_keyed_service_->UpdateItem(holding_space_item_id)
          ->SetProgress(ash::HoldingSpaceProgress(update->bytes_transferred,
                                                  update->total_bytes,
                                                  /*complete=*/false));
      return;
    case secure_channel::mojom::FileTransferStatus::kFailure:
    case secure_channel::mojom::FileTransferStatus::kCanceled:
      // Delete files created for failed and canceled items, in addition to
      // removing the DownloadItem objects.
      holding_space_keyed_service_->RemoveItem(holding_space_item_id);
      DeleteFile(update->payload_id);
      return;
    case secure_channel::mojom::FileTransferStatus::kSuccess:
      holding_space_keyed_service_
          ->UpdateItem(download_item.holding_space_item_id)
          ->SetProgress(ash::HoldingSpaceProgress(update->bytes_transferred,
                                                  update->total_bytes,
                                                  /*complete=*/true))
          .SetInvalidateImage(true);
      base::UmaHistogramCounts100000(
          "PhoneHub.CameraRoll.DownloadItem.TransferRate",
          CalculateItemTransferRate(download_item));
      pending_downloads_.erase(it);
  }
}

int CameraRollDownloadManagerImpl::CalculateItemTransferRate(
    const DownloadItem& download_item) const {
  base::TimeDelta total_download_time =
      base::TimeTicks::Now() - download_item.start_timestamp;
  base::ClampedNumeric bytes_per_second = base::ClampDiv(
      download_item.file_size_bytes, total_download_time.InSecondsF());
  return base::saturated_cast<int>(
      base::ClampDiv(bytes_per_second, kBytesPerKilobyte));
}

void CameraRollDownloadManagerImpl::DeleteFile(int64_t payload_id) {
  auto it = pending_downloads_.find(payload_id);
  if (it == pending_downloads_.end()) {
    return;
  }

  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](const base::FilePath& file_path, int64_t payload_id) {
                       if (!base::DeleteFile(file_path)) {
                         PA_LOG(WARNING) << "Failed to delete file for payload "
                                         << payload_id;
                       }
                     },
                     it->second.file_path, payload_id));
  pending_downloads_.erase(it);
}

}  // namespace phonehub
}  // namespace ash
