// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/disk_space/arc_disk_space_bridge.h"

#include <map>
#include <utility>
#include <vector>

#include "ash/components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ash/components/arc/arc_util.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/functional/bind.h"
#include "base/memory/singleton.h"
#include "chromeos/ash/components/dbus/spaced/spaced_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"

namespace arc {

namespace {

// Path to query disk space and disk quota for ARC.
constexpr char kArcDiskHome[] = "/home/chronos/user";

// Singleton factory for ArcDiskSpaceBridge.
class ArcDiskSpaceBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcDiskSpaceBridge,
          ArcDiskSpaceBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcDiskSpaceBridgeFactory";

  static ArcDiskSpaceBridgeFactory* GetInstance() {
    return base::Singleton<ArcDiskSpaceBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcDiskSpaceBridgeFactory>;
  ArcDiskSpaceBridgeFactory() = default;
  ~ArcDiskSpaceBridgeFactory() override = default;
};

bool IsAndroidUid(uint32_t uid) {
  const uint32_t android_uid_end = GetArcAndroidSdkVersionAsInt() < kArcVersionT
                                       ? kAndroidUidEndBeforeT
                                       : kAndroidUidEndAfterT;
  return kAndroidUidStart <= uid && uid <= android_uid_end;
}

bool IsAndroidGid(uint32_t gid) {
  return kAndroidGidStart <= gid && gid <= kAndroidGidEnd;
}

bool IsAndroidProjectId(uint32_t project_id) {
  const uint32_t project_id_for_android_apps_end =
      GetArcAndroidSdkVersionAsInt() < kArcVersionT
          ? kProjectIdForAndroidAppsEndBeforeT
          : kProjectIdForAndroidAppsEndAfterT;
  return (project_id >= kProjectIdForAndroidFilesStart &&
          project_id <= kProjectIdForAndroidFilesEnd) ||
         (project_id >= kProjectIdForAndroidAppsStart &&
          project_id <= project_id_for_android_apps_end);
}

void IsQuotaSupportedOnArcDiskHome(
    ArcDiskSpaceBridge::IsQuotaSupportedCallback callback) {
  ash::SpacedClient::Get()->IsQuotaSupported(
      kArcDiskHome,
      base::BindOnce(
          [](ArcDiskSpaceBridge::IsQuotaSupportedCallback callback,
             std::optional<bool> reply) {
            LOG_IF(ERROR, !reply.has_value())
                << "Failed to retrieve result from IsQuotaSupported";
            std::move(callback).Run(reply.value_or(false));
          },
          std::move(callback)));
}

bool ValidateIds(const std::vector<uint32_t>& ids,
                 base::RepeatingCallback<bool(uint32_t)> is_in_allowed_range,
                 std::string_view id_type) {
  for (const uint32_t id : ids) {
    if (!is_in_allowed_range.Run(id)) {
      LOG(ERROR) << "Android " << id_type << " " << id
                 << " is outside the allowed query range";
      return false;
    }
  }
  return true;
}

std::vector<uint32_t> GetVecWithShiftedIds(const std::vector<uint32_t>& ids,
                                           uint32_t shift_width) {
  std::vector<uint32_t> shifted_ids(ids.size());
  std::transform(ids.begin(), ids.end(), shifted_ids.begin(),
                 [&shift_width](const auto& id) { return id + shift_width; });
  return shifted_ids;
}

std::vector<int64_t> GetSpacesForIds(const std::map<uint32_t, int64_t>& map,
                                     const std::vector<uint32_t>& ids,
                                     std::string_view id_type) {
  std::vector<int64_t> spaces;
  for (uint32_t id : ids) {
    auto iter = map.find(id);
    if (iter == map.end()) {
      LOG(ERROR) << "Space for " << id_type << " " << id << " is not found in "
                 << "the map returned from spaced";
      // Return an empty list if the result for any ID is missing.
      return std::vector<int64_t>{};
    }
    spaces.push_back(iter->second);
  }
  return spaces;
}

}  // namespace

// static
ArcDiskSpaceBridge* ArcDiskSpaceBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcDiskSpaceBridgeFactory::GetForBrowserContext(context);
}

// static
ArcDiskSpaceBridge* ArcDiskSpaceBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcDiskSpaceBridgeFactory::GetForBrowserContextForTesting(context);
}

ArcDiskSpaceBridge::ArcDiskSpaceBridge(content::BrowserContext* context,
                                       ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service) {
  arc_bridge_service_->disk_space()->AddObserver(this);
  arc_bridge_service_->disk_space()->SetHost(this);
}

