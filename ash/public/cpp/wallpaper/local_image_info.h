// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WALLPAPER_LOCAL_IMAGE_INFO_H_
#define ASH_PUBLIC_CPP_WALLPAPER_LOCAL_IMAGE_INFO_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/files/file_path.h"
#include "base/unguessable_token.h"

namespace ash {

struct ASH_PUBLIC_EXPORT LocalImageInfo {
  LocalImageInfo();
  LocalImageInfo(base::UnguessableToken id, const base::FilePath& path);
  ~LocalImageInfo();

  // The unique identifier for the local image.
  base::UnguessableToken id;
  // The file path of the local image.
  base::FilePath path;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WALLPAPER_LOCAL_IMAGE_INFO_H_
