// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AMBIENT_AMBIENT_MANAGED_PHOTO_SOURCE_H_
#define ASH_PUBLIC_CPP_AMBIENT_AMBIENT_MANAGED_PHOTO_SOURCE_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"

namespace ash {

// Abstract class responsible for transferring images downloaded by policy from
// chrome to ash. There can only be a single instance of this class or its
// children.
// TODO(b/278873241): remove unused interface
class ASH_PUBLIC_EXPORT AmbientManagedPhotoSource {
 public:
  using ScreensaverImagesRepeatingCallback =
      base::RepeatingCallback<void(const std::vector<base::FilePath>& images)>;

  virtual std::vector<base::FilePath> GetScreensaverImages() = 0;

  // Sets a repeating callback which is run whenever new images are available.
  // In case this method is called multiple times with different callback
  // objects, the last callback object will be used.
  virtual void SetScreensaverImagesUpdatedCallback(
      ScreensaverImagesRepeatingCallback callback) = 0;

 protected:
  AmbientManagedPhotoSource();
  virtual ~AmbientManagedPhotoSource();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AMBIENT_AMBIENT_MANAGED_PHOTO_SOURCE_H_
