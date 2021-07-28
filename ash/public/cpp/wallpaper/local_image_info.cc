// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/wallpaper/local_image_info.h"

#include "base/files/file_path.h"
#include "base/unguessable_token.h"

namespace ash {

LocalImageInfo::LocalImageInfo() = default;
LocalImageInfo::LocalImageInfo(base::UnguessableToken id,
                               const base::FilePath& path)
    : id(id), path(path) {}
LocalImageInfo::~LocalImageInfo() = default;

}  // namespace ash
