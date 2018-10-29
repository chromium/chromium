// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_share_path.h"

#include "base/atomic_ref_count.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/chromeos_features.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/concierge/service.pb.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/seneschal_client.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"

namespace {

void OnSeneschalSharePathResponse(
    base::FilePath path,
    base::OnceCallback<void(bool, std::string)> callback,
    base::Optional<vm_tools::seneschal::SharePathResponse> response) {
  if (!response) {
    std::move(callback).Run(false, "System error");
    return;
  }
  std::move(callback).Run(response.value().success(),
                          response.value().failure_reason());
}

void CallSeneschalSharePath(
    Profile* profile,
    std::string vm_name,
    const base::FilePath path,
    bool persist,
    base::OnceCallback<void(bool, std::string)> callback) {
  // Verify path is in one of the allowable mount points.
  // This logic is similar to DownloadPrefs::SanitizeDownloadTargetPath().
  if (!path.IsAbsolute() || path.ReferencesParent()) {
    std::move(callback).Run(false, "Path must be absolute");
    return;
  }

  vm_tools::seneschal::SharePathRequest request;
  base::FilePath drivefs_path;
  base::FilePath relative_path;
  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(profile);
  base::FilePath drivefs_mount_point_path;
  base::FilePath drivefs_mount_name;

  // Allow download directory and subdirs.
  bool allowed_path = false;
  base::FilePath downloads =
      file_manager::util::GetDownloadsFolderForProfile(profile);
  if (downloads == path || downloads.AppendRelativePath(path, &relative_path)) {
    allowed_path = true;
    request.set_storage_location(
        vm_tools::seneschal::SharePathRequest::DOWNLOADS);
    request.set_owner_id(crostini::CryptohomeIdForProfile(profile));
  } else if (base::FeatureList::IsEnabled(chromeos::features::kDriveFs) &&
             integration_service &&
             (drivefs_mount_point_path =
                  integration_service->GetMountPointPath())
                 .AppendRelativePath(path, &drivefs_path) &&
             base::FilePath("/media/fuse")
                 .AppendRelativePath(drivefs_mount_point_path,
                                     &drivefs_mount_name)) {
    // Allow subdirs of DriveFS except .Trash.
    request.set_drivefs_mount_name(drivefs_mount_name.value());
    base::FilePath root("root");
    base::FilePath team_drives("team_drives");
    base::FilePath computers("Computers");
    base::FilePath trash(".Trash");  // Not to be shared!
    if (root == drivefs_path ||
        root.AppendRelativePath(drivefs_path, &relative_path)) {
      // My Drive and subdirs.
      allowed_path = true;
      request.set_storage_location(
          vm_tools::seneschal::SharePathRequest::DRIVEFS_MY_DRIVE);
    } else if (team_drives == drivefs_path ||
               team_drives.AppendRelativePath(drivefs_path, &relative_path)) {
      // Team Drives and subdirs.
      allowed_path = true;
      request.set_storage_location(
          vm_tools::seneschal::SharePathRequest::DRIVEFS_TEAM_DRIVES);
    } else if (computers == drivefs_path ||
               computers.AppendRelativePath(drivefs_path, &relative_path)) {
      // Computers and subdirs.
      allowed_path = true;
      request.set_storage_location(
          vm_tools::seneschal::SharePathRequest::DRIVEFS_COMPUTERS);
    } else if (trash == drivefs_path || trash.IsParent(drivefs_path)) {
      // Note: Do not expose .Trash which would allow linux apps to make
      // permanent deletes from Drive.  This branch is not especially required,
      // but is included to make it explicit that .Trash should not be shared.
      allowed_path = false;
    }
  } else if (base::FilePath("/media/removable")
                 .AppendRelativePath(path, &relative_path)) {
    // Allow subdirs of /media/removable.
    allowed_path = true;
    request.set_storage_location(
        vm_tools::seneschal::SharePathRequest::REMOVABLE);
  }

  if (!allowed_path) {
    std::move(callback).Run(false, "Path is not allowed");
    return;
  }

  // We will not make a blocking call to verify the path exists since
  // we don't want to block, and seneschal must verify this regardless.

  // Even if VM is not running, save to prefs now.
  if (persist) {
    PrefService* pref_service = profile->GetPrefs();
    ListPrefUpdate update(pref_service, crostini::prefs::kCrostiniSharedPaths);
    base::ListValue* shared_paths = update.Get();
    shared_paths->Append(std::make_unique<base::Value>(path.value()));
  }

  // VM must be running.
  // TODO(joelhockey): Start VM if not currently running.
  base::Optional<vm_tools::concierge::VmInfo> vm_info =
      crostini::CrostiniManager::GetForProfile(profile)->GetVmInfo(
          std::move(vm_name));
  if (!vm_info) {
    std::move(callback).Run(false, "VM not running");
    return;
  }

  request.set_handle(vm_info->seneschal_server_handle());
  request.mutable_shared_path()->set_path(relative_path.value());
  request.mutable_shared_path()->set_writable(true);
  chromeos::DBusThreadManager::Get()->GetSeneschalClient()->SharePath(
      request, base::BindOnce(&OnSeneschalSharePathResponse, std::move(path),
                              std::move(callback)));
}

// Barrier Closure that captures the first instance of error.
class ErrorCapture {
 public:
  ErrorCapture(int num_callbacks_left,
               base::OnceCallback<void(bool, std::string)> callback)
      : num_callbacks_left_(num_callbacks_left),
        callback_(std::move(callback)) {
    DCHECK_GE(num_callbacks_left, 0);
    if (num_callbacks_left == 0)
      std::move(callback_).Run(true, "");
  }

