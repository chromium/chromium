// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/disk_quota/arc_disk_quota_bridge.h"

#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/cryptohome/cryptohome_parameters.h"
#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/dbus/userdataauth/arc_quota_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace arc {

namespace {

// Singleton factory for ArcDiskQuotaBridge.
class ArcDiskQuotaBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcDiskQuotaBridge,
          ArcDiskQuotaBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcDiskQuotaBridgeFactory";

  static ArcDiskQuotaBridgeFactory* GetInstance() {
    return base::Singleton<ArcDiskQuotaBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcDiskQuotaBridgeFactory>;
  ArcDiskQuotaBridgeFactory() = default;
  ~ArcDiskQuotaBridgeFactory() override = default;
};

constexpr char kAndroidDownloadPath[] = "/storage/emulated/0/Download/";
constexpr char kAndroidExternalStoragePath[] = "/storage/emulated/0/";
constexpr char kAndroidDataMediaPath[] = "/data/media/0/";

// Size of the /data disk should be a multiple of this value.
// TODO(b/217650747): Finalize the value (using a tentative value for now).
constexpr int64_t kDiskSizeTickBytes = 1LL << 30;  // 1GB

// Disk expansion should not be executed if host's projected free disk space
// after resizing is smaller than this.
// TODO(b/217650747): Finalize the value.
// (Current value is taken from cryptohome::kTargetFreeSpaceAfterCleanup)
constexpr int64_t kMinHostFreeSpaceBytes = 2LL << 30;  // 2GB

}  // namespace

// static
bool ArcDiskQuotaBridge::convertPathForSetProjectId(
    const base::FilePath& android_path,
    user_data_auth::SetProjectIdAllowedPathType* parent_path_out,
    base::FilePath* child_path_out) {
  const base::FilePath kDownloadPath(kAndroidDownloadPath);
  const base::FilePath kExternalStoragePath(kAndroidExternalStoragePath);
  const base::FilePath kDataMediaPath(kAndroidDataMediaPath);

  if (android_path.ReferencesParent()) {
    LOG(ERROR) << "Path contains \"..\" : " << android_path.value();
    return false;
  }

  *child_path_out = base::FilePath();
  if (kDownloadPath.IsParent(android_path)) {
    // /storage/emulated/0/Download/* =>
    //     parent=/home/user/<hash>/MyFiles/Downloads/, child=*
    *parent_path_out =
        user_data_auth::SetProjectIdAllowedPathType::PATH_DOWNLOADS;
    return kDownloadPath.AppendRelativePath(android_path, child_path_out);
  } else if (kExternalStoragePath.IsParent(android_path)) {
    // /storage/emulated/0/* =>
    //     parent=/home/root/<hash>/android-data/, child=data/media/0/*
    *parent_path_out =
        user_data_auth::SetProjectIdAllowedPathType::PATH_ANDROID_DATA;
    // child_path should be relative to the root.
    return base::FilePath("/").AppendRelativePath(kDataMediaPath,
                                                  child_path_out) &&
           kExternalStoragePath.AppendRelativePath(android_path,
                                                   child_path_out);
  } else if (kDataMediaPath.IsParent(android_path)) {
    // /data/media/0/* =>
    //     parent=/home/root/<hash>/android-data/, child=data/media/0/*
    *parent_path_out =
        user_data_auth::SetProjectIdAllowedPathType::PATH_ANDROID_DATA;
    // child_path should be relative to the root.
    return base::FilePath("/").AppendRelativePath(android_path, child_path_out);
  } else {
    return false;
  }
}

// static
ArcDiskQuotaBridge* ArcDiskQuotaBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcDiskQuotaBridgeFactory::GetForBrowserContext(context);
}

ArcDiskQuotaBridge::ArcDiskQuotaBridge(content::BrowserContext* context,
                                       ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->disk_quota()->SetHost(this);
}

ArcDiskQuotaBridge::~ArcDiskQuotaBridge() {
  arc_bridge_service_->disk_quota()->SetHost(nullptr);
}

void ArcDiskQuotaBridge::SetUserInfo(const AccountId& account_id,
                                     const std::string& user_id_hash) {
  account_id_ = account_id;
  user_id_hash_ = user_id_hash;
}

