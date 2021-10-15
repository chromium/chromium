// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/phonehub/camera_roll_download_manager_impl.h"

#include <utility>

#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/safe_base_name.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/components/phonehub/proto/phonehub_api.pb.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace phonehub {

namespace {

scoped_refptr<base::SequencedTaskRunner> CreateTaskRunner() {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
}

}  // namespace

CameraRollDownloadManagerImpl::DownloadItem::DownloadItem(
    int64_t payload_id,
    const base::FilePath& file_path)
    : payload_id(payload_id), file_path(file_path) {}

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
    const chromeos::phonehub::proto::CameraRollItemMetadata& item_metadata,
    CreatePayloadFilesCallback payload_files_callback) {
  absl::optional<base::SafeBaseName> base_name(
      base::SafeBaseName::Create(item_metadata.file_name()));
  if (!base_name || base_name->path().value() != item_metadata.file_name()) {
    PA_LOG(WARNING) << "Camera roll item file name "
                    << item_metadata.file_name() << " is not a valid base name";
    return std::move(payload_files_callback).Run(absl::nullopt);
  }

  if (pending_downloads_.contains(payload_id)) {
    PA_LOG(WARNING) << "Payload " << payload_id
                    << " is already being downloaded";
    return std::move(payload_files_callback).Run(absl::nullopt);
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&CameraRollDownloadManagerImpl::DoCreatePayloadFiles,
                     weak_ptr_factory_.GetWeakPtr(), payload_id,
                     base_name->path(), std::move(payload_files_callback)));
}

void CameraRollDownloadManagerImpl::DoCreatePayloadFiles(
    int64_t payload_id,
    const base::FilePath& base_name,
    CreatePayloadFilesCallback payload_files_callback) {
  base::FilePath file_path =
      base::GetUniquePath(download_path_.Append(base_name));
  pending_downloads_.emplace(payload_id, DownloadItem(payload_id, file_path));

  base::File output_file =
      base::File(file_path, base::File::Flags::FLAG_CREATE_ALWAYS |
                                base::File::Flags::FLAG_WRITE);
  base::File input_file = base::File(
      file_path, base::File::Flags::FLAG_OPEN | base::File::Flags::FLAG_READ);
  std::move(payload_files_callback)
      .Run(absl::make_optional(
          chromeos::secure_channel::mojom::PayloadFiles::New(
              std::move(input_file), std::move(output_file))));
}

void CameraRollDownloadManagerImpl::UpdateDownloadProgress(
    chromeos::secure_channel::mojom::FileTransferUpdatePtr update) {
  auto it = pending_downloads_.find(update->payload_id);
  if (it == pending_downloads_.end()) {
    PA_LOG(ERROR) << "Received unexpected FileTransferUpdate for payload "
                  << update->payload_id;
    return;
  }

  DownloadItem& download_item = it->second;
  if (download_item.holding_space_item_id.empty()) {
    download_item.holding_space_item_id =
        holding_space_keyed_service_->AddPhoneHubCameraRollItem(
            download_item.file_path,
            ash::HoldingSpaceProgress(update->bytes_transferred,
                                      update->total_bytes));
  } else {
    holding_space_keyed_service_
        ->UpdateItem(download_item.holding_space_item_id)
        ->SetProgress(ash::HoldingSpaceProgress(update->bytes_transferred,
                                                update->total_bytes))
        .SetInvalidateImage(
            update->status ==
            chromeos::secure_channel::mojom::FileTransferStatus::kSuccess);
  }

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
