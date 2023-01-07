// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_DISK_QUOTA_ARC_DISK_QUOTA_BRIDGE_H_
#define ASH_COMPONENTS_ARC_DISK_QUOTA_ARC_DISK_QUOTA_BRIDGE_H_

#include "ash/components/arc/mojom/disk_quota.mojom.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/cryptohome/dbus-constants.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcBridgeService;

// This class proxies quota requests from Android to cryptohome.
class ArcDiskQuotaBridge : public KeyedService, public mojom::DiskQuotaHost {
 public:
  // Returns singleton instance for the given BrowserContext,
  // or nullptr if the browser |context| is not allowed to use ARC.
  static ArcDiskQuotaBridge* GetForBrowserContext(
      content::BrowserContext* context);

  ArcDiskQuotaBridge(content::BrowserContext* context,
                     ArcBridgeService* bridge_service);

  ArcDiskQuotaBridge(const ArcDiskQuotaBridge&) = delete;
  ArcDiskQuotaBridge& operator=(const ArcDiskQuotaBridge&) = delete;

  ~ArcDiskQuotaBridge() override;

  void SetAccountId(const AccountId& account_id);

  // mojom::DiskQuotaHost overrides:
  void IsQuotaSupported(IsQuotaSupportedCallback callback) override;

  void GetCurrentSpaceForUid(uint32_t uid,
                             GetCurrentSpaceForUidCallback callback) override;

  void GetCurrentSpaceForGid(uint32_t gid,
                             GetCurrentSpaceForGidCallback callback) override;

  void GetCurrentSpaceForProjectId(
      uint32_t project_id,
      GetCurrentSpaceForProjectIdCallback callback) override;

  void GetFreeDiskSpace(GetFreeDiskSpaceCallback) override;

 private:
  void OnGetFreeDiskSpace(GetFreeDiskSpaceCallback callback,
                          absl::optional<int64_t> reply);

  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  AccountId account_id_;

  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<ArcDiskQuotaBridge> weak_factory_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_DISK_QUOTA_ARC_DISK_QUOTA_BRIDGE_H_
