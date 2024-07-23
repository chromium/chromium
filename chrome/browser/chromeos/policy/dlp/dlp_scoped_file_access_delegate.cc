// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_scoped_file_access_delegate.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/task/bind_post_task.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_access_copy_or_move_delegate_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "components/enterprise/data_controls/core/browser/dlp_histogram_helper.h"
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace policy {

namespace {
dlp::RequestFileAccessRequest PrepareBaseRequestFileAccessRequest(
    const std::vector<base::FilePath>& files) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  dlp::RequestFileAccessRequest request;
  for (const auto& file : files)
    request.add_files_paths(file.value());

  request.set_process_id(base::GetCurrentProcId());
  return request;
}

void RequestFileAccessForSystem(
    const std::vector<base::FilePath>& files,
    base::OnceCallback<void(file_access::ScopedFileAccess)> callback,
    bool check_default) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (file_access::ScopedFileAccessDelegate::HasInstance()) {
    if (check_default) {
      file_access::ScopedFileAccessDelegate::Get()->RequestDefaultFilesAccess(
          files, base::BindPostTask(content::GetIOThreadTaskRunner({}),
                                    std::move(callback)));
    } else {
      file_access::ScopedFileAccessDelegate::Get()->RequestFilesAccessForSystem(
          files, base::BindPostTask(content::GetIOThreadTaskRunner({}),
                                    std::move(callback)));
    }
  } else {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  file_access::ScopedFileAccess::Allowed()));
  }
}

// Returns true if `file_path` is in MyFiles directory.
bool IsInLocalFileSystem(const base::FilePath& file_path) {
  base::FilePath my_files_folder;
  base::PathService::Get(chrome::DIR_USER_DOCUMENTS, &my_files_folder);
  if (my_files_folder == file_path || my_files_folder.IsParent(file_path)) {
    return true;
  }
  return false;
}

void ReportDefaultFileAccessUMA(bool is_deny,
                                const std::vector<base::FilePath>& files) {
  for (const auto& file : files) {
    if (IsInLocalFileSystem(file)) {
      if (is_deny) {
        data_controls::DlpHistogramEnumeration(
            data_controls::dlp::kFilesDefaultFileAccess,
            DlpScopedFileAccessDelegate::DefaultAccess::kMyFilesDeny);
      } else {
        data_controls::DlpHistogramEnumeration(
            data_controls::dlp::kFilesDefaultFileAccess,
            DlpScopedFileAccessDelegate::DefaultAccess::kMyFilesAllow);
      }
    } else {
      if (is_deny) {
        data_controls::DlpHistogramEnumeration(
            data_controls::dlp::kFilesDefaultFileAccess,
            DlpScopedFileAccessDelegate::DefaultAccess::kSystemFilesDeny);
      } else {
        data_controls::DlpHistogramEnumeration(
            data_controls::dlp::kFilesDefaultFileAccess,
            DlpScopedFileAccessDelegate::DefaultAccess::kSystemFilesAllow);
      }
    }
  }
}

}  // namespace

// static
void DlpScopedFileAccessDelegate::Initialize(
    DlpScopedFileAccessDelegate::DlpClientProvider client_provider) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!HasInstance()) {
    new DlpScopedFileAccessDelegate(std::move(client_provider));
  }
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce([]() {
        if (!request_files_access_for_system_io_callback_) {
          request_files_access_for_system_io_callback_ =
              new file_access::ScopedFileAccessDelegate::
                  RequestFilesAccessCheckDefaultCallback(base::BindPostTask(
                      content::GetUIThreadTaskRunner({}),
                      base::BindRepeating(&RequestFileAccessForSystem)));
        }
      }));
}

DlpScopedFileAccessDelegate::DlpScopedFileAccessDelegate(
    DlpScopedFileAccessDelegate::DlpClientProvider client_provider)
    : client_provider_(std::move(client_provider)), weak_ptr_factory_(this) {
  CHECK(!client_provider_.is_null());
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DlpFileAccessCopyOrMoveDelegateFactory::Initialize();
}

