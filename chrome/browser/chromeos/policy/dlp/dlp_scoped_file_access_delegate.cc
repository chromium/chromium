// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_scoped_file_access_delegate.h"

#include <sys/stat.h>

#include "base/process/process_handle.h"
#include "chromeos/dbus/dlp/dlp_client.h"

namespace policy {

namespace {

static DlpScopedFileAccessDelegate* g_delegate = nullptr;

ino_t GetInodeValue(const base::FilePath& path) {
  struct stat file_stats;
  if (stat(path.value().c_str(), &file_stats) != 0)
    return 0;
  return file_stats.st_ino;
}

}  // namespace

// static
DlpScopedFileAccessDelegate* DlpScopedFileAccessDelegate::Get() {
  return g_delegate;
}

// static
void DlpScopedFileAccessDelegate::Initialize(chromeos::DlpClient* client) {
  g_delegate = new DlpScopedFileAccessDelegate(client);
}

// static
void DlpScopedFileAccessDelegate::DeleteInstance() {
  delete g_delegate;
  g_delegate = nullptr;
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
  for (const auto& file : files) {
    auto inode_n = GetInodeValue(file);
    if (inode_n > 0) {
      request.add_inodes(inode_n);
    }
  }
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
