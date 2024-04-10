// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_DLC_PREDOWNLOAD_LIST_POLICY_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_DLC_PREDOWNLOAD_LIST_POLICY_HANDLER_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "third_party/protobuf/src/google/protobuf/repeated_ptr_field.h"

namespace policy {

// This class downloads DLCs set in the `DeviceDlcPredownloadList` policy. That
// includes predownloading DLCs as soon as OOBE and downloading missing DLCs
// when the policy is updated.
class DeviceDlcPredownloadListPolicyHandler {
 public:
  class OnInstallDlcHandler {
   public:
    virtual ~OnInstallDlcHandler() = default;

    virtual void OnInstallDlcComplete(
        const base::Value& dlc_id,
        const ash::DlcserviceClient::InstallResult& install_result) = 0;
  };

  ~DeviceDlcPredownloadListPolicyHandler();

  // This class is non-copyable and non-movable.
  DeviceDlcPredownloadListPolicyHandler(
      const DeviceDlcPredownloadListPolicyHandler& other) = delete;
  DeviceDlcPredownloadListPolicyHandler& operator=(
      const DeviceDlcPredownloadListPolicyHandler& other) = delete;
  DeviceDlcPredownloadListPolicyHandler(
      DeviceDlcPredownloadListPolicyHandler&& other) = delete;
  DeviceDlcPredownloadListPolicyHandler& operator=(
      DeviceDlcPredownloadListPolicyHandler&& other) = delete;

  // Decode a list of DLCs that should be pre downloaded to the device from
  // human-readable strings to DLC IDs. Any warning messages from the decoding
  // and schema validation process are stored in |warning|.
  static base::Value::List DecodeDeviceDlcPredownloadListPolicy(
      const google::protobuf::RepeatedPtrField<std::string>& raw_policy_value,
      std::string& out_warning);

  static std::unique_ptr<DeviceDlcPredownloadListPolicyHandler> Create();

 private:
  DeviceDlcPredownloadListPolicyHandler();

  // Download and install all the DLCs specified by the
  // kDeviceDlcPredownloadList CrosSetting.
  void TriggerPredownloadDlcs();

  raw_ptr<ash::CrosSettings> cros_settings_;
  std::unique_ptr<OnInstallDlcHandler> on_install_dlc_handler_;
  base::CallbackListSubscription dlc_predownloader_subscription_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_DLC_PREDOWNLOAD_LIST_POLICY_HANDLER_H_
