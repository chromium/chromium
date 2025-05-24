// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_LOBSTER_LOBSTER_IMAGE_DOWNLOAD_RESPONSE_H_
#define ASH_PUBLIC_CPP_LOBSTER_LOBSTER_IMAGE_DOWNLOAD_RESPONSE_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"

namespace ash {

struct LobsterImageDownloadResponse {
  base::FilePath download_path;
  bool success;
};

using LobsterImageDownloadResponseCallback =
    base::OnceCallback<void(const ash::LobsterImageDownloadResponse&)>;

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_LOBSTER_LOBSTER_IMAGE_DOWNLOAD_RESPONSE_H_
