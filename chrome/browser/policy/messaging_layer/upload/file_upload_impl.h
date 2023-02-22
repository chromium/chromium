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
  Status DoInitiate(base::StringPiece origin_path,        // IN
                    base::StringPiece upload_parameters,  // IN
                    int64_t* total,                       // OUT
                    std::string* session_token            // OUT
                    ) override;

  Status DoNextStep(int64_t total,              // IN
                    int64_t* uploaded,          // INOUT
                    std::string* session_token  // INOUT
                    ) override;

  Status DoFinalize(base::StringPiece session_token,  // IN
                    std::string* access_parameters    // OUT
                    ) override;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_FILE_UPLOAD_IMPL_H_
