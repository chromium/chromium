// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/disk_quota/arc_disk_quota_bridge.h"

#include <utility>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/functional/bind.h"
#include "base/memory/singleton.h"
#include "chromeos/ash/components/dbus/spaced/spaced_client.h"
#include "chromeos/ash/components/dbus/userdataauth/arc_quota_client.h"

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

}  // namespace

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
  ash::ArcQuotaClient::Get()->GetArcDiskFeatures(
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
  ash::ArcQuotaClient::Get()->GetCurrentSpaceForArcUid(
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
  ash::ArcQuotaClient::Get()->GetCurrentSpaceForArcGid(
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
  ash::ArcQuotaClient::Get()->GetCurrentSpaceForArcProjectId(
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

void ArcDiskQuotaBridge::GetFreeDiskSpace(GetFreeDiskSpaceCallback callback) {
  ash::SpacedClient::Get()->GetFreeDiskSpace(
      "/home/chronos/user",
      base::BindOnce(&ArcDiskQuotaBridge::OnGetFreeDiskSpace,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcDiskQuotaBridge::OnGetFreeDiskSpace(GetFreeDiskSpaceCallback callback,
                                            absl::optional<int64_t> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "spaced::GetFreeDiskSpace failed";
    std::move(callback).Run(nullptr);
    return;
  }

  mojom::DiskSpacePtr disk_space = mojom::DiskSpace::New();
  disk_space->space_in_bytes = reply.value();
  std::move(callback).Run(std::move(disk_space));
}

}  // namespace arc
