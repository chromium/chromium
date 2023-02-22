// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_FILE_UPLOAD_IMPL_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_FILE_UPLOAD_IMPL_H_

#include <string>

#include "base/strings/string_piece.h"
#include "chrome/browser/policy/messaging_layer/upload/file_upload_job.h"
#include "components/reporting/util/status.h"

namespace reporting {

class FileUploadDelegate : public FileUploadJob::Delegate {
 public:
  FileUploadDelegate();

 private:
  // Delegate implementation.
  // Asynchronously initializes upload.
  // Calls back with `total` and `session_token` are set, or Status in case
  // of error.
  void DoInitiate(
      base::StringPiece origin_path,
      base::StringPiece upload_parameters,
      base::OnceCallback<void(
          StatusOr<std::pair<int64_t /*total*/,
                             std::string /*session_token*/>>)> cb) override;

  // Asynchronously uploads the next chunk.
  // Calls back with new `uploaded` and `session_token` (could be the same),
  // or Status in case of an error.
  void DoNextStep(
      int64_t total,
      int64_t uploaded,
      base::StringPiece session_token,
      base::OnceCallback<void(
          StatusOr<std::pair<int64_t /*uploaded*/,
                             std::string /*session_token*/>>)> cb) override;

  // Asynchronously finalizes upload (once `uploaded` reached `total`).
  // Calls back with `access_parameters`, or Status in case of error.
  void DoFinalize(
      base::StringPiece session_token,
      base::OnceCallback<void(StatusOr<std::string /*access_parameters*/>)> cb)
      override;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_FILE_UPLOAD_IMPL_H_
