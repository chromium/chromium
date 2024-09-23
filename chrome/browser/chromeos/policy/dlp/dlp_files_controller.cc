// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_files_controller.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/task/bind_post_task.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_scoped_file_access_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "components/enterprise/data_controls/core/browser/component.h"
#include "components/file_access/scoped_file_access_copy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

namespace policy {
namespace {
// FileSystemContext instance set for testing.
std::optional<storage::FileSystemContext*> g_file_system_context_for_testing =
    std::nullopt;

// This callback is used when we copy a file within the internal filesystem
// (Downloads / MyFiles). It is called after the source URL of the source file
// is retrieved. It creates a callback `delayed_add_file` and requests the
// ScopedFileAccess for the copy operation. To this access token the
// `delayed_add_file` callback is added so it is called after the copy operation
// finishes.
void GotFilesSourcesOfCopy(
    storage::FileSystemURL destination,
    ::dlp::RequestFileAccessRequest file_access_request,
    base::OnceCallback<void(std::unique_ptr<file_access::ScopedFileAccess>)>
        result_callback,
    const ::dlp::GetFilesSourcesResponse response) {
  if (response.files_metadata_size() == 0) {
    std::move(result_callback)
        .Run(std::make_unique<file_access::ScopedFileAccess>(
            file_access::ScopedFileAccess::Allowed()));
    return;
  }
  DCHECK(response.files_metadata_size() == 1);
  if (!chromeos::DlpClient::Get() || !chromeos::DlpClient::Get()->IsAlive()) {
    std::move(result_callback)
        .Run(std::make_unique<file_access::ScopedFileAccess>(
            file_access::ScopedFileAccess::Allowed()));
    return;
  }

  if (!response.files_metadata().Get(0).has_source_url() ||
      response.files_metadata().Get(0).source_url().empty()) {
    std::move(result_callback)
        .Run(std::make_unique<file_access::ScopedFileAccess>(
            file_access::ScopedFileAccess::Allowed()));
    return;
  }

  ::dlp::AddFilesRequest request;
  ::dlp::AddFileRequest* add_request = request.add_add_file_requests();
  add_request->set_file_path(destination.path().value());
  add_request->set_source_url(response.files_metadata().Get(0).source_url());
  add_request->set_referrer_url(
      response.files_metadata().Get(0).referrer_url());

  // The callback will be invoked with the destruction of the
  // ScopedFileAccessCopy object
  base::OnceCallback<void()> delayed_add_file = base::BindPostTask(
      base::SingleThreadTaskRunner::GetCurrentDefault(),
      base::BindOnce(
          [](::dlp::AddFilesRequest&& request) {
            // TODO(https://crbug.com/1368497): we might want to use the
            // callback for error handling.
            chromeos::DlpClient::Get()->AddFiles(request, base::DoNothing());
          },
          std::move(request)));

  chromeos::DlpClient::RequestFileAccessCallback add_file_callback =
      base::BindOnce(
          [](base::OnceCallback<void(
                 std::unique_ptr<file_access::ScopedFileAccess>)>
                 result_callback,
             base::OnceCallback<void()> delayed_add_file,
             const ::dlp::RequestFileAccessResponse response,
             base::ScopedFD fd) {
            std::move(result_callback)
                .Run(std::make_unique<file_access::ScopedFileAccessCopy>(
                    response.allowed(), std::move(fd),
                    std::move(delayed_add_file)));
          },
          std::move(result_callback), std::move(delayed_add_file));

  chromeos::DlpClient::Get()->RequestFileAccess(file_access_request,
                                                std::move(add_file_callback));
}

// Converts DataTransferEndpoint object to DlpFileDestination.
DlpFileDestination DTEndpointToFileDestination(
    const ui::DataTransferEndpoint* endpoint) {
  DCHECK(endpoint);

  switch (endpoint->type()) {
    case ui::EndpointType::kUrl:
      DCHECK(endpoint->GetURL());
      return DlpFileDestination(*endpoint->GetURL());

    case ui::EndpointType::kArc:
      return DlpFileDestination(data_controls::Component::kArc);

    case ui::EndpointType::kCrostini:
      return DlpFileDestination(data_controls::Component::kCrostini);

    case ui::EndpointType::kPluginVm:
      return DlpFileDestination(data_controls::Component::kPluginVm);

    case ui::EndpointType::kLacros:
    case ui::EndpointType::kDefault:
    case ui::EndpointType::kClipboardHistory:
    case ui::EndpointType::kBorealis:
    case ui::EndpointType::kUnknownVm:
      return DlpFileDestination(data_controls::Component::kUnknownComponent);
  }
}

// Converts file paths to file system URLs.
std::vector<storage::FileSystemURL> ConvertLocalFilePathsToFileSystemUrls(
    const storage::FileSystemContext& file_system_context,
    const std::vector<base::FilePath>& paths) {
  std::vector<storage::FileSystemURL> file_system_urls;

  for (const auto& path : paths) {
    file_system_urls.push_back(file_system_context.CreateCrackedFileSystemURL(
        blink::StorageKey(), storage::kFileSystemTypeLocal, path));
  }

  return file_system_urls;
}

}  // namespace

DlpFilesController::FileDaemonInfo::FileDaemonInfo(
    ino64_t inode,
    time_t crtime,
    const base::FilePath& path,
    const std::string& source_url,
    const std::string& referrer_url)
    : inode(inode),
      crtime(crtime),
      path(path),
      source_url(source_url),
      referrer_url(referrer_url) {}

DlpFilesController::FileDaemonInfo::FileDaemonInfo(const FileDaemonInfo& o)
    : inode(o.inode),
      crtime(o.crtime),
      path(o.path),
      source_url(o.source_url),
      referrer_url(o.referrer_url) {}

DlpFilesController::FolderRecursionDelegate::FolderRecursionDelegate(
    storage::FileSystemContext* file_system_context,
    const storage::FileSystemURL& root,
    FileURLsCallback callback)
    : RecursiveOperationDelegate(file_system_context),
      root_(root),
      callback_(std::move(callback)) {}

DlpFilesController::FolderRecursionDelegate::~FolderRecursionDelegate() =
    default;

void DlpFilesController::FolderRecursionDelegate::Run() {
  NOTREACHED_IN_MIGRATION();
}

void DlpFilesController::FolderRecursionDelegate::RunRecursively() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  StartRecursiveOperation(*root_,
                          storage::FileSystemOperation::ERROR_BEHAVIOR_SKIP,
                          base::BindOnce(&FolderRecursionDelegate::Completed,
                                         weak_ptr_factory_.GetWeakPtr()));
}
void DlpFilesController::FolderRecursionDelegate::ProcessFile(
    const storage::FileSystemURL& url,
    StatusCallback callback) {
  file_system_context()->operation_runner()->GetMetadata(
      url, {storage::FileSystemOperation::GetMetadataField::kIsDirectory},
      base::BindOnce(&FolderRecursionDelegate::OnGetMetadata,
                     weak_ptr_factory_.GetWeakPtr(), url, std::move(callback)));
}
void DlpFilesController::FolderRecursionDelegate::ProcessDirectory(
    const storage::FileSystemURL& url,
    StatusCallback callback) {
  std::move(callback).Run(base::File::FILE_OK);
}
void DlpFilesController::FolderRecursionDelegate::PostProcessDirectory(
    const storage::FileSystemURL& url,
    StatusCallback callback) {
  std::move(callback).Run(base::File::FILE_OK);
}
base::WeakPtr<storage::RecursiveOperationDelegate>
DlpFilesController::FolderRecursionDelegate::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void DlpFilesController::FolderRecursionDelegate::OnGetMetadata(
    const storage::FileSystemURL& url,
    StatusCallback callback,
    base::File::Error result,
    const base::File::Info& file_info) {
  if (result != base::File::FILE_OK) {
    std::move(callback).Run(result);
    return;
  }
  if (file_info.is_directory) {
    std::move(callback).Run(base::File::FILE_ERROR_NOT_A_FILE);
    return;
  }
  files_urls_.push_back(url);
  std::move(callback).Run(base::File::FILE_OK);
}

