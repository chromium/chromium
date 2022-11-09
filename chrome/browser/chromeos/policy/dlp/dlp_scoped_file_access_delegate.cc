// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_scoped_file_access_delegate.h"

#include "base/process/process_handle.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_access_copy_or_move_delegate_factory.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace policy {

namespace {

dlp::RequestFileAccessRequest PrepareBaseRequestFileAccessRequest(
    const std::vector<base::FilePath>& files) {
  dlp::RequestFileAccessRequest request;
  for (const auto& file : files)
    request.add_files_paths(file.value());

  request.set_process_id(base::GetCurrentProcId());
  return request;
}

}  // namespace

// static
void DlpScopedFileAccessDelegate::Initialize(chromeos::DlpClient* client) {
  if (!HasInstance()) {
    new DlpScopedFileAccessDelegate(client);
  }
}

DlpScopedFileAccessDelegate::DlpScopedFileAccessDelegate(
    chromeos::DlpClient* client)
    : client_(client), weak_ptr_factory_(this) {
  DlpFileAccessCopyOrMoveDelegateFactory::Initialize();
}

DlpScopedFileAccessDelegate::~DlpScopedFileAccessDelegate() {
  DlpFileAccessCopyOrMoveDelegateFactory::DeleteInstance();
}

void DlpScopedFileAccessDelegate::RequestFilesAccess(
    const std::vector<base::FilePath>& files,
    const GURL& destination_url,
    base::OnceCallback<void(file_access::ScopedFileAccess)> callback) {
  if (!client_->IsAlive()) {
    std::move(callback).Run(file_access::ScopedFileAccess::Allowed());
    return;
  }

  dlp::RequestFileAccessRequest request =
      PrepareBaseRequestFileAccessRequest(files);
  request.set_destination_url(destination_url.spec());

  PostRequestFileAccessToDaemon(request, std::move(callback));
}

void DlpScopedFileAccessDelegate::RequestFilesAccessForSystem(
    const std::vector<base::FilePath>& files,
    base::OnceCallback<void(file_access::ScopedFileAccess)> callback) {
  if (!client_->IsAlive()) {
    std::move(callback).Run(file_access::ScopedFileAccess::Allowed());
    return;
  }

  dlp::RequestFileAccessRequest request =
      PrepareBaseRequestFileAccessRequest(files);
  request.set_destination_component(dlp::DlpComponent::SYSTEM);

  PostRequestFileAccessToDaemon(request, std::move(callback));
}

void DlpScopedFileAccessDelegate::PostRequestFileAccessToDaemon(
    const ::dlp::RequestFileAccessRequest request,
    base::OnceCallback<void(file_access::ScopedFileAccess)> callback) {
  // base::Unretained is safe as |client_| (global dbus singleton) outlives the
  // usage of |callback|.
  auto dbus_cb = base::BindOnce(
      &chromeos::DlpClient::RequestFileAccess, base::Unretained(client_),
      request,
      base::BindOnce(&DlpScopedFileAccessDelegate::OnResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(dbus_cb));
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
