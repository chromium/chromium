// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PROJECTOR_APP_PENDING_SCREENCAST_H_
#define ASH_WEBUI_PROJECTOR_APP_PENDING_SCREENCAST_H_

#include <set>
#include <string>

#include "ash/webui/projector_app/public/mojom/projector_types.mojom.h"
#include "base/files/file_path.h"
#include "base/time/time.h"

namespace ash {

// A container class that keeps the ash::projector::mojom::ProjectorScreencast
// instance and its associated information (e.g. the folder)
class PendingScreencastContainer {
 public:
  PendingScreencastContainer();
  explicit PendingScreencastContainer(const base::FilePath& container_dir);
  PendingScreencastContainer(const base::FilePath& container_dir,
                             const std::string& name,
                             int64_t total_size_in_bytes,
                             int64_t bytes_transferred);
  PendingScreencastContainer(const PendingScreencastContainer&);
  PendingScreencastContainer& operator=(const PendingScreencastContainer&);
  ~PendingScreencastContainer();

  void SetTotalSizeInBytes(int64_t size);
  void SetTotalBytesTransferred(int64_t size);
  void SetName(const std::string& name);
  void SetCreatedTime(base::Time created_time);

  // Simple setters.
  void set_container_dir(base::FilePath container_dir) {
    container_dir_ = container_dir;
  }
  void set_upload_failed(bool upload_failed) {
    pending_screencast_.upload_failed = upload_failed;
  }

  // Simple getters.
  const ash::projector::mojom::PendingScreencast& pending_screencast() const {
    return pending_screencast_;
  }
  int64_t total_size_in_bytes() const { return total_size_in_bytes_; }
  int64_t bytes_transferred() const { return bytes_transferred_; }
  const base::FilePath& container_dir() const { return container_dir_; }

  bool operator==(const PendingScreencastContainer& rhs) const;

 private:
  void UpdatePendingScreencast();

  // The container path of the screencast. It's a relative path of drive, looks
  // like "/root/projector_data/abc".
  base::FilePath container_dir_;

  // The total size of a screencast in bytes, including the media file and the
  // metadata file under `container_dir`.
  int64_t total_size_in_bytes_ = 0;

  // The bytes have been transferred to drive.
  int64_t bytes_transferred_ = 0;

  ash::projector::mojom::PendingScreencast pending_screencast_;
};

struct PendingScreencastContainerSetComparator {
  bool operator()(const PendingScreencastContainer& a,
                  const PendingScreencastContainer& b) const;
};

// The set to store pending screencasts.
using PendingScreencastContainerSet =
    std::set<PendingScreencastContainer,
             PendingScreencastContainerSetComparator>;

}  // namespace ash

#endif  // ASH_WEBUI_PROJECTOR_APP_PENDING_SCREENCAST_H_
