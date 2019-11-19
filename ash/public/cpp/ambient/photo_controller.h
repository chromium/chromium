// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AMBIENT_PHOTO_CONTROLLER_H_
#define ASH_PUBLIC_CPP_AMBIENT_PHOTO_CONTROLLER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/callback_forward.h"
#include "base/macros.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

// Interface for a class which is responsible for managing photos in the ambient
// mode in ash.
class ASH_PUBLIC_EXPORT PhotoController {
 public:
  static PhotoController* Get();

  using PhotoDownloadCallback = base::OnceCallback<void(const gfx::ImageSkia&)>;

  // Get next image.
  virtual void GetNextImage(PhotoDownloadCallback callback) = 0;

 protected:
  PhotoController();
  virtual ~PhotoController();

 private:
  DISALLOW_COPY_AND_ASSIGN(PhotoController);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AMBIENT_PHOTO_CONTROLLER_H_
