// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/phonehub/fake_camera_roll_download_manager.h"

#include <utility>
#include <vector>

#include "ash/components/phonehub/camera_roll_download_manager.h"
#include "ash/components/phonehub/proto/phonehub_api.pb.h"
#include "base/containers/flat_map.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel_types.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace phonehub {

FakeCameraRollDownloadManager::FakeCameraRollDownloadManager() = default;

FakeCameraRollDownloadManager::~FakeCameraRollDownloadManager() = default;

void FakeCameraRollDownloadManager::CreatePayloadFiles(
    int64_t payload_id,
    const phonehub::proto::CameraRollItemMetadata& item_metadata,
    CreatePayloadFilesCallback payload_files_callback) {
  if (should_create_payload_files_succeed_) {
    payload_update_map_.emplace(
        payload_id,
        std::vector<chromeos::secure_channel::mojom::FileTransferUpdatePtr>());
    std::move(payload_files_callback)
        .Run(CreatePayloadFilesResult::kSuccess,
             absl::make_optional(
                 chromeos::secure_channel::mojom::PayloadFiles::New()));
  } else {
    std::move(payload_files_callback)
        .Run(CreatePayloadFilesResult::kInvalidFileName, absl::nullopt);
  }
}

void FakeCameraRollDownloadManager::UpdateDownloadProgress(
    chromeos::secure_channel::mojom::FileTransferUpdatePtr update) {
  payload_update_map_.at(update->payload_id).push_back(std::move(update));
}

void FakeCameraRollDownloadManager::DeleteFile(int64_t payload_id) {
  payload_update_map_.erase(payload_id);
}

const std::vector<chromeos::secure_channel::mojom::FileTransferUpdatePtr>&
FakeCameraRollDownloadManager::GetFileTransferUpdates(
    int64_t payload_id) const {
  return payload_update_map_.at(payload_id);
}

}  // namespace phonehub
}  // namespace ash