DlpScopedFileAccessDelegate::~DlpScopedFileAccessDelegate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DlpFileAccessCopyOrMoveDelegateFactory::DeleteInstance();
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce([]() {
        if (request_files_access_for_system_io_callback_) {
          delete request_files_access_for_system_io_callback_;
          request_files_access_for_system_io_callback_ = nullptr;
        }
      }));
}

void DlpScopedFileAccessDelegate::RequestFilesAccess(
    const std::vector<base::FilePath>& files,
    const GURL& destination_url,
    base::OnceCallback<void(file_access::ScopedFileAccess)> callback) {
  CHECK(destination_url.is_valid());
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* client = client_provider_.Run();
  if (!client || !client->IsAlive()) {
    std::move(callback).Run(file_access::ScopedFileAccess::Allowed());
    return;
  }

  dlp::RequestFileAccessRequest request =
      PrepareBaseRequestFileAccessRequest(files);
  request.set_destination_url(destination_url.spec());

  PostRequestFileAccessToDaemon(client, request, std::move(callback));
}

void DlpScopedFileAccessDelegate::RequestFilesAccessForSystem(
    const std::vector<base::FilePath>& files,
    base::OnceCallback<void(file_access::ScopedFileAccess)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto* client = client_provider_.Run();
  if (!client || !client->IsAlive()) {
    std::move(callback).Run(file_access::ScopedFileAccess::Allowed());
    return;
  }

  dlp::RequestFileAccessRequest request =
      PrepareBaseRequestFileAccessRequest(files);
  request.set_destination_component(dlp::DlpComponent::SYSTEM);

  PostRequestFileAccessToDaemon(client, request, std::move(callback));
}

void DlpScopedFileAccessDelegate::RequestDefaultFilesAccess(
    const std::vector<base::FilePath>& files,
    base::OnceCallback<void(file_access::ScopedFileAccess)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (chromeos::features::IsDataControlsFileAccessDefaultDenyEnabled()) {
    // With is_allowed set the caller will not deny anything themself but the
    // daemon will block the access as no request was sent to it.
    std::move(callback).Run(file_access::ScopedFileAccess::Allowed());
    ReportDefaultFileAccessUMA(/*is_deny=*/true, files);
    return;
  }
  RequestFilesAccessForSystem(files, std::move(callback));
  ReportDefaultFileAccessUMA(/*is_deny=*/false, files);
}

DlpScopedFileAccessDelegate::RequestFilesAccessIOCallback
DlpScopedFileAccessDelegate::CreateFileAccessCallback(
    const GURL& destination) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return base::BindPostTask(
      content::GetUIThreadTaskRunner({}),
      base::BindRepeating(
          [](const GURL& destination, const std::vector<base::FilePath>& files,
             base::OnceCallback<void(file_access::ScopedFileAccess)> callback) {
            if (file_access::ScopedFileAccessDelegate::HasInstance()) {
              file_access::ScopedFileAccessDelegate::Get()->RequestFilesAccess(
                  files, destination,
                  base::BindPostTask(content::GetIOThreadTaskRunner({}),
                                     std::move(callback)));
            } else {
              content::GetIOThreadTaskRunner({})->PostTask(
                  FROM_HERE,
                  base::BindOnce(std::move(callback),
                                 file_access::ScopedFileAccess::Allowed()));
            }
          },
          destination));
}

void DlpScopedFileAccessDelegate::PostRequestFileAccessToDaemon(
    chromeos::DlpClient* client,
    const ::dlp::RequestFileAccessRequest request,
    base::OnceCallback<void(file_access::ScopedFileAccess)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  client->RequestFileAccess(
      request,
      base::BindOnce(&DlpScopedFileAccessDelegate::OnResponse,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DlpScopedFileAccessDelegate::OnResponse(
    base::OnceCallback<void(file_access::ScopedFileAccess)> callback,
    const dlp::RequestFileAccessResponse response,
    base::ScopedFD fd) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (response.has_error_message()) {
    std::move(callback).Run(file_access::ScopedFileAccess::Allowed());
    return;
  }

  std::move(callback).Run(
      file_access::ScopedFileAccess(response.allowed(), std::move(fd)));
}

}  // namespace policy
