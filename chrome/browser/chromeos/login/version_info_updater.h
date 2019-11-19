// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_VERSION_INFO_UPDATER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_VERSION_INFO_UPDATER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"

namespace device {
class BluetoothAdapter;
}

namespace chromeos {

class CrosSettings;

// Fetches all info we want to show on OOBE/Login screens about system
// version, boot times and cloud policy.
class VersionInfoUpdater : public policy::CloudPolicyStore::Observer {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when OS version label should be updated.
    virtual void OnOSVersionLabelTextUpdated(
        const std::string& os_version_label_text) = 0;

    // Called when the enterprise info notice should be updated.
    virtual void OnEnterpriseInfoUpdated(const std::string& enterprise_info,
                                         const std::string& asset_id) = 0;

    // Called when the device info should be updated.
    virtual void OnDeviceInfoUpdated(const std::string& bluetooth_name) = 0;

    // Called when ADB sideloading status should be updated.
    virtual void OnAdbSideloadStatusUpdated(bool enabled) = 0;
  };

  explicit VersionInfoUpdater(Delegate* delegate);
  ~VersionInfoUpdater() override;

  // Sets delegate.
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  // Starts fetching version info. The delegate will be notified when update
  // is received.
  void StartUpdate(bool is_official_build);

  // Determine whether the system information will be displayed forcedly.
  base::Optional<bool> IsSystemInfoEnforced() const;

 private:
  // policy::CloudPolicyStore::Observer interface:
  void OnStoreLoaded(policy::CloudPolicyStore* store) override;
  void OnStoreError(policy::CloudPolicyStore* store) override;

  // Update the version label.
  void UpdateVersionLabel();

  // Check and update enterprise domain.
  void UpdateEnterpriseInfo();

  // Set enterprise domain name and device asset ID.
  void SetEnterpriseInfo(const std::string& enterprise_display_domain,
                         const std::string& asset_id);

  // Creates a serial number string.
  void UpdateSerialNumberInfo();

  // Callback from chromeos::VersionLoader giving the version.
  void OnVersion(const std::string& version);

  // Callback from device::BluetoothAdapterFactory::GetAdapter.
  void OnGetAdapter(scoped_refptr<device::BluetoothAdapter> adapter);

  // Callback from SessionManagerClient::QueryAdbSideload.
  void OnQueryAdbSideload(
      SessionManagerClient::AdbSideloadResponseCode response_code,
      bool enabled);

  // Information pieces for version label.
  std::string version_text_;
  std::string serial_number_text_;

  // Full text for the OS version label.
  std::string os_version_label_text_;

  std::vector<std::unique_ptr<CrosSettings::ObserverSubscription>>
      subscriptions_;

  chromeos::CrosSettings* cros_settings_;

  Delegate* delegate_;

  // Weak pointer factory so we can give our callbacks for invocation
  // at a later time without worrying that they will actually try to
  // happen after the lifetime of this object.
  base::WeakPtrFactory<VersionInfoUpdater> weak_pointer_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VersionInfoUpdater);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_VERSION_INFO_UPDATER_H_