  void Run(base::FilePath path, bool success, std::string failure_reason) {
    if (!success) {
      LOG(ERROR) << "Error SharePath=" << path.value()
                 << ", FailureReason=" << failure_reason;
      if (success_) {
        success_ = false;
        first_failure_reason_ = failure_reason;
      }
    }

    if (!num_callbacks_left_.Decrement())
      std::move(callback_).Run(success_, first_failure_reason_);
  }

 private:
  base::AtomicRefCount num_callbacks_left_;
  base::OnceCallback<void(bool, std::string)> callback_;
  bool success_ = true;
  std::string first_failure_reason_;
};  // class

}  // namespace

namespace crostini {

void SharePath(Profile* profile,
               std::string vm_name,
               const base::FilePath& path,
               bool persist,
               base::OnceCallback<void(bool, std::string)> callback) {
  DCHECK(profile);
  DCHECK(callback);
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kCrostiniFiles)) {
    std::move(callback).Run(false, "Flag crostini-files not enabled");
  }
  CallSeneschalSharePath(profile, vm_name, path, persist, std::move(callback));
}

void SharePaths(Profile* profile,
                std::string vm_name,
                std::vector<base::FilePath> paths,
                bool persist,
                base::OnceCallback<void(bool, std::string)> callback) {
  DCHECK(profile);

  base::RepeatingCallback<void(base::FilePath, bool, std::string)> barrier =
      base::BindRepeating(
          &ErrorCapture::Run,
          base::Owned(new ErrorCapture(paths.size(), std::move(callback))));
  for (const auto& path : paths) {
    CallSeneschalSharePath(profile, kCrostiniDefaultVmName, path, persist,
                           base::BindOnce(barrier, std::move(path)));
  }
}

void UnsharePath(Profile* profile,
                 std::string vm_name,
                 const base::FilePath& path,
                 base::OnceCallback<void(bool, std::string)> callback) {
  PrefService* pref_service = profile->GetPrefs();
  ListPrefUpdate update(pref_service, crostini::prefs::kCrostiniSharedPaths);
  base::ListValue* shared_paths = update.Get();
  shared_paths->Remove(base::Value(path.value()), nullptr);
  // TODO(joelhockey): Call to seneschal once UnsharePath is supported,
  // and update FilesApp to watch for changes.
  std::move(callback).Run(true, "");
}

std::vector<base::FilePath> GetPersistedSharedPaths(Profile* profile) {
  DCHECK(profile);
  std::vector<base::FilePath> result;
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kCrostiniFiles)) {
    return result;
  }
  const base::ListValue* shared_paths =
      profile->GetPrefs()->GetList(prefs::kCrostiniSharedPaths);
  for (const auto& path : *shared_paths) {
    result.emplace_back(base::FilePath(path.GetString()));
  }
  return result;
}

void SharePersistedPaths(Profile* profile,
                         base::OnceCallback<void(bool, std::string)> callback) {
  SharePaths(profile, kCrostiniDefaultVmName, GetPersistedSharedPaths(profile),
             false /* persist */, std::move(callback));
}

}  // namespace crostini
