// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_files_controller.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/task/bind_post_task.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_scoped_file_access_delegate.h"
#include "chrome/browser/enterprise/data_controls/component.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "components/file_access/scoped_file_access_copy.h"
#include "storage/browser/file_system/file_system_url.h"

namespace policy {
namespace {
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
                    response.allowed(), base::ScopedFD(),
                    std::move(delayed_add_file)));
          },
          std::move(result_callback), std::move(delayed_add_file));

  chromeos::DlpClient::Get()->RequestFileAccess(file_access_request,
                                                std::move(add_file_callback));
}

// Returns true if `file_path` is in My Files directory.
bool IsInLocalFileSystem(const base::FilePath& file_path) {
  base::FilePath my_files_folder;
  base::PathService::Get(base::DIR_HOME, &my_files_folder);
  if (my_files_folder == file_path || my_files_folder.IsParent(file_path)) {
    return true;
  }
  return false;
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

  absl::optional<data_controls::Component> dst_component =
      MapFilePathToPolicyComponent(profile, destination.path());
  absl::optional<data_controls::Component> src_component =
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

}  // namespace policy
