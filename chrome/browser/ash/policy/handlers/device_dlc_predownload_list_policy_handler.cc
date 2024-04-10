// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_dlc_predownload_list_policy_handler.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/ash/components/settings/cros_settings.h"

namespace policy {

namespace {

void OnInstallDlcComplete(
    const base::Value& dlc_id,
    const ash::DlcserviceClient::InstallResult& install_result) {
  if (install_result.error != dlcservice::kErrorNone) {
    LOG(ERROR) << "Failed to install DLC (" << dlc_id
               << "): " << install_result.error;
  }
}

}  // namespace

class OnInstallDlcHandlerImpl
    : public DeviceDlcPredownloadListPolicyHandler::OnInstallDlcHandler {
 public:
  OnInstallDlcHandlerImpl() = default;
  ~OnInstallDlcHandlerImpl() override = default;

  void OnInstallDlcComplete(
      const base::Value& dlc_id,
      const ash::DlcserviceClient::InstallResult& install_result) override {
    if (install_result.error != dlcservice::kErrorNone) {
      LOG(ERROR) << "Failed to install DLC (" << dlc_id
                 << "): " << install_result.error;
    }
  }
};

DeviceDlcPredownloadListPolicyHandler::
    ~DeviceDlcPredownloadListPolicyHandler() = default;

// static
base::Value::List
DeviceDlcPredownloadListPolicyHandler::DecodeDeviceDlcPredownloadListPolicy(
    const google::protobuf::RepeatedPtrField<std::string>& raw_policy_value,
    std::string& out_warning) {
  std::vector<std::string> unknown_dlcs;
  constexpr auto policy_value_to_dlc_id =
      base::MakeFixedFlatMap<std::string_view, std::string_view>(
          {{"scanner_drivers", "sane-backends-pfu"}});

  base::Value::List dlcs_to_predownload =
      base::Value::List::with_capacity(raw_policy_value.size());
  for (const auto& dlc_to_predownload : raw_policy_value) {
    if (!policy_value_to_dlc_id.contains(dlc_to_predownload)) {
      unknown_dlcs.push_back(dlc_to_predownload);
      continue;
    }
    std::string_view dlc_id = policy_value_to_dlc_id.at(dlc_to_predownload);
    if (!base::Contains(dlcs_to_predownload, dlc_id)) {
      // Silently ignore duplicate values.
      dlcs_to_predownload.Append(dlc_id);
    }
  }

  if (unknown_dlcs.empty()) {
    out_warning.clear();
  } else {
    out_warning =
        base::StrCat({"Unknown DLCs: ", base::JoinString(unknown_dlcs, ", ")});
  }

  return dlcs_to_predownload;
}

// static
std::unique_ptr<DeviceDlcPredownloadListPolicyHandler>
DeviceDlcPredownloadListPolicyHandler::Create() {
  return base::WrapUnique(new DeviceDlcPredownloadListPolicyHandler());
}

DeviceDlcPredownloadListPolicyHandler::DeviceDlcPredownloadListPolicyHandler()
    : cros_settings_(ash::CrosSettings::Get()) {
  dlc_predownloader_subscription_ = cros_settings_->AddSettingsObserver(
      ash::kDeviceDlcPredownloadList,
      base::BindRepeating(
          &DeviceDlcPredownloadListPolicyHandler::TriggerPredownloadDlcs,
          base::Unretained(this)));
  // Trigger predownload in case `CrosSettings` are already fetched to the
  // device.
  this->TriggerPredownloadDlcs();
}

void DeviceDlcPredownloadListPolicyHandler::TriggerPredownloadDlcs() {
  const base::Value::List* dlcs_to_predownload = nullptr;
  cros_settings_->GetList(ash::kDeviceDlcPredownloadList, &dlcs_to_predownload);

  if (!dlcs_to_predownload) {
    return;
  }

  for (const base::Value& dlc_id : *dlcs_to_predownload) {
    dlcservice::InstallRequest install_request;
    install_request.set_id(dlc_id.GetString());
    ash::DlcserviceClient::Get()->Install(
        install_request, base::BindOnce(OnInstallDlcComplete, dlc_id.Clone()),
        /*progress_callback=*/base::DoNothing());
  }
}

}  // namespace policy
