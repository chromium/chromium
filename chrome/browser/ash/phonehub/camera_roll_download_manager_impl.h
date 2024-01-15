// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PHONEHUB_CAMERA_ROLL_DOWNLOAD_MANAGER_IMPL_H_
#define CHROME_BROWSER_ASH_PHONEHUB_CAMERA_ROLL_DOWNLOAD_MANAGER_IMPL_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/safe_base_name.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"
#include "chromeos/ash/components/phonehub/camera_roll_download_manager.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel_types.mojom.h"

namespace ash {
namespace phonehub {

// CameraRollDownloadManager implementation.
class CameraRollDownloadManagerImpl
    : public phonehub::CameraRollDownloadManager {
 public:
  CameraRollDownloadManagerImpl(
      const base::FilePath& download_path,
      ash::HoldingSpaceKeyedService* holding_space_keyed_service);
  ~CameraRollDownloadManagerImpl() override;

  // CameraRollDownloadManager:
  void CreatePayloadFiles(
      int64_t payload_id,
      const proto::CameraRollItemMetadata& item_metadata,
      CreatePayloadFilesCallback payload_files_callback) override;
  void UpdateDownloadProgress(
      secure_channel::mojom::FileTransferUpdatePtr update) override;
  void DeleteFile(int64_t payload_id) override;

 private:
  // Internal representation of an item being downloaded.
  struct DownloadItem {
    DownloadItem(int64_t payload_id,
                 const base::FilePath& file_path,
                 int64_t file_size_bytes,
                 const std::string& holding_space_item_id);
    DownloadItem(DownloadItem&&) = default;
    DownloadItem& operator=(DownloadItem&&) = default;
    ~DownloadItem();

    int64_t payload_id;
    base::FilePath file_path;
    int64_t file_size_bytes;
    std::string holding_space_item_id;
    base::TimeTicks start_timestamp = base::TimeTicks::Now();
  };

  void OnDiskSpaceCheckComplete(
      const base::SafeBaseName& base_name,
      int64_t payload_id,
      int64_t file_size_bytes,
      CreatePayloadFilesCallback payload_files_callback,
      bool has_enough_disk_space);
  void OnUniquePathFetched(int64_t payload_id,
                           int64_t file_size_bytes,
                           CreatePayloadFilesCallback payload_files_callback,
                           const base::FilePath& unique_path);
  void OnPayloadFilesCreated(
      int64_t payload_id,
      const base::FilePath& file_path,
      int64_t file_size_bytes,
      CreatePayloadFilesCallback payload_files_callback,
      std::optional<secure_channel::mojom::PayloadFilesPtr> payload_files);
  int CalculateItemTransferRate(const DownloadItem& download_item) const;

  const base::FilePath download_path_;
  raw_ptr<ash::HoldingSpaceKeyedService> holding_space_keyed_service_;
  // Performs blocking I/O operations for creating and deleting payload files.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Item downloads that are still in progress, keyed by payload IDs.
  base::flat_map<int64_t, DownloadItem> pending_downloads_;

  base::WeakPtrFactory<CameraRollDownloadManagerImpl> weak_ptr_factory_{this};
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PHONEHUB_CAMERA_ROLL_DOWNLOAD_MANAGER_IMPL_H_
