// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/disk_quota/arc_disk_quota_bridge.h"

#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/cryptohome/cryptohome_parameters.h"
#include "base/bind.h"
#include "base/memory/singleton.h"
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
    //     parent=/home/user/<hash>/Downloads/, child=*
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

void ArcDiskQuotaBridge::SetAccountId(const AccountId& account_id) {
  account_id_ = account_id;
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

}  // namespace arc