void DlpFilesController::FolderRecursionDelegate::Completed(
    base::File::Error result) {
  std::move(callback_).Run(std::move(files_urls_));
}

DlpFilesController::RootsRecursionDelegate::RootsRecursionDelegate(
    storage::FileSystemContext* file_system_context,
    std::vector<storage::FileSystemURL> roots,
    DlpFilesController::FolderRecursionDelegate::FileURLsCallback callback)
    : file_system_context_(file_system_context),
      roots_(std::move(roots)),
      callback_(std::move(callback)) {}

DlpFilesController::RootsRecursionDelegate::~RootsRecursionDelegate() = default;

void DlpFilesController::RootsRecursionDelegate::Run() {
  for (const auto& root : roots_) {
    auto recursion_delegate = std::make_unique<FolderRecursionDelegate>(
        file_system_context_, root,
        base::BindOnce(&RootsRecursionDelegate::Completed,
                       weak_ptr_factory_.GetWeakPtr()));
    recursion_delegate->RunRecursively();
    delegates_.push_back(std::move(recursion_delegate));
  }
}

void DlpFilesController::RootsRecursionDelegate::Completed(
    std::vector<storage::FileSystemURL> files_urls) {
  counter_++;
  files_urls_.insert(std::end(files_urls_), std::begin(files_urls),
                     std::end(files_urls));
  if (counter_ == roots_.size()) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback_), std::move(files_urls_)));
    content::GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE, this);
  }
}

