// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_TEST_TEST_AMBIENT_MANAGED_PHOTO_SOURCE_H_
#define ASH_AMBIENT_TEST_TEST_AMBIENT_MANAGED_PHOTO_SOURCE_H_

#include "ash/public/cpp/ambient/ambient_managed_photo_source.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"

namespace ash {

// A test only implementation of the managed photo source to be used
// in unit tests
class TestAmbientManagedPhotoSource : public AmbientManagedPhotoSource {
 public:
  TestAmbientManagedPhotoSource();
  ~TestAmbientManagedPhotoSource() override;

  std::vector<base::FilePath> GetScreensaverImages() override;

  void SetScreensaverImagesUpdatedCallback(
      ScreensaverImagesRepeatingCallback callback) override;

  // Used for setting images for testing
  void SetImagesForTesting(const std::vector<base::FilePath>& images);

 private:
  std::vector<base::FilePath> images_file_paths_;
  ScreensaverImagesRepeatingCallback callback_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_TEST_TEST_AMBIENT_MANAGED_PHOTO_SOURCE_H_