void ArcDiskQuotaBridge::IsQuotaSupported(IsQuotaSupportedCallback callback) {
  chromeos::ArcQuotaClient::Get()->GetArcDiskFeatures(
      user_data_auth::GetArcDiskFeaturesRequest(),
      base::BindOnce(
          [](IsQuotaSupportedCallback callback,
             absl::optional<user_data_auth::GetArcDiskFeaturesReply> reply) {
            LOG_IF(ERROR, !reply.has_value())
                << "Failed to retrieve result from IsQuotaSupported call.";
            bool result = false;
            if (reply.has_value()) {
              result = reply->quota_supported();
            }
            std::move(callback).Run(result);
          },
          std::move(callback)));
}

void ArcDiskQuotaBridge::GetCurrentSpaceForUid(
    uint32_t uid,
    GetCurrentSpaceForUidCallback callback) {
  user_data_auth::GetCurrentSpaceForArcUidRequest request;
  request.set_uid(uid);
  chromeos::ArcQuotaClient::Get()->GetCurrentSpaceForArcUid(
      request,
      base::BindOnce(
          [](GetCurrentSpaceForUidCallback callback, int uid,
             absl::optional<user_data_auth::GetCurrentSpaceForArcUidReply>
                 reply) {
            LOG_IF(ERROR, !reply.has_value())
                << "Failed to retrieve result from "
                   "GetCurrentSpaceForUid for android uid="
                << uid;
            int64_t result = -1LL;
            if (reply.has_value()) {
              result = reply->cur_space();
            }
            std::move(callback).Run(result);
          },
          std::move(callback), uid));
}

void ArcDiskQuotaBridge::GetCurrentSpaceForGid(
    uint32_t gid,
    GetCurrentSpaceForGidCallback callback) {
  user_data_auth::GetCurrentSpaceForArcGidRequest request;
  request.set_gid(gid);
  chromeos::ArcQuotaClient::Get()->GetCurrentSpaceForArcGid(
      request,
      base::BindOnce(
          [](GetCurrentSpaceForGidCallback callback, int gid,
             absl::optional<user_data_auth::GetCurrentSpaceForArcGidReply>
                 reply) {
            LOG_IF(ERROR, !reply.has_value())
                << "Failed to retrieve result from "
                   "GetCurrentSpaceForGid for android gid="
                << gid;
            int result = -1LL;
            if (reply.has_value()) {
              result = reply->cur_space();
            }
            std::move(callback).Run(result);
          },
          std::move(callback), gid));
}

void ArcDiskQuotaBridge::GetCurrentSpaceForProjectId(
    uint32_t project_id,
    GetCurrentSpaceForProjectIdCallback callback) {
  user_data_auth::GetCurrentSpaceForArcProjectIdRequest request;
  request.set_project_id(project_id);
  chromeos::ArcQuotaClient::Get()->GetCurrentSpaceForArcProjectId(
      request,
      base::BindOnce(
          [](GetCurrentSpaceForProjectIdCallback callback, int project_id,
             absl::optional<user_data_auth::GetCurrentSpaceForArcProjectIdReply>
                 reply) {
            LOG_IF(ERROR, !reply.has_value())
                << "Failed to retrieve result from "
                   "GetCurrentSpaceForProjectId for project_id="
                << project_id;
            int result = -1LL;
            if (reply.has_value()) {
              result = reply->cur_space();
            }
            std::move(callback).Run(result);
          },
          std::move(callback), project_id));
}