DlpFilesController::DlpFilesController(const DlpRulesManager& rules_manager)
    : rules_manager_(rules_manager) {}

DlpFilesController::~DlpFilesController() = default;

void DlpFilesController::RequestCopyAccess(
    const storage::FileSystemURL& source_file,
    const storage::FileSystemURL& destination,
    base::OnceCallback<void(std::unique_ptr<file_access::ScopedFileAccess>)>
        result_callback) {
  if (!chromeos::DlpClient::Get() || !chromeos::DlpClient::Get()->IsAlive()) {
    std::move(result_callback)
        .Run(std::make_unique<file_access::ScopedFileAccess>(
            file_access::ScopedFileAccess::Allowed()));
    return;
  }
  Profile* profile = ProfileManager::GetPrimaryUserProfile();

  std::optional<data_controls::Component> dst_component =
      MapFilePathToPolicyComponent(profile, destination.path());
  std::optional<data_controls::Component> src_component =
      MapFilePathToPolicyComponent(profile, source_file.path());

  // Copy from external is not limited by DLP.
  // TODO(b/297190245): currently there is no component for mounted archives and
  // they are considered as not in the local file system so we end up in the if
  // below when a file is copied from a mounted archive. When mounting of
  // restricted archives is supported, we however need to apply the restriction
  // of the source archive to the copied files and not just always allow as
  // below.
  if (src_component.has_value() || !IsInLocalFileSystem(source_file.path())) {
    std::move(result_callback)
        .Run(std::make_unique<file_access::ScopedFileAccess>(
            file_access::ScopedFileAccess::Allowed()));
    return;
  }

  ::dlp::DlpComponent proto =
      dst_component ? dlp::MapPolicyComponentToProto(*dst_component)
                    : ::dlp::DlpComponent::SYSTEM;

  ::dlp::RequestFileAccessRequest file_access_request;
  file_access_request.set_process_id(base::GetCurrentProcId());
  file_access_request.add_files_paths(source_file.path().value());
  file_access_request.set_destination_component(proto);

  if (!dst_component.has_value()) {
    // We allow internal copy, we still have to get the scopedFS
    // and we might need to copy the source URL information.
    if (IsInLocalFileSystem(destination.path())) {
      ::dlp::GetFilesSourcesRequest request;
      request.add_files_paths(source_file.path().value());
      chromeos::DlpClient::Get()->GetFilesSources(
          request,
          base::BindOnce(&GotFilesSourcesOfCopy, destination,
                         file_access_request, std::move(result_callback)));
    } else {
      std::move(result_callback)
          .Run(std::make_unique<file_access::ScopedFileAccess>(
              /*allowed=*/false, base::ScopedFD()));
    }

    return;
  }

  chromeos::DlpClient::Get()->RequestFileAccess(
      file_access_request,
      base::BindOnce(
          [](base::OnceCallback<void(
                 std::unique_ptr<file_access::ScopedFileAccess>)> callback,
             ::dlp::RequestFileAccessResponse res, base::ScopedFD fd) {
            std::move(callback).Run(
                std::make_unique<file_access::ScopedFileAccess>(res.allowed(),
                                                                std::move(fd)));
          },
          std::move(result_callback)));
}