ArcDiskSpaceBridge::~ArcDiskSpaceBridge() {
  arc_bridge_service_->disk_space()->SetHost(nullptr);
  arc_bridge_service_->disk_space()->RemoveObserver(this);
}

void ArcDiskSpaceBridge::IsQuotaSupported(IsQuotaSupportedCallback callback) {
  // Whether ARC quota is supported is an AND of the following two booleans:
  // * Whether there are no unmounted Android users (from cryptohome)
  // * Whether |kArcDiskHome| is mounted with quota enabled (from spaced)
  // Query cryptohome first, as the first one is more likely to be false.
  ash::UserDataAuthClient::Get()->GetArcDiskFeatures(
      user_data_auth::GetArcDiskFeaturesRequest(),
      base::BindOnce(
          [](IsQuotaSupportedCallback callback,
             std::optional<user_data_auth::GetArcDiskFeaturesReply> reply) {
            LOG_IF(ERROR, !reply.has_value())
                << "Failed to retrieve result from GetArcDiskFeatures call.";
            if (!reply.has_value() || !reply->quota_supported()) {
              std::move(callback).Run(false);
              return;
            }
            IsQuotaSupportedOnArcDiskHome(std::move(callback));
          },
          std::move(callback)));
}

void ArcDiskSpaceBridge::GetQuotaCurrentSpaceForUid(
    uint32_t android_uid,
    GetQuotaCurrentSpaceForUidCallback callback) {
  if (!IsAndroidUid(android_uid)) {
    LOG(ERROR) << "Android uid " << android_uid
               << " is outside the allowed query range";
    std::move(callback).Run(-1);
    return;
  }

  const uint32_t cros_uid = android_uid + kArcUidShift;
  ash::SpacedClient::Get()->GetQuotaCurrentSpaceForUid(
      kArcDiskHome, cros_uid,
      base::BindOnce(
          [](GetQuotaCurrentSpaceForUidCallback callback, int cros_uid,
             std::optional<int64_t> reply) {
            LOG_IF(ERROR, !reply.has_value())
                << "Failed to retrieve result from GetQuotaCurrentSpaceForUid "
                << "for uid=" << cros_uid;
            std::move(callback).Run(reply.value_or(-1));
          },
          std::move(callback), cros_uid));
}

void ArcDiskSpaceBridge::GetQuotaCurrentSpaceForGid(
    uint32_t android_gid,
    GetQuotaCurrentSpaceForGidCallback callback) {
  if (!IsAndroidGid(android_gid)) {
    LOG(ERROR) << "Android gid " << android_gid
               << " is outside the allowed query range";
    std::move(callback).Run(-1);
    return;
  }

  const uint32_t cros_gid = android_gid + kArcGidShift;
  ash::SpacedClient::Get()->GetQuotaCurrentSpaceForGid(
      kArcDiskHome, cros_gid,
      base::BindOnce(
          [](GetQuotaCurrentSpaceForGidCallback callback, int cros_gid,
             std::optional<int64_t> reply) {
            LOG_IF(ERROR, !reply.has_value())
                << "Failed to retrieve result from GetQuotaCurrentSpaceForGid "
                << "for gid=" << cros_gid;
            std::move(callback).Run(reply.value_or(-1));
          },
          std::move(callback), cros_gid));
}

void ArcDiskSpaceBridge::GetQuotaCurrentSpaceForProjectId(
    uint32_t project_id,
    GetQuotaCurrentSpaceForProjectIdCallback callback) {
  if (!IsAndroidProjectId(project_id)) {
    LOG(ERROR) << "Android project id " << project_id
               << " is outside the allowed query range";
    std::move(callback).Run(-1);
    return;
  }
  ash::SpacedClient::Get()->GetQuotaCurrentSpaceForProjectId(
      kArcDiskHome, project_id,
      base::BindOnce(
          [](GetQuotaCurrentSpaceForProjectIdCallback callback, int project_id,
             std::optional<int64_t> reply) {
            LOG_IF(ERROR, !reply.has_value())
                << "Failed to retrieve result from "
                   "GetQuotaCurrentSpaceForProjectId for project_id="
                << project_id;
            std::move(callback).Run(reply.value_or(-1));
          },
          std::move(callback), project_id));
}

