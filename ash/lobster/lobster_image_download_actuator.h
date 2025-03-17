// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOBSTER_LOBSTER_IMAGE_DOWNLOAD_ACTUATOR_H_
#define ASH_LOBSTER_LOBSTER_IMAGE_DOWNLOAD_ACTUATOR_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/public/cpp/lobster/lobster_image_download_response.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "ui/base/ime/text_input_client.h"
#include "url/gurl.h"

namespace ash {

class ASH_EXPORT LobsterImageDownloadActuator {
 public:
  LobsterImageDownloadActuator();
  ~LobsterImageDownloadActuator();

  void WriteImageToPath(const base::FilePath& dir,
                        const std::string& query,
                        uint32_t id,
                        const std::string& image_bytes,
                        LobsterImageDownloadResponseCallback status_callback);

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace ash

#endif  // ASH_LOBSTER_LOBSTER_IMAGE_DOWNLOAD_ACTUATOR_H_
