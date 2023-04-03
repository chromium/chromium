// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/test/test_ambient_managed_photo_source.h"

#include <vector>

#include "base/files/file_path.h"

namespace ash {

TestAmbientManagedPhotoSource::TestAmbientManagedPhotoSource() = default;
TestAmbientManagedPhotoSource::~TestAmbientManagedPhotoSource() = default;

void TestAmbientManagedPhotoSource::SetImagesForTesting(
    const std::vector<base::FilePath>& images_file_paths) {
  images_file_paths_ = images_file_paths;
  if (callback_) {
    callback_.Run(images_file_paths_);
  }
}

std::vector<base::FilePath>
TestAmbientManagedPhotoSource::GetScreensaverImages() {
  return images_file_paths_;
}

void TestAmbientManagedPhotoSource::SetScreensaverImagesUpdatedCallback(
    ScreensaverImagesRepeatingCallback callback) {
  callback_ = std::move(callback);
}

}  // namespace ash
