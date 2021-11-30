// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_DISK_QUOTA_ARC_DISK_QUOTA_BRIDGE_H_
#define ASH_COMPONENTS_ARC_DISK_QUOTA_ARC_DISK_QUOTA_BRIDGE_H_

#include "ash/components/arc/mojom/disk_quota.mojom.h"
#include "base/files/file_path.h"
#include "chromeos/dbus/cryptohome/UserDataAuth.pb.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/core/keyed_service.h"
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

  // Converts an Android path to a pair of (parent_path, child_path) to be
  // passed to SetProjectId() on cryptohome.
  // Returns false if SetProjectId() is not allowed for the path.
  // (go/arc-project-quota)
  static bool convertPathForSetProjectId(
      const base::FilePath& android_path,
      user_data_auth::SetProjectIdAllowedPathType* parent_path_out,
      base::FilePath* child_path_out);

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

  void SetProjectId(uint32_t project_id,
                    const std::string& android_path,
                    SetProjectIdCallback callback) override;

 private:
  ArcBridgeService* const arc_bridge_service_;  // Owned by ArcServiceManager.

  AccountId account_id_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_DISK_QUOTA_ARC_DISK_QUOTA_BRIDGE_H_
