// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_SCOPED_FILE_ACCESS_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_SCOPED_FILE_ACCESS_DELEGATE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "url/gurl.h"

namespace chromeos {
class DlpClient;
}  // namespace chromeos

namespace policy {

// Delegate class to proxy file access requests to DLP daemon over D-Bus when
// DLP Files restrictions should apply.
class DlpScopedFileAccessDelegate
    : public file_access::ScopedFileAccessDelegate {
 public:
  ~DlpScopedFileAccessDelegate() override = default;

  // Initializes the singleton instance.
  static void Initialize(chromeos::DlpClient* client);

  // Requests access to |files| in order to be sent to |destination_url|.
  // |continuation_callback| is called with a token that should be hold until
  // `open()` operation on the files finished.
  void RequestFilesAccess(
      const std::vector<base::FilePath>& files,
      const GURL& destination_url,
      base::OnceCallback<void(file_access::ScopedFileAccess)> callback)
      override;

 protected:
  explicit DlpScopedFileAccessDelegate(chromeos::DlpClient* client);

 private:
  friend class DlpScopedFileAccessDelegateTest;
  // Handles D-Bus response to access files.
  void OnResponse(
      base::OnceCallback<void(file_access::ScopedFileAccess)> callback,
      const ::dlp::RequestFileAccessResponse response,
      base::ScopedFD fd);

  raw_ptr<chromeos::DlpClient> client_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_SCOPED_FILE_ACCESS_DELEGATE_H_
