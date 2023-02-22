
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/file_upload_impl.h"

#include <string>

#include "base/strings/string_piece.h"
#include "components/reporting/util/status.h"

namespace reporting {
FileUploadDelegate::FileUploadDelegate() = default;

Status FileUploadDelegate::DoInitiate(base::StringPiece origin_path,
                                      base::StringPiece upload_parameters,
                                      int64_t* total,
                                      std::string* session_token) {
  return Status(error::UNIMPLEMENTED, "Not yet implemented");
}

Status FileUploadDelegate::DoNextStep(int64_t total,
                                      int64_t* uploaded,
                                      std::string* session_token) {
  return Status(error::UNIMPLEMENTED, "Not yet implemented");
}

Status FileUploadDelegate::DoFinalize(base::StringPiece session_token,
                                      std::string* access_parameters) {
  return Status(error::UNIMPLEMENTED, "Not yet implemented");
}
}  // namespace reporting
