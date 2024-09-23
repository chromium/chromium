// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/operation.h"

#include <stdint.h>

#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "chrome/browser/extensions/api/image_writer_private/error_constants.h"
#include "chrome/browser/extensions/api/image_writer_private/image_writer_utility_client.h"
#include "chrome/browser/extensions/api/image_writer_private/operation_manager.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {
namespace image_writer {

using content::BrowserThread;

void Operation::Write(base::OnceClosure continuation) {
  DCHECK(IsRunningInCorrectSequence());
  if (IsCancelled()) {
    return;
  }

  SetStage(image_writer_api::Stage::kWrite);
  StartUtilityClient();

  int64_t file_size;
  if (!base::GetFileSize(image_path_, &file_size)) {
    Error(error::kImageReadError);
    return;
  }

  image_writer_client_->Write(
      base::BindRepeating(&Operation::WriteImageProgress, this, file_size),
      base::BindOnce(&Operation::CompleteAndContinue, this,
                     std::move(continuation)),
      base::BindOnce(&Operation::Error, this), image_path_, device_path_);
}

void Operation::VerifyWrite(base::OnceClosure continuation) {
  DCHECK(IsRunningInCorrectSequence());

  if (IsCancelled()) {
    return;
  }

  SetStage(image_writer_api::Stage::kVerifyWrite);
  StartUtilityClient();

  int64_t file_size;
  if (!base::GetFileSize(image_path_, &file_size)) {
    Error(error::kImageReadError);
    return;
  }

  image_writer_client_->Verify(
      base::BindRepeating(&Operation::WriteImageProgress, this, file_size),
      base::BindOnce(&Operation::CompleteAndContinue, this,
                     std::move(continuation)),
      base::BindOnce(&Operation::Error, this), image_path_, device_path_);
}

}  // namespace image_writer
}  // namespace extensions