void DlpFilesController::CheckIfPasteOrDropIsAllowed(
    const std::vector<base::FilePath>& files,
    const ui::DataTransferEndpoint* data_dst,
    CheckIfDlpAllowedCallback result_callback) {
  std::vector<base::FilePath> local_files;
  for (const auto& path : files) {
    if (!IsInLocalFileSystem(path)) {
      continue;
    }
    local_files.push_back(path);
  }

  scoped_refptr<storage::FileSystemContext> file_system_context =
      GetFileSystemContextForPrimaryProfile();
  if (!file_system_context) {
    std::move(result_callback).Run(/*is_allowed=*/true);
    return;
  }

  std::vector<storage::FileSystemURL> files_urls =
      ConvertLocalFilePathsToFileSystemUrls(*file_system_context, local_files);
  if (files_urls.empty()) {
    std::move(result_callback).Run(/*is_allowed=*/true);
    return;
  }

  DlpFileDestination destination = DTEndpointToFileDestination(data_dst);

  auto* roots_recursion_delegate = new RootsRecursionDelegate(
      file_system_context.get(), std::move(files_urls),
      base::BindOnce(&DlpFilesController::ContinueCheckIfPasteOrDropIsAllowed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(destination),
                     std::move(result_callback)));
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&RootsRecursionDelegate::Run,
                     // base::Unretained() is safe since |recursion_delegate|
                     // will delete itself after all the files list if ready.
                     base::Unretained(roots_recursion_delegate)));
}

storage::FileSystemContext*
DlpFilesController::GetFileSystemContextForPrimaryProfile() {
  if (g_file_system_context_for_testing.has_value()) {
    return g_file_system_context_for_testing.value();
  }

  Profile* profile = ProfileManager::GetPrimaryUserProfile();

  content::StoragePartition* storage = profile->GetDefaultStoragePartition();
  return storage->GetFileSystemContext();
}

void DlpFilesController::SetFileSystemContextForTesting(
    storage::FileSystemContext* file_system_context) {
  g_file_system_context_for_testing = file_system_context;
}

void DlpFilesController::ContinueCheckIfPasteOrDropIsAllowed(
    const DlpFileDestination& destination,
    CheckIfDlpAllowedCallback result_callback,
    std::vector<storage::FileSystemURL> files) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!chromeos::DlpClient::Get() || !chromeos::DlpClient::Get()->IsAlive()) {
    std::move(result_callback).Run(/*is_allowed=*/true);
    return;
  }

  ::dlp::CheckFilesTransferRequest request;
  for (const auto& file : files) {
    request.add_files_paths(file.path().value());
  }
  if (destination.component().has_value()) {
    request.set_destination_component(
        dlp::MapPolicyComponentToProto(destination.component().value()));
  } else {
    DCHECK(destination.url());
    request.set_destination_url(destination.url()->spec());
  }
  request.set_file_action(::dlp::FileAction::COPY);

  auto return_drop_allowed_cb =
      base::BindOnce(&DlpFilesController::ReturnIfActionAllowed,
                     weak_ptr_factory_.GetWeakPtr(), dlp::FileAction::kCopy,
                     std::move(result_callback));
  chromeos::DlpClient::Get()->CheckFilesTransfer(
      request, std::move(return_drop_allowed_cb));
}

void DlpFilesController::ReturnIfActionAllowed(
    dlp::FileAction action,
    CheckIfDlpAllowedCallback result_callback,
    ::dlp::CheckFilesTransferResponse response) {
  if (response.has_error_message()) {
    LOG(ERROR) << "Failed to get check files transfer, error: "
               << response.error_message();
    std::move(result_callback).Run(/*is_allowed=*/true);
    return;
  }

  if (response.files_paths().empty()) {
    std::move(result_callback).Run(/*is_allowed=*/true);
    return;
  }

  std::vector<base::FilePath> blocked_files(response.files_paths().begin(),
                                            response.files_paths().end());
  ShowDlpBlockedFiles(/*task_id=*/std::nullopt, std::move(blocked_files),
                      action);
  std::move(result_callback).Run(/*is_allowed=*/false);
}

}  // namespace policy
