// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_DRIVEFS_DRIVEFS_HOST_OBSERVER_H_
#define ASH_COMPONENTS_DRIVEFS_DRIVEFS_HOST_OBSERVER_H_

#include <vector>

namespace drivefs {
namespace mojom {
class DriveError;
class FileChange;
class SyncingStatus;
}  // namespace mojom

class DriveFsHostObserver {
 public:
  virtual void OnUnmounted() {}
  virtual void OnSyncingStatusUpdate(const mojom::SyncingStatus& status) {}
  virtual void OnMirrorSyncingStatusUpdate(const mojom::SyncingStatus& status) {
  }
  virtual void OnFilesChanged(const std::vector<mojom::FileChange>& changes) {}
  virtual void OnError(const mojom::DriveError& error) {}

 protected:
  ~DriveFsHostObserver() = default;
};

}  // namespace drivefs

#endif  // ASH_COMPONENTS_DRIVEFS_DRIVEFS_HOST_OBSERVER_H_
