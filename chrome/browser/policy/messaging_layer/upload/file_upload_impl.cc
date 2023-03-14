
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/file_upload_impl.h"

#include <string>

#include "base/strings/string_piece.h"
#include "components/reporting/util/status.h"

namespace reporting {
FileUploadDelegate::FileUploadDelegate() = default;

void FileUploadDelegate::DoInitiate(
    base::StringPiece origin_path,
    base::StringPiece upload_parameters,
    base::OnceCallback<void(
        StatusOr<std::pair<int64_t /*total*/, std::string /*session_token*/>>)>
        cb) {
  std::move(cb).Run(Status(error::UNIMPLEMENTED, "Not yet implemented"));
}

void FileUploadDelegate::DoNextStep(
    int64_t total,
    int64_t uploaded,
    base::StringPiece session_token,
    base::OnceCallback<void(StatusOr<std::pair<int64_t /*uploaded*/,
                                               std::string /*session_token*/>>)>
        cb) {
  std::move(cb).Run(Status(error::UNIMPLEMENTED, "Not yet implemented"));
}

void FileUploadDelegate::DoFinalize(
    base::StringPiece session_token,
    base::OnceCallback<void(StatusOr<std::string /*access_parameters*/>)> cb) {
  std::move(cb).Run(Status(error::UNIMPLEMENTED, "Not yet implemented"));
}
}  // namespace reporting
