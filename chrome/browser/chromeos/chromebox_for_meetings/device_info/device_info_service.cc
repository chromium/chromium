// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/chromebox_for_meetings/device_info/device_info_service.h"

#include <cstdint>

#include "base/bind.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/common/channel_info.h"
#include "chromeos/dbus/chromebox_for_meetings/cfm_hotline_client.h"
#include "components/version_info/version_info.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {
namespace cfm {

namespace {
constexpr char kRootPartition[] = "/";
constexpr char kStatefulPartition[] = "/mnt/stateful_partition";
constexpr char kReleaseVersion[] = "CHROMEOS_RELEASE_VERSION";
constexpr char kReleasBuildType[] = "CHROMEOS_RELEASE_BUILD_TYPE";
constexpr char kReleaseTrack[] = "CHROMEOS_RELEASE_TRACK";
constexpr char kReleaseChromeMilestone[] = "CHROMEOS_RELEASE_CHROME_MILESTONE";

static DeviceInfoService* g_info_service = nullptr;

}  // namespace

// static
void DeviceInfoService::Initialize() {
  CHECK(!g_info_service);
  g_info_service = new DeviceInfoService();
}

// static
void DeviceInfoService::Shutdown() {
  CHECK(g_info_service);
  delete g_info_service;
  g_info_service = nullptr;
}

// static
DeviceInfoService* DeviceInfoService::Get() {
  CHECK(g_info_service)
      << "DeviceInfoService::Get() called before Initialize()";
  return g_info_service;
}

// static
bool DeviceInfoService::IsInitialized() {
  return g_info_service;
}

bool DeviceInfoService::ServiceRequestReceived(
    const std::string& interface_name) {
  if (interface_name != mojom::MeetDevicesInfo::Name_) {
    return false;
  }

  service_adaptor_.BindServiceAdaptor();
  return true;
}

void DeviceInfoService::OnBindService(
    mojo::ScopedMessagePipeHandle receiver_pipe) {
  receivers_.Add(this, mojo::PendingReceiver<mojom::MeetDevicesInfo>(
                           std::move(receiver_pipe)));
}

void DeviceInfoService::OnAdaptorConnect(bool success) {
  if (!success) {
    LOG(ERROR) << "mojom::DeviceInfo Service Adaptor connection failed.";
    return;
  }

  VLOG(3) << "mojom::DeviceInfo Service Adaptor is connected.";
  CHECK(ash::DeviceSettingsService::IsInitialized());
  ash::DeviceSettingsService::Get()->AddObserver(this);
}

void DeviceInfoService::OnAdaptorDisconnect() {
  LOG(WARNING) << "mojom::DeviceInfo Service Adaptor has been disconnected";
  Reset();
}

void DeviceInfoService::DeviceSettingsUpdated() {
  // Post to primary task runner
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&DeviceInfoService::UpdatePolicyInfo,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void DeviceInfoService::OnDeviceSettingsServiceShutdown() {
  // Post to primary task runner
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&DeviceInfoService::Reset,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void DeviceInfoService::AddDeviceSettingsObserver(
    ::mojo::PendingRemote<mojom::PolicyInfoObserver> observer) {
  mojo::Remote<mojom::PolicyInfoObserver> info_observer(std::move(observer));
  if (!current_policy_info_.is_null()) {
    info_observer->OnPolicyInfoChange(current_policy_info_->Clone());
  }

  policy_remotes_.Add(std::move(info_observer));
}

void DeviceInfoService::UpdatePolicyInfo() {
  auto* device_settings = ash::DeviceSettingsService::Get();
  if (!device_settings || !device_settings->policy_data()) {
    return;
  }
  auto* policy_data = device_settings->policy_data();
  auto policy_info = mojom::PolicyInfo::New();

  if (policy_data->has_timestamp()) {
    policy_info->timestamp_ms = policy_data->timestamp();
  }

  if (policy_data->has_device_id()) {
    policy_info->device_id = policy_data->device_id();
  }

  if (policy_data->has_service_account_identity()) {
    policy_info->service_account_email_address =
        policy_data->service_account_identity();
  }

  if (policy_data->has_gaia_id()) {
    base::StringToInt64(policy_data->gaia_id(),
                        &policy_info->service_account_gaia_id);
  }

  if (current_policy_info_.Equals(policy_info)) {
    return;
  }

  current_policy_info_ = std::move(policy_info);

  for (auto& remote : policy_remotes_) {
    remote->OnPolicyInfoChange(current_policy_info_->Clone());
  }
}

void DeviceInfoService::GetPolicyInfo(GetPolicyInfoCallback callback) {
  if (!current_policy_info_.is_null()) {
    std::move(callback).Run(current_policy_info_->Clone());
  } else {
    std::move(callback).Run(nullptr);
  }
}

void DeviceInfoService::GetSysInfo(GetSysInfoCallback callback) {
  auto root = base::FilePath(kRootPartition);
  auto stateful = base::FilePath(kStatefulPartition);

  auto sys_info = mojom::SysInfo::New();
  sys_info->kernel_version = base::SysInfo::KernelVersion();

  std::string value;
  if (base::SysInfo::GetLsbReleaseValue(kReleaseVersion, &value)) {
    sys_info->release_version = std::move(value);
  }
  if (base::SysInfo::GetLsbReleaseValue(kReleasBuildType, &value)) {
    sys_info->release_build_type = std::move(value);
  }
  if (base::SysInfo::GetLsbReleaseValue(kReleaseTrack, &value)) {
    sys_info->release_track = std::move(value);
  }
  if (base::SysInfo::GetLsbReleaseValue(kReleaseChromeMilestone, &value)) {
    sys_info->release_milestone = std::move(value);
  }

  sys_info->browser_version = version_info::GetVersionNumber();
  sys_info->channel_name = version_info::GetChannelString(chrome::GetChannel());

  std::move(callback).Run(std::move(sys_info));
}

// Private methods

DeviceInfoService::DeviceInfoService()
    : service_adaptor_(mojom::MeetDevicesInfo::Name_, this),
      task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  CfmHotlineClient::Get()->AddObserver(this);
  current_policy_info_.reset();
  // Device settings update may not be triggered in some cases
  DeviceSettingsUpdated();
}

DeviceInfoService::~DeviceInfoService() {
  CfmHotlineClient::Get()->RemoveObserver(this);
  Reset();
}

void DeviceInfoService::Reset() {
  receivers_.Clear();
  policy_remotes_.Clear();
  ash::DeviceSettingsService::Get()->RemoveObserver(this);
}

}  // namespace cfm
}  // namespace chromeos
