// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/projector/test/mock_projector_client.h"

namespace ash {

MockProjectorClient::MockProjectorClient() {
  const bool result = screencast_container_path_.CreateUniqueTempDir();
  DCHECK(result);
}

MockProjectorClient::~MockProjectorClient() = default;

bool MockProjectorClient::GetDriveFsMountPointPath(
    base::FilePath* result) const {
  *result = screencast_container_path_.GetPath();
  return true;
}

bool MockProjectorClient::IsSelfieCamVisible() const {
  return is_selfie_cam_visible_;
}

void MockProjectorClient::SetSelfieCamVisible(bool visible) {
  is_selfie_cam_visible_ = visible;
}

}  // namespace ash
