// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_SCOPED_FILE_ACCESS_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_SCOPED_FILE_ACCESS_DELEGATE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
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
  using DefaultAccess = file_access::ScopedFileAccessDelegate::DefaultAccess;

  ~DlpScopedFileAccessDelegate() override;

  using DlpClientProvider = base::RepeatingCallback<chromeos::DlpClient*()>;

  // Initializes the singleton instance.
  static void Initialize(DlpClientProvider client_provider);

  // file_access::ScopedFileAccessDelegate:
  void RequestFilesAccess(
      const std::vector<base::FilePath>& files,
      const GURL& destination_url,
      base::OnceCallback<void(file_access::ScopedFileAccess)> callback)
      override;
  void RequestFilesAccessForSystem(
      const std::vector<base::FilePath>& files,
      base::OnceCallback<void(file_access::ScopedFileAccess)> callback)
      override;
  void RequestDefaultFilesAccess(
      const std::vector<base::FilePath>& files,
      base::OnceCallback<void(file_access::ScopedFileAccess)> callback)
      override;
  RequestFilesAccessIOCallback CreateFileAccessCallback(
      const GURL& destination) const override;

 protected:
  explicit DlpScopedFileAccessDelegate(DlpClientProvider client_provider);

 private:
  friend class DlpScopedFileAccessDelegateTest;
  friend class DlpScopedFileAccessDelegateInteractiveUITest;
  friend std::unique_ptr<DlpScopedFileAccessDelegate> std::make_unique<
      DlpScopedFileAccessDelegate>(DlpClientProvider&& client_provider);

  // Starts a RequestFileAccess request to the daemon.
  void PostRequestFileAccessToDaemon(
      chromeos::DlpClient* client,
      const ::dlp::RequestFileAccessRequest request,
      base::OnceCallback<void(file_access::ScopedFileAccess)> callback);

  // Handles D-Bus response to access files.
  void OnResponse(
      base::OnceCallback<void(file_access::ScopedFileAccess)> callback,
      const ::dlp::RequestFileAccessResponse response,
      base::ScopedFD fd);

  const DlpClientProvider client_provider_;

  base::WeakPtrFactory<DlpScopedFileAccessDelegate> weak_ptr_factory_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_SCOPED_FILE_ACCESS_DELEGATE_H_
