// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOBSTER_LOBSTER_IMAGE_ACTUATOR_H_
#define ASH_LOBSTER_LOBSTER_IMAGE_ACTUATOR_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/lobster/lobster_image_download_response.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "ui/base/ime/text_input_client.h"
#include "url/gurl.h"

namespace ash {

void ASH_EXPORT CopyToClipboard(const std::string& image_bytes);

bool ASH_EXPORT InsertImageOrCopyToClipboard(ui::TextInputClient* input_client,
                                             const std::string& image_bytes);

void ASH_EXPORT
WriteImageToPath(const base::FilePath& save_dir,
                 const std::string& query,
                 uint32_t id,
                 const std::string& image_bytes,
                 LobsterImageDownloadResponseCallback status_callback);

}  // namespace ash

#endif  // ASH_LOBSTER_LOBSTER_IMAGE_ACTUATOR_H_
