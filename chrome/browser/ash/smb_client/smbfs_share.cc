// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/smbfs_share.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/smb_client/smb_service_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/smb_shares/smb_credentials_dialog.h"
#include "crypto/sha2.h"
#include "storage/browser/file_system/external_mount_points.h"

namespace ash::smb_client {

namespace {

constexpr char kMountDirPrefix[] = "smbfs-";
constexpr char kMountIdHashSeparator[] = "#";
constexpr base::TimeDelta kAllowCredentialsTimeout = base::Seconds(5);

SmbMountResult MountErrorToMountResult(smbfs::mojom::MountError mount_error) {
  switch (mount_error) {
    case smbfs::mojom::MountError::kOk:
      return SmbMountResult::kSuccess;
    case smbfs::mojom::MountError::kTimeout:
      return SmbMountResult::kAborted;
    case smbfs::mojom::MountError::kInvalidUrl:
      return SmbMountResult::kInvalidUrl;
    case smbfs::mojom::MountError::kInvalidOptions:
      return SmbMountResult::kInvalidOperation;
    case smbfs::mojom::MountError::kNotFound:
      return SmbMountResult::kNotFound;
    case smbfs::mojom::MountError::kAccessDenied:
      return SmbMountResult::kAuthenticationFailed;
    case smbfs::mojom::MountError::kInvalidProtocol:
      return SmbMountResult::kUnsupportedDevice;
    case smbfs::mojom::MountError::kUnknown:
    default:
      return SmbMountResult::kUnknownFailure;
  }
}

}  // namespace

SmbFsShare::SmbFsShare(Profile* profile,
                       const SmbUrl& share_url,
                       const std::string& display_name,
                       const MountOptions& options)
    : profile_(profile),
      share_url_(share_url),
      display_name_(display_name),
      options_(options),
      mount_id_(GenerateStableMountId()) {
  DCHECK(share_url_.IsValid());
}

SmbFsShare::~SmbFsShare() {
  Unmount(base::DoNothing());
}

void SmbFsShare::Mount(SmbFsShare::MountCallback callback) {
  DCHECK(!mounter_);
  DCHECK(!host_);

  if (unmount_pending_) {
    LOG(WARNING) << "Cannot mount a shared that is being unmounted";
    std::move(callback).Run(SmbMountResult::kMountExists);
    return;
  }

  // TODO(amistry): Come up with a scheme for consistent mount paths between
  // sessions.
  const std::string mount_dir = base::StrCat({kMountDirPrefix, mount_id_});
  if (mounter_creation_callback_for_test_) {
    mounter_ = mounter_creation_callback_for_test_.Run(
        share_url_.ToString(), mount_dir, options_, this);
  } else {
    mounter_ = std::make_unique<smbfs::SmbFsMounter>(
        share_url_.ToString(), mount_dir, options_, this,
        disks::DiskMountManager::GetInstance());
  }
  mounter_->Mount(base::BindOnce(&SmbFsShare::OnMountDone,
                                 base::Unretained(this), std::move(callback)));
}

void SmbFsShare::Remount(const MountOptions& options,
                         SmbFsShare::MountCallback callback) {
  if (IsMounted() || unmount_pending_) {
    std::move(callback).Run(SmbMountResult::kMountExists);
    return;
  }

  options_ = options;

  Mount(std::move(callback));
}

void SmbFsShare::DeleteRecursively(
    const base::FilePath& path,
    SmbFsShare::DeleteRecursivelyCallback callback) {
  if (!host_) {
    std::move(callback).Run(base::File::FILE_ERROR_FAILED);
    return;
  }

  // Only one recursive delete operation can be outstanding at any time.
  if (delete_recursively_callback_) {
    LOG(WARNING) << "A recursive delete operation is already in progress";
    std::move(callback).Run(base::File::FILE_ERROR_FAILED);
    return;
  }

  // smbfs should have no visibility into the full path of the file (which
  // includes the FUSE mount point): it sees a filesystem rooted at the base of
  // the mount point path.
  base::FilePath transformed_path("/");
  bool success = mount_path().AppendRelativePath(path, &transformed_path);
  if (!success) {
    LOG(ERROR)
        << "Could not construct absolute path for recursive delete operation";
    std::move(callback).Run(base::File::FILE_ERROR_FAILED);
    return;
  }

  delete_recursively_callback_ = std::move(callback);
  host_->DeleteRecursively(std::move(transformed_path),
                           base::BindOnce(&SmbFsShare::OnDeleteRecursivelyDone,
                                          base::Unretained(this)));
}

void SmbFsShare::OnDeleteRecursivelyDone(base::File::Error error) {
  DCHECK(delete_recursively_callback_);
  std::move(delete_recursively_callback_).Run(error);
}

void SmbFsShare::Unmount(SmbFsShare::UnmountCallback callback) {
  if (unmount_pending_) {
    LOG(WARNING) << "Cannot unmount a shared that is being unmounted";
    std::move(callback).Run(MountError::kInternalError);
    return;
  }

  unmount_pending_ = true;

  // Cancel any pending mount request.
  mounter_.reset();

  if (!host_) {
    LOG(WARNING) << "Cannot unmount as the share is already unmounted";
    std::move(callback).Run(MountError::kPathNotMounted);
    return;
  }

  // Remove volume from VolumeManager. It's critical this is done before
  // revoking the filesystem from ExternalMountPoints as some observers
  // (ie. Crostini) need to create a cracked FileSystemURL (which
  // requires the mount to still be registered with ExternalMountPoints)
  // during the unmount process.
  file_manager::VolumeManager::Get(profile_)->RemoveSmbFsVolume(
      host_->mount_path());

  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  DCHECK(mount_points);
  bool success = mount_points->RevokeFileSystem(mount_id_);
  CHECK(success);

  // Get cros-disks to unmount cleanly. In the process it will kill smbfs which
  // may result in OnDisconnected() being called, but reentrant calls to
  // Unmount() will be aborted as unmount_pending_ == true.
  host_->Unmount(base::BindOnce(&SmbFsShare::OnUnmountDone,
                                base::Unretained(this), std::move(callback)));
}

void SmbFsShare::OnUnmountDone(SmbFsShare::UnmountCallback callback,
                               MountError result) {
  host_.reset();

  // Must do this *after* destroying SmbFsHost so that reentrant calls to
  // Unmount() exit early.
  unmount_pending_ = false;

  // Callback to SmbService::OnSuspendUnmountDone().
  std::move(callback).Run(result);
}

void SmbFsShare::OnMountDone(MountCallback callback,
                             smbfs::mojom::MountError mount_error,
                             std::unique_ptr<smbfs::SmbFsHost> smbfs_host) {
  // Don't need the mounter any more.
  mounter_.reset();

  if (mount_error != smbfs::mojom::MountError::kOk) {
    std::move(callback).Run(MountErrorToMountResult(mount_error));
    return;
  }

  DCHECK(smbfs_host);
  host_ = std::move(smbfs_host);

  storage::ExternalMountPoints* const mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  DCHECK(mount_points);
  bool success = mount_points->RegisterFileSystem(
      mount_id_, storage::kFileSystemTypeSmbFs,
      storage::FileSystemMountOption(), host_->mount_path());
  CHECK(success);

  file_manager::VolumeManager::Get(profile_)->AddSmbFsVolume(
      host_->mount_path(), display_name_);
  std::move(callback).Run(SmbMountResult::kSuccess);
}

void SmbFsShare::OnDisconnected() {
  Unmount(base::DoNothing());

  // At this point, we won't receive any more callbacks from the Mojo host, so
  // run any pending callbacks.
  if (remove_credentials_callback_) {
    LOG(WARNING) << "Mojo disconnected while removing credentials";
    std::move(remove_credentials_callback_).Run(false /* success */);
  }

  if (delete_recursively_callback_) {
    LOG(WARNING)
        << "Mojo disconnected while recursively deleting a path on the share";
    std::move(delete_recursively_callback_).Run(base::File::FILE_ERROR_FAILED);
  }
}

void SmbFsShare::AllowCredentialsRequest() {
  allow_credential_request_ = true;
  allow_credential_request_expiry_ =
      base::TimeTicks::Now() + kAllowCredentialsTimeout;
}

void SmbFsShare::RequestCredentials(RequestCredentialsCallback callback) {
  if (allow_credential_request_expiry_ < base::TimeTicks::Now()) {
    allow_credential_request_ = false;
  }

  if (!allow_credential_request_) {
    std::move(callback).Run(true /* cancel */, "" /* username */,
                            "" /* workgroup */, "" /* password */);
    return;
  }

  smb_dialog::SmbCredentialsDialog::Show(
      mount_id_, share_url_.ToString(),
      base::BindOnce(&SmbFsShare::OnSmbCredentialsDialogShowDone,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
  // Reset the allow dialog state to prevent showing another dialog to the user
  // immediately after they've dismissed one.
  allow_credential_request_ = false;
}

void SmbFsShare::OnSmbCredentialsDialogShowDone(
    RequestCredentialsCallback callback,
    bool canceled,
    const std::string& username,
    const std::string& password) {
  if (canceled) {
    std::move(callback).Run(true /* cancel */, "" /* username */,
                            "" /* workgroup */, "" /* password */);
    return;
  }

  std::string parsed_username = username;
  std::string workgroup;
  ParseUserName(username, &parsed_username, &workgroup);

  // Save updated credentials for future suspend/resume.
  options_.username = parsed_username;
  options_.workgroup = workgroup;
  options_.password = password;

  std::move(callback).Run(false /* cancel */, parsed_username, workgroup,
                          password);
}

void SmbFsShare::RemoveSavedCredentials(RemoveCredentialsCallback callback) {
  DCHECK(!remove_credentials_callback_);

  if (!host_) {
    std::move(callback).Run(false /* success */);
    return;
  }

  remove_credentials_callback_ = std::move(callback);
  host_->RemoveSavedCredentials(base::BindOnce(
      &SmbFsShare::OnRemoveSavedCredentialsDone, base::Unretained(this)));
}

void SmbFsShare::OnRemoveSavedCredentialsDone(bool success) {
  DCHECK(remove_credentials_callback_);
  std::move(remove_credentials_callback_).Run(success);
}

void SmbFsShare::SetMounterCreationCallbackForTest(
    MounterCreationCallback callback) {
  mounter_creation_callback_for_test_ = std::move(callback);
}

std::string SmbFsShare::GenerateStableMountId() const {
  std::string hash_input = GenerateStableMountIdInput();
  return base::ToLowerASCII(base::HexEncode(
      crypto::SHA256HashString(hash_input).c_str(), crypto::kSHA256Length));
}

std::string SmbFsShare::GenerateStableMountIdInput() const {
  std::vector<std::string> mount_id_hash_components;

  // Shares are unique based on the user the profile is owned by.
  mount_id_hash_components.push_back(
      ProfileHelper::Get()->GetUserIdHashFromProfile(profile_));

  // The hostname in the URL should be that entered by the user or
  // specified in the preconfigured share policy. It should not have
  // been resolved to an IP address if a hostname was specified.
  //
  // This property is true implicitly due to the call path for
  // SmbFsShare creation.
  mount_id_hash_components.push_back(share_url_.ToString());

  // Distinguish between Kerberos and user-supplied credentials.
  mount_id_hash_components.push_back(
      base::NumberToString(options_.kerberos_options ? 1 : 0));

  // Distinguish between domains / workgroups (may be empty for guest or
  // Kerberos).
  mount_id_hash_components.push_back(options_.workgroup);

  // Distinguish between usernames (may be empty for guest or Kerberos).
  mount_id_hash_components.push_back(options_.username);

  return base::JoinString(mount_id_hash_components, kMountIdHashSeparator);
}

}  // namespace ash::smb_client