void ArcDiskQuotaBridge::SetProjectId(uint32_t project_id,
                                      const std::string& android_path,
                                      SetProjectIdCallback callback) {
  user_data_auth::SetProjectIdAllowedPathType parent_path;
  base::FilePath child_path;
  if (!convertPathForSetProjectId(base::FilePath(android_path), &parent_path,
                                  &child_path)) {
    LOG(ERROR) << "Setting a project ID to path " << android_path
               << " is not allowed";
    std::move(callback).Run(false);
    return;
  }

  user_data_auth::SetProjectIdRequest request;
  request.set_project_id(project_id);
  request.set_parent_path(parent_path);
  request.set_child_path(child_path.value());
  *request.mutable_account_id() =
      cryptohome::CreateAccountIdentifierFromAccountId(account_id_);
  chromeos::ArcQuotaClient::Get()->SetProjectId(
      request,
      base::BindOnce(
          [](SetProjectIdCallback callback, const int project_id,
             const user_data_auth::SetProjectIdAllowedPathType parent_path,
             const std::string& child_path,
             absl::optional<user_data_auth::SetProjectIdReply> reply) {
            LOG_IF(ERROR, !reply.has_value())
                << "Failed to set project ID " << project_id
                << " to parent_path=" << parent_path
                << " child_path=" << child_path;
            bool result = false;
            if (reply.has_value()) {
              result = reply->success();
            }
            std::move(callback).Run(result);
          },
          std::move(callback), project_id, parent_path, child_path.value()));
}

void ArcDiskQuotaBridge::RequestDataDiskExpansion(
    int64_t total_space,
    int64_t free_space,
    RequestDataDiskExpansionCallback callback) {
  // Gets free disk space under /home in the host.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace,
                     base::FilePath("/home")),
      base::BindOnce(&ArcDiskQuotaBridge::OnHostFreeSpace,
                     weak_factory_.GetWeakPtr(), total_space, free_space,
                     std::move(callback)));
}

void ArcDiskQuotaBridge::OnHostFreeSpace(
    int64_t guest_total_space,
    int64_t guest_free_space,
    RequestDataDiskExpansionCallback callback,
    int64_t host_free_space) {
  // Calculate new_disk_size.
  // - new_disk_size should be a multiple of kDiskSizeTickBytes (1G).
  // - projected_host_free_space should be larger than kMinHostFreeSpaceBytes.
  // - Projected guest free space (new_disk_size - guest_used_space)
  //   should be larger than kDiskSizeTickBytes.
  // For example, if guest_used_space is 4.3G, new_disk_size will be 6G.
  // (Projected guest free space = 1.7G > kDiskSizeTickBytes)
  const int64_t guest_used_space = guest_total_space - guest_free_space;
  const int64_t new_disk_size =
      kDiskSizeTickBytes * ((guest_used_space / kDiskSizeTickBytes) + 2);

  const int64_t projected_host_free_space =
      host_free_space - (new_disk_size - guest_total_space);
  if (projected_host_free_space < kMinHostFreeSpaceBytes) {
    LOG(WARNING) << "RequestDataDiskExpansion rejected because there is not "
                 << "enough free disk space in the host. host_free_space="
                 << host_free_space << " projected_host_free_space="
                 << projected_host_free_space;
    std::move(callback).Run(-1LL);
    return;
  }

  vm_tools::concierge::ResizeDiskImageRequest resize_request;
  resize_request.set_cryptohome_id(user_id_hash_);
  resize_request.set_vm_name(kArcVmName);
  resize_request.set_disk_size(new_disk_size);

  VLOG(1) << "Sending ResizeDiskImageRequest:"
          << " host_free_space=" << host_free_space
          << " guest_used_space=" << guest_used_space
          << " current_disk_size=" << guest_total_space
          << " new_disk_size=" << new_disk_size;

  ash::ConciergeClient::Get()->ResizeDiskImage(
      resize_request, base::BindOnce(&ArcDiskQuotaBridge::OnResizeDiskResponse,
                                     weak_factory_.GetWeakPtr(), new_disk_size,
                                     std::move(callback)));
}

void ArcDiskQuotaBridge::OnResizeDiskResponse(
    int64_t new_disk_size,
    RequestDataDiskExpansionCallback callback,
    absl::optional<vm_tools::concierge::ResizeDiskImageResponse> response) {
  if (!response) {
    LOG(ERROR) << "OnResizeDiskResponse: Got null response";
    std::move(callback).Run(-1LL);
    return;
  }

  if (response->status() !=
      vm_tools::concierge::DiskImageStatus::DISK_STATUS_RESIZED) {
    LOG(ERROR) << "OnResizeDiskResponse: ResizeDiskImageRequest failed:"
               << " status=" << response->status()
               << " failure_reason=" << response->failure_reason();
    std::move(callback).Run(-1LL);
    return;
  }

  std::move(callback).Run(new_disk_size);
}

}  // namespace arc
