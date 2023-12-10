// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/disk_quota/arc_disk_quota_bridge.h"

#include <utility>

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
    ArcDiskQuotaBridge::IsQuotaSupportedCallback callback) {
  ash::SpacedClient::Get()->IsQuotaSupported(
      kArcDiskHome,
      base::BindOnce(
          [](ArcDiskQuotaBridge::IsQuotaSupportedCallback callback,
             std::optional<bool> reply) {
            LOG_IF(ERROR, !reply.has_value())
                << "Failed to retrieve result from IsQuotaSupported";
            std::move(callback).Run(reply.value_or(false));
          },
          std::move(callback)));
}

}  // namespace

// static
ArcDiskQuotaBridge* ArcDiskQuotaBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcDiskQuotaBridgeFactory::GetForBrowserContext(context);
}

// static
ArcDiskQuotaBridge* ArcDiskQuotaBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcDiskQuotaBridgeFactory::GetForBrowserContextForTesting(context);
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

void ArcDiskQuotaBridge::GetCurrentSpaceForUid(
    uint32_t android_uid,
    GetCurrentSpaceForUidCallback callback) {
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
          [](GetCurrentSpaceForUidCallback callback, int cros_uid,
             std::optional<int64_t> reply) {
            LOG_IF(ERROR, !reply.has_value())
                << "Failed to retrieve result from GetQuotaCurrentSpaceForUid "
                << "for uid=" << cros_uid;
            std::move(callback).Run(reply.value_or(-1));
          },
          std::move(callback), cros_uid));
}

void ArcDiskQuotaBridge::GetCurrentSpaceForGid(
    uint32_t android_gid,
    GetCurrentSpaceForGidCallback callback) {
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
          [](GetCurrentSpaceForGidCallback callback, int cros_gid,
             std::optional<int64_t> reply) {
            LOG_IF(ERROR, !reply.has_value())
                << "Failed to retrieve result from GetQuotaCurrentSpaceForGid "
                << "for gid=" << cros_gid;
            std::move(callback).Run(reply.value_or(-1));
          },
          std::move(callback), cros_gid));
}

void ArcDiskQuotaBridge::GetCurrentSpaceForProjectId(
    uint32_t project_id,
    GetCurrentSpaceForProjectIdCallback callback) {
  if (!IsAndroidProjectId(project_id)) {
    LOG(ERROR) << "Android project id " << project_id
               << " is outside the allowed query range";
    std::move(callback).Run(-1);
    return;
  }
  ash::SpacedClient::Get()->GetQuotaCurrentSpaceForProjectId(
      kArcDiskHome, project_id,
      base::BindOnce(
          [](GetCurrentSpaceForProjectIdCallback callback, int project_id,
             std::optional<int64_t> reply) {
            LOG_IF(ERROR, !reply.has_value())
                << "Failed to retrieve result from "
                   "GetQuotaCurrentSpaceForProjectId for project_id="
                << project_id;
            std::move(callback).Run(reply.value_or(-1));
          },
          std::move(callback), project_id));
}

void ArcDiskQuotaBridge::GetFreeDiskSpace(GetFreeDiskSpaceCallback callback) {
  ash::SpacedClient::Get()->GetFreeDiskSpace(
      kArcDiskHome,
      base::BindOnce(&ArcDiskQuotaBridge::OnGetFreeDiskSpace,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcDiskQuotaBridge::OnGetFreeDiskSpace(GetFreeDiskSpaceCallback callback,
                                            std::optional<int64_t> reply) {
  if (!reply.has_value()) {
    LOG(ERROR) << "spaced::GetFreeDiskSpace failed";
    std::move(callback).Run(nullptr);
    return;
  }

  mojom::DiskSpacePtr disk_space = mojom::DiskSpace::New();
  disk_space->space_in_bytes = reply.value();
  std::move(callback).Run(std::move(disk_space));
}

// static
void ArcDiskQuotaBridge::EnsureFactoryBuilt() {
  ArcDiskQuotaBridgeFactory::GetInstance();
}

}  // namespace arc