void ArcDiskSpaceBridge::GetQuotaCurrentSpacesForIds(
    const std::vector<uint32_t>& android_uids,
    const std::vector<uint32_t>& android_gids,
    const std::vector<uint32_t>& android_project_ids,
    GetQuotaCurrentSpacesForIdsCallback callback) {
  if (!ValidateIds(android_uids, base::BindRepeating(&IsAndroidUid), "UID") ||
      !ValidateIds(android_gids, base::BindRepeating(&IsAndroidGid), "GID") ||
      !ValidateIds(android_project_ids,
                   base::BindRepeating(&IsAndroidProjectId), "project ID")) {
    std::move(callback).Run(nullptr);
    return;
  }
  const std::vector<uint32_t> cros_uids =
      GetVecWithShiftedIds(android_uids, kArcUidShift);
  const std::vector<uint32_t> cros_gids =
      GetVecWithShiftedIds(android_gids, kArcGidShift);
  ash::SpacedClient::Get()->GetQuotaCurrentSpacesForIds(
      kArcDiskHome, cros_uids, cros_gids, android_project_ids,
      base::BindOnce(
          [](GetQuotaCurrentSpacesForIdsCallback callback,
             const std::vector<uint32_t>& cros_uids,
             const std::vector<uint32_t>& cros_gids,
             const std::vector<uint32_t>& project_ids,
             std::optional<ash::SpacedClient::SpaceMaps> result) {
            if (!result.has_value()) {
              LOG(ERROR) << "SpacedClient::GetQuotaCurrentSpacesForIds failed";
              std::move(callback).Run(nullptr);
              return;
            }
            mojom::QuotaSpacesPtr quota_spaces = mojom::QuotaSpaces::New();
            quota_spaces->curspaces_for_uids =
                GetSpacesForIds(result->curspaces_for_uids, cros_uids, "UID");
            quota_spaces->curspaces_for_gids =
                GetSpacesForIds(result->curspaces_for_gids, cros_gids, "GID");
            quota_spaces->curspaces_for_project_ids = GetSpacesForIds(
                result->curspaces_for_project_ids, project_ids, "project ID");
            std::move(callback).Run(std::move(quota_spaces));
          },
          std::move(callback), cros_uids, cros_gids, android_project_ids));
}

void ArcDiskSpaceBridge::GetFreeDiskSpace(GetFreeDiskSpaceCallback callback) {
  ash::SpacedClient::Get()->GetFreeDiskSpace(
      kArcDiskHome,
      base::BindOnce(
          [](GetFreeDiskSpaceCallback callback, std::optional<int64_t> reply) {
            if (!reply.has_value()) {
              LOG(ERROR) << "spaced::GetFreeDiskSpace failed";
              std::move(callback).Run(nullptr);
              return;
            }

            mojom::DiskSpacePtr disk_space = mojom::DiskSpace::New();
            disk_space->space_in_bytes = reply.value();
            std::move(callback).Run(std::move(disk_space));
          },
          std::move(callback)));
}

bool ArcDiskSpaceBridge::GetApplicationsSize(
    GetApplicationsSizeCallback callback) {
  auto* disk_space_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->disk_space(), GetApplicationsSize);
  if (!disk_space_instance) {
    return false;
  }
  disk_space_instance->GetApplicationsSize(std::move(callback));
  return true;
}

void ArcDiskSpaceBridge::OnConnectionReady() {
  ash::SpacedClient::Get()->AddObserver(this);
  ash::SpacedClient::Get()->GetFreeDiskSpace(
      kArcDiskHome, base::BindOnce(&ArcDiskSpaceBridge::OnGetFreeDiskSpace,
                                   weak_factory_.GetWeakPtr()));
}

void ArcDiskSpaceBridge::OnConnectionClosed() {
  ash::SpacedClient::Get()->RemoveObserver(this);
}

void ArcDiskSpaceBridge::OnGetFreeDiskSpace(std::optional<int64_t> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "Failed to call GetFreeDiskSpace from ArcDiskSpaceBridge";
    return;
  }
  SendResizeStorageBalloon(reply.value());
}

void ArcDiskSpaceBridge::OnSpaceUpdate(const SpaceEvent& event) {
  SendResizeStorageBalloon(event.free_space_bytes());
}

void ArcDiskSpaceBridge::SendResizeStorageBalloon(int64_t free_space_bytes) {
  auto* disk_space_instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_bridge_service_->disk_space(), ResizeStorageBalloon);
  if (!disk_space_instance) {
    return;
  }
  disk_space_instance->ResizeStorageBalloon(std::max(
      int64_t(free_space_bytes - kStorageBalloonFreeSpaceBufferSizeInBytes),
      int64_t(0)));
}

// static
void ArcDiskSpaceBridge::EnsureFactoryBuilt() {
  ArcDiskSpaceBridgeFactory::GetInstance();
}

}  // namespace arc
