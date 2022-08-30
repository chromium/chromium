// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_scoped_file_access_delegate.h"

#include "base/process/process_handle.h"
#include "chromeos/dbus/dlp/dlp_client.h"

namespace policy {

// static
void DlpScopedFileAccessDelegate::Initialize(chromeos::DlpClient* client) {
  if (!HasInstance()) {
    new DlpScopedFileAccessDelegate(client);
  }
}

DlpScopedFileAccessDelegate::DlpScopedFileAccessDelegate(
    chromeos::DlpClient* client)
    : client_(client) {}

void DlpScopedFileAccessDelegate::RequestFilesAccess(
    const std::vector<base::FilePath>& files,
    const GURL& destination_url,
    base::OnceCallback<void(file_access::ScopedFileAccess)> callback) {
  if (!client_->IsAlive()) {
    std::move(callback).Run(file_access::ScopedFileAccess::Allowed());
    return;
  }

  dlp::RequestFileAccessRequest request;
  for (const auto& file : files)
    request.add_files_paths(file.value());

  request.set_destination_url(destination_url.spec());
  request.set_process_id(base::GetCurrentProcId());
  client_->RequestFileAccess(
      request, base::BindOnce(&DlpScopedFileAccessDelegate::OnResponse,
                              base::Unretained(this), std::move(callback)));
}

void DlpScopedFileAccessDelegate::OnResponse(
    base::OnceCallback<void(file_access::ScopedFileAccess)> callback,
    const dlp::RequestFileAccessResponse response,
    base::ScopedFD fd) {
  if (response.has_error_message()) {
    std::move(callback).Run(file_access::ScopedFileAccess::Allowed());
    return;
  }
  std::move(callback).Run(
      file_access::ScopedFileAccess(response.allowed(), std::move(fd)));
}

}  // namespace policy
